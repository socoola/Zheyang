#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#include <event.h>
#include <log.h>
#include <frame.h>
#include <stream.h>
#include <outputs.h>
#include <rtp.h>
#include <rtp_media.h>
#include <conf_parse.h>
#include "inputs.h"
#include "include/libcomponent.h"
#include "include/list.h"
#include "include/av_type.h"
#include "include/c_audio.h"
#include "include/c_user.h"
#include "include/c_video.h"
#include "include/utility.h"
#include "config.h"
#include "c_wlchk.h"

#define WIRELESS_CHECK_THREAD_ALIVE_THRESHOLD				(10 * 100)
#define WIRELESS_CHECK_THREAD_PRIORITY						2

static THREAD_CONTEXT g_wireless_check_thread_context;

static void * wireless_check_thread_function(void * arg)
{
    c_init_wireless_check(true);

    while(1)
    {
        c_do_wireless_check_handler();
    }

    c_deinit_wireless_check();
}

bool t_create_wireless_check_thread()
{
	bool ret;
/*
    if( read_config_file( WORK_DICTORY"//wireless.conf") < 0 )
    {
        return false;
    }
  */  
	g_wireless_check_thread_context.alive_threshold = WIRELESS_CHECK_THREAD_ALIVE_THRESHOLD;
	g_wireless_check_thread_context.handler = wireless_check_thread_function;
	g_wireless_check_thread_context.priority = WIRELESS_CHECK_THREAD_PRIORITY;
	if (! (ret = create_thread(&g_wireless_check_thread_context)))
		printf("%s: failed\n");

    return ret;
}

