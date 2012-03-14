#ifndef _MSG_QUEUE_H_
#define _MSG_QUEUE_H_

#define MSG_MAGIC		0x3db0aa74

typedef struct tagMsg
{
	unsigned int magic;
	unsigned int msg;
	unsigned int param1;
	unsigned int param2;
	unsigned int param3;
	unsigned int param4;
} MSG;

typedef struct tagMsgConext
{
	int s_w;
	int s_r;
	pthread_mutex_t mutex;
	char buffer[sizeof(MSG)];
	unsigned int len;
} MSG_CONTEXT;

bool create_msg_queue(MSG_CONTEXT * c, unsigned int size);

static inline void destroy_msg_queue(MSG_CONTEXT * c)
{
	pthread_mutex_destroy(&c->mutex);
	close(c->s_r);
	close(c->s_w);
	memset(c, 0, sizeof(MSG_CONTEXT));
}

static inline bool post_msg(MSG_CONTEXT * c, MSG * msg)
{
	msg->magic = MSG_MAGIC;		
	pthread_mutex_lock(&c->mutex);
	if (! is_socket_writeable(c->s_w, 0))
	{
		pthread_mutex_unlock(&c->mutex);
		//printf("%s: busy\n", __func__);
		return false;
	}	
	if (! socket_send(c->s_w, (const char *)msg, sizeof(MSG)))
	{
		pthread_mutex_unlock(&c->mutex);
		//printf("%s: send error\n", __func__);
		return false;
	}
	pthread_mutex_unlock(&c->mutex);
	return true;
}

static inline bool send_msg(MSG_CONTEXT * c, MSG * msg)
{
	msg->magic = MSG_MAGIC;		
	pthread_mutex_lock(&c->mutex);
	if (! socket_send(c->s_w, (const char *)msg, sizeof(MSG)))
	{
		pthread_mutex_unlock(&c->mutex);
		//printf("%s: send error\n", __func__);
		return false;
	}
	pthread_mutex_unlock(&c->mutex);
	return true;
}

static inline bool recv_msg(MSG_CONTEXT * c, MSG * msg)
{
	int ret, offset;
	unsigned int magic = MSG_MAGIC;
	
	if (0 > (ret = recv(c->s_r, c->buffer + c->len, sizeof(MSG) - c->len, 0)))
	{
		//printf("%s: recv error %d\n", __func__, ret);
		return false;
	}
	c->len += ret;
	if (c->len != sizeof(MSG))
	{
		//printf("%s: unexpect 1\n", __func__);
		return false;
	}
	for (offset = 0; offset < sizeof(MSG) - 4; offset ++)
	{
		if (0 == memcmp(c->buffer + offset, &magic, 4))
			break;
	}
	if (offset)
	{
		c->len -= offset;
		memcpy(c->buffer, c->buffer + offset, c->len);
		//printf("%s: unexpect 2\n", __func__);
		return false;
	}
	
	memcpy((char *)msg, c->buffer, sizeof(MSG));
	c->len = 0;
	return true;
}

#endif

