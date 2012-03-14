#ifndef _C_VIDEO_H_
#define _C_VIDEO_H_

typedef void (* GET_VIDEO_FRAME_CALLBACK)(VIDEO_FRAME * frame);

typedef void (* REQUEST_JPEG_CALLBACK)(bool result, unsigned char * jpeg, unsigned int len, void * param);

bool c_request_video(GET_VIDEO_FRAME_CALLBACK callback, unsigned char stream);

bool c_end_video(GET_VIDEO_FRAME_CALLBACK callback, unsigned char stream);

bool c_request_jpeg(REQUEST_JPEG_CALLBACK callback, void * param, int resolution);

//bool c_reinit_video();

int c_get_video_codec(unsigned char stream);

unsigned int c_get_video_width(unsigned char stream);

unsigned int c_get_video_height(unsigned char stream);

unsigned int c_get_video_bitrate(unsigned char stream);

bool c_init_video();

void c_deinit_video();

void c_do_video_handler();

void c_reinit_osd();

void c_start_motion_detect(MOTION_DETECT_CONFIG * md_cfg);

void c_stop_motion_detect();

#define c_is_motion_detected p_is_motion_detected

#define c_reset_motion_detected p_reset_motion_detected

void c_reinit_camera();

void c_camera_control(int command, int value);

#endif
