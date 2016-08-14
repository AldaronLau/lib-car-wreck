#include <stdint.h>

typedef struct {
	char dev_name[20];
	uint16_t w;
	uint16_t h;
	void* output;
} car_camera_t;

const char* car_camera_init(car_camera_t* camera, uint16_t id,
	uint16_t w, uint16_t h, void* output);
const char* car_camera_loop(car_camera_t* camera);
const char* car_camera_kill(car_camera_t* camera);
