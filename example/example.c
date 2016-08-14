#include <stdio.h>
#include "lib-car-wreck/car.h"

int main(int argc, char **argv) {
	car_camera_t camera;
	uint8_t output[640*480*3]; // RGB
	const char* error;
	int i;

	if((error = car_camera_init(&camera, 0, 640, 480, output))) {
		printf("Error %s.\n", error);
		return 1;
	}
	for(i = 0; i < 20; i++) {
		if((error = car_camera_loop(&camera))) {
			printf("Error %s.\n", error);
			return 1;
		}
	}
	if((error = car_camera_kill(&camera))) {
		printf("Error %s.\n", error);
		return 1;
	}
	printf("Success!\n");
	return 0;
}
