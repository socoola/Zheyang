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

const char *spook_config = 
"\nFrameHeap 200;\n\n"
"Port 554;\n\n"
"Input video {\n"
"	FrameRate 30;\n"
"	Format \"h264\";\n"
"	output \"h264\";\n"
"}\n\n"
"Input video {\n"
"	FrameRate 30;\n"
"	Format \"h264\"; # or mjpeg\n"
"	output \"mpeg4\";\n"
"}\n\n"
"Input video {	\n"
"	FrameRate 30;\n"
"	Format \"h264\"; # or mjpeg\n"
"	output \"mjpeg\";\n"
"}\n\n"
"Input video {\n"
"	FrameRate 30;\n"
"	Format \"h264\"; # or mjpeg\n"
"	output \"h264_low\";\n"
"}\n\n"
"Input audio {\n"
"	FrameRate 8000;\n"
"	Format \"adpcm\";\n"
"	output \"pcm\";\n"
"}\n\n"
"RTSP-Handler Live {\n"
"	Path /webcam;\n"
"	Track \"h264\";\n"
"	Track \"pcm\";\n"
"}\n\n"
"RTSP-Handler Live {\n"
"	Path /webcam1;\n"	
"	Track \"mpeg4\";\n"
"	Track \"pcm\";\n"
"}\n\n"
"RTSP-Handler Live {\n"
"	Path /webcam2;\n"	
"	Track \"mjpeg\";\n"
"	Track \"pcm\";\n"
"}\n\n"
"RTSP-Handler Live {	\n"
"	Path /webcam3;\n"
"	Track \"h264_low\";\n"
"	Track \"pcm\";\n"
"}\n"
;

bool c_init_rtsp_server()
{
    int i;
    
	enum { DB_NONE, DB_FOREGROUND, DB_DEBUG } debug_mode = DB_NONE;

	printf("\r\nHello World!\r\n");

	//c_request_audio(NULL, ADPCM);

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
	//framedrop_init();
    video_init();
    audio_init();
	live_init();
	http_init();
	//control_listen();
    if(1)
    {
        FILE *fp;
        int len = strlen(spook_config);
        if((fp=fopen("/tmp/spook.conf","wb+"))==NULL)
        {
            spook_log( SL_ERR, "unable to open /tmp/spook.conf:%s",	strerror( errno ) );
            c_deinit_rtsp_server();
            return 0;
        }

        if(fwrite(spook_config, 1, len, fp) != len)
        {
            printf("write to file error:%d, %s", len, strerror( errno ));
        }
        
        fclose(fp);
    }

	if( read_config_file( "/tmp/spook.conf" ) < 0 )
    {
        spook_log( SL_ERR, "parse /tmp/spook.conf error" );
        c_deinit_rtsp_server();
        return 0;
	}

    return 1;
}

bool c_do_rtsp_server_handler()
{
    event_loop(1);

    return 1;
}

int global_deinit()
{
    event_deinit();
    deinit_frame_heap();
}

void c_deinit_rtsp_server()
{
    
    http_deinit();
    live_deinit();
    video_deinit();
    audio_deinit();

    access_log_deinit();
    spook_log_deinit();

    global_deinit();
}

