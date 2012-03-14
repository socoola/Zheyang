#ifndef _THREAD_H_
#define _THREAD_H_

#define MIN_PRIORITY					0
#define MAX_PRIORITY					4

typedef struct tagThreadContext
{
	unsigned long alive_threshold;
	int priority;
	unsigned long tick;
	bool exit;
	bool quited;
	pthread_t thread;
	void * (* handler)(void * arg);
} THREAD_CONTEXT;

bool create_thread(THREAD_CONTEXT * c);

#define end_thread(c) c.exit = true

bool is_thread_alive(THREAD_CONTEXT * c);

#define is_thread_quited(c) c.quited

#endif
