# lib-car-wreck
A portable library for capture video and audio.

# Why use lib-car-wreck?

## Simple API

There are only three functions in this library:

```
const char* car_camera_init(car_camera_t* camera, uint16_t id, uint16_t w, uint16_t h, void* output);
```

```
const char* car_camera_loop(car_camera_t* camera);
```

```
const char* car_camera_kill(car_camera_t* camera);
```

The return value for all of these is NULL, unless there is an error: then a
string explaining the error is returned.

The 1st parameter is an object that represents a camera.  Just do a:
```
	car_camera_t camera;
```

And pass a:
```
	&camera
```
Into all of the functions

For most cases "id" will be zero.  If you have plugged in an extra webcam "id"
will be one.  And the more cameras you plug in just keep adding to the variable
to use them.

"w" and "h" are the width and height of the image you want to recieve, and
output is where you want to receive the data.  The size is assumed to be w * h * 3.
3 for Red, Green, and Blue.

## Cross-Platform

Ok... this hasn't been implemented yet, but I plan to support Windows & Apple
with this library too ( right now just Linux ).

## Easy include

Just copy the source directory into whatever project you want to use it in.

## Small

Now you won't have to include a giant project like opencv if you just want to
get an image from a webcam!

## Open Source

Ha ha ha!  Anyone is welcome to contribute.

