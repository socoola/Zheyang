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
#include "c_rtsp.h"


#define RTSP_THREAD_ALIVE_THRESHOLD				(10 * 100)
#define RTSP_THREAD_PRIORITY						2

THREAD_CONTEXT g_rtsp_thread_context;

static void * rtsp_thread_function(void * arg);

bool t_create_rtsp_thread()
{
	bool ret;
	g_rtsp_thread_context.alive_threshold = RTSP_THREAD_ALIVE_THRESHOLD;
	g_rtsp_thread_context.handler = rtsp_thread_function;
	g_rtsp_thread_context.priority = RTSP_THREAD_PRIORITY;
	if (! (ret = create_thread(&g_rtsp_thread_context)))
		printf("%s: failed\n", __func__);
	
	return ret;
}

static void * rtsp_thread_function(void * arg)
{
	THREAD_CONTEXT * c = (THREAD_CONTEXT *)arg;

	if (! c_init_rtsp_server())
	{
		c_reboot();
		return NULL;
	}

	while (! c->exit)
	{
		c->tick = get_tickcount();
		c_do_rtsp_server_handler();
        printf("rtsp handler\n");
	}
	
	c_deinit_rtsp_server();
	printf("rtsp quited\n");
	c->quited = true;

	return NULL;
}


