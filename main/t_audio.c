#include "include/libcomponent.h"
#include "t_audio.h"
#include "msg_queue.h"
#include "enet/enet.h"
#include <log.h>

#define AUDIO_THREAD_ALIVE_THRESHOLD				(10 * 100)
#define AUDIO_THREAD_PRIORITY						2

THREAD_CONTEXT g_audio_thread_context;

extern MSG_CONTEXT *g_pMsgQ;

static void * audio_thread_function(void * arg);

bool t_create_audio_thread()
{
	bool ret;
	g_audio_thread_context.alive_threshold = AUDIO_THREAD_ALIVE_THRESHOLD;
	g_audio_thread_context.handler = audio_thread_function;
	g_audio_thread_context.priority = AUDIO_THREAD_PRIORITY;
	if (! (ret = create_thread(&g_audio_thread_context)))
		printf("%s: failed\n");

    return ret;
}

static void * audio_thread_function(void * arg)
{
	THREAD_CONTEXT * c = (THREAD_CONTEXT *)arg;

	if (! c_init_audio())
	{
		c_reboot();
		return NULL;
	}
	
	while (! c->exit)
	{
		c->tick = get_tickcount();
		if (! c_do_audio_handler())
			break;
	}
	
	c_deinit_audio();
	printf("audio quited\n");
	c->quited = true;

	return NULL;
}
