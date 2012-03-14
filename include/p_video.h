#ifndef _P_VIDEO_H
#define _P_VIDEO_H

#define MAX_VIDEO_STREAMS_NUMBER				4

#define OSD_POS_LEFT_TOP				0
#define OSD_POS_RIGHT_TOP				1
#define OSD_POS_LEFT_BOTTOM				2
#define OSD_POS_RIGHT_BOTTOM			3
#define OSD_POS_MID						4

#define OSD_COLOR_WHITE_BLACK			0
#define OSD_COLOR_WHITE_TRANSPARENT		1
#define OSD_COLOR_BLACK_TRANSPARENT		2
#define OSD_COLOR_YELLOW_TRANSPARENT	3
#define OSD_COLOR_RED_TRANSPARENT		4
#define OSD_COLOR_BLUE_TRANSPARENT		5

#define OSD_COLOR_WHITE					0
#define OSD_COLOR_BLACK					1
#define OSD_COLOR_YELLOW				2
#define OSD_COLOR_RED					3
#define OSD_COLOR_BLUE					4

void p_init_video_context(int snapshot_quality, SENSOR sensor);

bool p_set_video_stream_params(unsigned char stream, int resolution, int codec, int bitrate, int fps, int gop, int rate_ctrl, int quality);

bool p_init_video();

void p_do_video_handler(struct list_head * head);

bool p_is_video_working();

void p_deinit_video();

bool p_open_video(unsigned char stream);

void p_close_video(unsigned char stream);

bool p_snapshot(int resolution);

void p_cancel_snapshot(int resolution);


#define MAX_MD_WINOWS_NUMBER					3

#define MAX_MD_SENSITIVITY						9

typedef struct tagMotionDetectConfig
{
	struct 
	{
		bool valid;
		unsigned char left;
		unsigned char top;
		unsigned char right;
		unsigned char bottom;
	} roi[MAX_MD_WINOWS_NUMBER];
	unsigned char sensitivity;
	bool osd;
	int osd_color;
} MOTION_DETECT_CONFIG;

bool p_start_motion_detect(MOTION_DETECT_CONFIG * md_cfg);

void p_stop_motion_detect();

void p_set_motion_detect_delay(unsigned long tick);

bool p_is_motion_detected();

void p_reset_motion_detected();

void p_set_osd(bool osd, const char * msg, int pos, int color, bool mask, int mask_left, int mask_top, int mask_right, int mask_bottom);

#define ISP_BRIGHTNESS_MIN					0
#define ISP_BRIGHTNESS_MAX					50
#define ISP_BRIGHTNESS_DEFAULT				25

#define ISP_CONTRAST_MIN						0
#define ISP_CONTRAST_MAX						10
#define ISP_CONTRAST_DEFAULT					5

#define ISP_HUE_MIN							0
#define ISP_HUE_MAX							32
#define ISP_HUE_DEFAULT						16

#define ISP_SATURATION_MIN					0
#define ISP_SATURATION_MAX					16
#define ISP_SATURATION_DEFAULT				8

#define ISP_SHARPNESS_MIN					0
#define ISP_SHARPNESS_MAX					4
#define ISP_SHARPNESS_DEFAULT				2

#define ISP_POWERFREQ_MIN					0
#define ISP_POWERFREQ_MAX					1
#define ISP_POWERFREQ_DEFAULT				1

#define ISP_EV_MIN							0
#define ISP_EV_MAX							1
#define ISP_EV_DEFAULT						0

#define ISP_SCENE_MIN						0
#define ISP_SCENE_MAX						5
#define ISP_SCENE_DEFAULT					0

bool p_isp_set_brightness(int value);
bool p_isp_set_contrast(int value);
bool p_isp_set_hue(int value);
bool p_isp_set_saturation(int value);
bool p_isp_set_sharpness(int value);
bool p_isp_set_powerfreq(int value);
bool p_isp_set_ev(int value);
bool p_isp_set_scene(int value);
bool p_isp_set_flip(int value);

#endif
