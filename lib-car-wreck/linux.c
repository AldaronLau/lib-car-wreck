/*
 *  Lib Car Wreck!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>	      /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include "car.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define ERROR(...) snprintf(car_error, 256, __VA_ARGS__)

struct buffer {
	void   *start;
	size_t  length;
};

static int fd = -1;
struct buffer *buffers;
static unsigned int n_buffers;
static char car_error[256]; 

static int xioctl(int fh, int request, void *arg)
{
	int r;

	do {
		r = ioctl(fh, request, arg);
	} while (r == -1 && EINTR == errno);

	return r;
}

static inline uint8_t read_frame(void* output) {
	struct v4l2_buffer buf;

	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
		if(errno == EAGAIN) {
			return 0;
		}else{
			ERROR("VIDIOC_DQBUF");
			return 2;
		}
	}

	if(buf.index > n_buffers) {
		ERROR("Fail!");
		return 2;
	}

	memcpy(output, buffers[buf.index].start, buf.bytesused);

	if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
		ERROR("VIDIOC_QBUF");
		return 2;
	}

	return 1;
}

static uint8_t stop_capturing(void) {
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
		ERROR("VIDIOC_STREAMOFF");
		return 1;
	}
	return 0;
}

static inline uint8_t start_capturing(void) {
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
			ERROR("VIDIOC_QBUF");
			return 1;
		}
	}
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
		ERROR("VIDIOC_STREAMON");
		return 1;
	}
	return 0;
}

static uint8_t uninit_device(void) {
	unsigned int i;

	for (i = 0; i < n_buffers; ++i) {
		if (munmap(buffers[i].start, buffers[i].length) == -1) {
			ERROR("munmap");
			return 1;
		}
	}
	free(buffers);
	return 0;
}

static inline uint8_t init_mmap(const char* dev_name) {
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
		if (EINVAL == errno) {
			ERROR("%s does not support memory mapping\n", dev_name);
			return 1;
		} else {
			ERROR("VIDIOC_REQBUFS");
			return 1;
		}
	}

	if (req.count < 2) {
		ERROR("Insufficient buffer memory on %s\n", dev_name);
		return 1;
	}

	buffers = calloc(req.count, sizeof(*buffers));

	if (!buffers) {
		ERROR("Out of memory\n");
		return 1;
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;

		if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
			ERROR("VIDIOC_QUERYBUF");
			return 1;
		}

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start =
			mmap(NULL /* start anywhere */,
			      buf.length,
			      PROT_READ | PROT_WRITE /* required */,
			      MAP_SHARED /* recommended */,
			      fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start) {
			ERROR("mmap");
			return 1;
		}
	}
	return 0;
}

static uint8_t init_device(const char* dev_name, uint16_t w, uint16_t h) {
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
		if (EINVAL == errno)
			ERROR("%s is not a V4L2 device.\n", dev_name);
		else
			ERROR("VIDIOC_QUERYCAP");
		return 1;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		ERROR("%s is not a video capture device.\n", dev_name);
		return 1;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		ERROR("%s does not support streaming i/o.\n", dev_name);
		return 1;
	}

	/* Select video input, video standard and tune here. */
	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (xioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		xioctl(fd, VIDIOC_S_CROP, &crop);
	}

	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (w && h) {
		fmt.fmt.pix.width       = w;
		fmt.fmt.pix.height      = h;
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

		if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
			ERROR("VIDIOC_S_FMT");
			return 1;
		}

		/* Note VIDIOC_S_FMT may change width and height. */
	} else {
		/* Preserve original settings as set by v4l2-ctl for example */
		if (xioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
			ERROR("VIDIOC_G_FMT");
			return 1;
		}
	}

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	return init_mmap(dev_name);
}

static inline uint8_t open_device(const char* dev_name) {
	struct stat st;

	if (stat(dev_name, &st) == -1) {
		ERROR("Cannot identify '%s': %d, %s\n",
			dev_name, errno, strerror(errno));
		return 1;
	}

	if (!S_ISCHR(st.st_mode)) {
		ERROR("%s is not a device\n", dev_name);
		return 1;
	}

	fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);

	if (fd == -1) {
		ERROR("Cannot open '%s': %d, %s\n",
			 dev_name, errno, strerror(errno));
		return 1;
	}
	return 0;
}

const char* car_camera_init(car_camera_t* camera, uint16_t id,
	uint16_t w, uint16_t h, void* output)
{
	sprintf(camera->dev_name, "/dev/video%d", id);
	camera->w = w;
	camera->h = h;
	camera->output = output;

	if(open_device(camera->dev_name)) return car_error;
	if(init_device(camera->dev_name, camera->w, camera->h))return car_error;
	if(start_capturing()) return car_error;
	return NULL;
}

const char* car_camera_loop(car_camera_t* camera) {
	for (;;) {
		fd_set fds;
		struct timeval tv;
		int r;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		/* Timeout. */
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(fd + 1, &fds, NULL, NULL, &tv);

		if (r == -1) {
			if (EINTR == errno)
				continue;

			ERROR("select");	
			return car_error;
		}

		if (0 == r) {
			ERROR("select timeout\n");
			return car_error;
		}

		switch (read_frame(camera->output)) {
			case 1: return NULL;
			case 2: return car_error;
			default: break;
		}
		/* EAGAIN - continue select loop. */
	}
	return NULL;
}

const char* car_camera_kill(car_camera_t* camera) {
	if(stop_capturing()) return car_error;
	if(uninit_device()) return car_error;
	if (close(fd) == -1) {
		ERROR("close");
		return car_error;
	}
	return NULL;
}
