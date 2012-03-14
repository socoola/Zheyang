#include "include/libcomponent.h"
#include "t_video.h"

#define VIDEO_THREAD_ALIVE_THRESHOLD				(10 * 100)
#define VIDEO_THREAD_PRIORITY						1

THREAD_CONTEXT g_video_thread_context;

static void * video_thread_function(void * arg);

bool t_create_video_thread()
{
	bool ret;
	g_video_thread_context.alive_threshold = VIDEO_THREAD_ALIVE_THRESHOLD;
	g_video_thread_context.handler = video_thread_function;
	g_video_thread_context.priority = VIDEO_THREAD_PRIORITY;
	if (! (ret = create_thread(&g_video_thread_context)))
		printf("%s: failed\n", __func__);
	
	return ret;
}

static void * video_thread_function(void * arg)
{
	THREAD_CONTEXT * c = (THREAD_CONTEXT *)arg;

	if (! c_init_video())
	{
		c_reboot();
		return NULL;
	}

	while (! c->exit)
	{
		c->tick = get_tickcount();
		c_do_video_handler();
	}
	
	c_deinit_video();
	printf("video quited\n");
	c->quited = true;

	return NULL;
}
