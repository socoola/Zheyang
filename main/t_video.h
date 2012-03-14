#ifndef _T_VIDEO_H_
#define _T_VIDEO_H_

extern THREAD_CONTEXT g_video_thread_context;

bool t_create_video_thread();

#define t_end_video_thread() end_thread(g_video_thread_context)

#define t_is_video_thread_quited() is_thread_quited(g_video_thread_context)

/*static inline void t_end_video_thread()
{
	end_thread(&g_video_thread_context);
}*/

static inline bool t_is_video_thread_alive()
{
	if (! is_thread_alive(&g_video_thread_context))
	{
		printf("%s: failed\n", __func__);
		return false;
	}
	
	return true;
}

#endif
