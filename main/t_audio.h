#ifndef _T_AUDIO_H_
#define _T_AUDIO_H_

extern THREAD_CONTEXT g_audio_thread_context;

bool t_create_audio_thread();

#define t_end_audio_thread() end_thread(g_audio_thread_context)

#define t_is_audio_thread_quited() is_thread_quited(g_audio_thread_context)

/*static inline void t_end_audio_thread()
{
	end_thread(&g_audio_thread_context);
}*/

static inline bool t_is_audio_thread_alive()
{
	if (! is_thread_alive(&g_audio_thread_context))
	{
		printf("%s: failed\n");
		return false;
	}
	
	return true;
}

#endif
