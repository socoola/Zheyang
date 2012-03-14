#include "include/libcomponent.h"
#include "c_speak.h"

#define SPEAK_THREAD_ALIVE_THRESHOLD				(10 * 100)
#define SPEAK_THREAD_PRIORITY						2

static THREAD_CONTEXT g_speak_thread_context;

static void * speak_thread_function(void * arg)
{
    THREAD_CONTEXT * c = (THREAD_CONTEXT *)arg;    

    c_init_speak_server(12345);
    while(!c->exit)
    {
        c_do_speak_server_handler();
    }

    c_deinit_speak_server();

	return NULL;
}

bool t_create_speak_thread()
{
    bool ret;
    
    g_speak_thread_context.alive_threshold = SPEAK_THREAD_ALIVE_THRESHOLD;
	g_speak_thread_context.handler = speak_thread_function;
	g_speak_thread_context.priority = SPEAK_THREAD_PRIORITY;
	if (! (ret = create_thread(&g_speak_thread_context)))
		printf("%s: failed\n");
	
	return ret;
}

