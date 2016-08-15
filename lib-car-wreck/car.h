#include <stdint.h>

typedef struct {
	// Linux specific
	char dev_name[20];
	void* buffer;

	//
	void* data; // JPEG file data
	uint32_t size; // Size of JPEG file
} car_camera_t;

const char* car_camera_init(car_camera_t* camera, uint16_t id,
	uint16_t w, uint16_t h, void** output);
const char* car_camera_loop(car_camera_t* camera);
const char* car_camera_kill(car_camera_t* camera);
