#ifndef _SENSOR_TYPE_H_
#define _SENSOR_TYPE_H_

typedef enum
{
	SENSOR_UNKNOWN = 0,
	OV9715,
	OV7725
} SENSOR;

#define CAMERA_BRIGHTNESS						0
#define CAMERA_BRIGHTNESS_CAP					(1 << CAMERA_BRIGHTNESS)

#define CAMERA_CONTRAST							1
#define CAMERA_CONTRAST_CAP						(1 << CAMERA_CONTRAST)

#define CAMERA_HUE								2
#define CAMERA_HUE_CAP							(1 << CAMERA_HUE)

#define CAMERA_SATURATION						3
#define CAMERA_SATURATION_CAP					(1 << CAMERA_SATURATION)

#define CAMERA_SHARPNESS						4
#define CAMERA_SHARPNESS_CAP					(1 << CAMERA_SHARPNESS)

#define CAMERA_POWERFREQ						5
#define CAMERA_POWERFREQ_CAP					(1 << CAMERA_POWERFREQ)

#define CAMERA_EV								6
#define CAMERA_EV_CAP							(1 << CAMERA_EV)

#define CAMERA_SCENE							7
#define CAMERA_SCENE_CAP						(1 << CAMERA_SCENE)

#define CAMERA_FLIP								8
#define CAMERA_FLIP_MIN							0
#define CAMERA_FLIP_MAX							3
#define CAMERA_FLIP_DEFAULT						0
#define CAMERA_FLIP_CAP							(1 << CAMERA_FLIP)

#endif
