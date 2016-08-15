#include <car.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define ERROR(...) snprintf(car_error, 256, __VA_ARGS__)

static int fd = -1;
struct v4l2_buffer buf;
char car_error[256];

static int xioctl(int fd, int request, void *arg)
{
	int r;

	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);
	return r;
}

const char* car_camera_init(car_camera_t* camera, uint16_t id,
	uint16_t w, uint16_t h, void** output)
{
	// Open the device
	fd = open("/dev/video0", O_RDWR | O_NONBLOCK, 0);
	if (fd == -1) {
		// couldn't find capture device
		ERROR("Opening Video device\n");
		return car_error;
	}

	// Is it available?
	struct v4l2_capability caps;
	if (xioctl(fd, VIDIOC_QUERYCAP, &caps) == -1) {
		ERROR("Querying Capabilites\n");
		return car_error;
	}

	// Set image format.
	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = w;
	fmt.fmt.pix.height = h;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;

	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)) {
		ERROR("Error setting Pixel Format\n");
		return car_error;
	}

	// Request a video capture buffer.
	struct v4l2_requestbuffers req;
	CLEAR(req);
	req.count = 1;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	 
	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
	{
		ERROR("Requesting Buffer\n");
		return car_error;
	}

	// Query buffer
	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
	if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) {
		ERROR("Querying Buffer\n");
		return car_error;
	}
	*output = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
		fd, buf.m.offset);
	camera->size = buf.length;

	// Start the capture:
	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;

	if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
		ERROR("VIDIOC_QBUF");
		return car_error;
	}

	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
		ERROR("VIDIOC_STREAMON");
		return car_error;
	}
	return NULL;
}

const char* car_camera_loop(car_camera_t* camera) {
CAR_CAMERA_LOOP:;
	struct timeval tv;

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	/* Timeout. */
	tv.tv_sec = 2;
	tv.tv_usec = 0;

//	printf("point 3\n");
	int r = select(fd+1, &fds, NULL, NULL, &tv);
//	printf("point 4\n");
	if(r == -1) {
		if (EINTR == errno) goto CAR_CAMERA_LOOP;
		ERROR("Waiting for Frame\n");
		return car_error;
	}
//	printf("point 5\n");
	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if(xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
		if(errno == EAGAIN) goto CAR_CAMERA_LOOP;
		ERROR("Retrieving Frame %s\n", strerror(errno));
		close(fd);
		return car_error;
	}
//	printf("point 6\n");

	if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
		ERROR("VIDIOC_QBUF");
		return car_error;
	}
	return NULL;
}

const char* car_camera_kill(car_camera_t* camera) {
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
		ERROR("VIDIOC_STREAMOFF");
		return car_error;
	}
	if (munmap(camera->buffer, camera->size) == -1) {
		ERROR("munmap");
		return car_error;
	}
	if (close(fd) == -1) {
		ERROR("close");
		return car_error;
	}
	return NULL;
}
