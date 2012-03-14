#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>

#include <event.h>
#include <log.h>
#include <frame.h>
#include <stream.h>
#include <inputs.h>
#include <encoders.h>
#include <filters.h>
#include <outputs.h>
#include <rtp.h>
#include <conf_parse.h>
#include <config.h>

#include "include/libcomponent.h"
#include "c_udp_server.h"

bool c_init_udp_server(const char *config_file)
{
	int i;
    
	enum { DB_NONE, DB_FOREGROUND, DB_DEBUG } debug_mode = DB_NONE;

	debug_mode = DB_FOREGROUND;
	switch( debug_mode )
	{
	case DB_NONE:
		spook_log_init( SL_INFO );
		break;
	case DB_FOREGROUND:
		spook_log_init( SL_VERBOSE );
		break;
	case DB_DEBUG:
		spook_log_init( SL_DEBUG );
		break;
	}

	if( init_random() < 0 ) return 0;

	access_log_init();
/*
	oss_init();
#ifdef SPOOK_INPUT_V4L
	v4l_init();
#endif
#ifdef SPOOK_INPUT_V4L2
	v4l2_init();
#endif
#ifdef SPOOK_INPUT_DC1394
	dc1394_init();
#endif
#ifdef SPOOK_INPUT_VDIG
	vdig_init();
#endif
#ifdef SPOOK_ENCODER_JPEG
	jpeg_init();
#endif
*/
	framedrop_init();
/*
	alaw_init();
#ifdef SPOOK_ENCODER_MPEG4
	mpeg4_init();
	mpeg4_dec_init();
#endif
    mp2_init();
*/
	//input_rtsp_init();
    video_init();
    audio_init();
	live_init();
	http_init();
	control_listen();

	if( read_config_file( config_file ) < 0 ) return 0;

    return 1;
}

void c_do_udp_server_handler()
{
	event_loop(0);
}

void c_deinit_udp_server()
{

}


