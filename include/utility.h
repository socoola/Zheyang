#ifndef _UTILITY_H_
#define _UTILITY_H_

#ifdef MEM_DEBUG
void * my_malloc(unsigned int size);
void my_free(void * p);
#else 
#define my_malloc(size) malloc(size)
#define my_free(p) free(p)
#endif	

#define get_tickcount() (unsigned long)times(NULL)

#ifdef MIN
#undef MIN
#endif
#define MIN(x,y) (((x)<(y))?(x):(y))

#ifdef MAX
#undef MAX
#endif
#define MAX(x,y) (((x)<(y))?(y):(x))

static inline void set_socket_timeout(int s, unsigned int w_timeout, unsigned int r_timeout)
{
	struct timeval tv;
	tv.tv_sec = w_timeout;
	tv.tv_usec = 0;
	setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,(char *)&tv,sizeof(tv));
	tv.tv_sec = r_timeout;
	setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(char *)&tv,sizeof(tv));
}

static inline void set_socket_buff_size(int s, unsigned int w_size, unsigned int r_size)
{
	setsockopt(s,SOL_SOCKET,SO_SNDBUF,(char *)&w_size,sizeof(int));
	setsockopt(s,SOL_SOCKET,SO_RCVBUF,(char *)&r_size,sizeof(int));
}

static inline void disable_socket_linger(int s)
{
	struct linger linger;
	linger.l_onoff = 1;
	linger.l_linger = 0;
	setsockopt(s,SOL_SOCKET, SO_LINGER,(const char *)&linger,sizeof(linger));
}

static inline void set_socket_reuseaddr(int s)
{
	int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
}

static inline void set_socket_broadcast(int s)
{
	int opt = 1;
	setsockopt(s,SOL_SOCKET,SO_BROADCAST,(char *)&opt,sizeof(int));
}

static inline void set_socket_nonblock(int s)
{
	fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
}

static inline void set_socket_ttl(int s, unsigned int ttl)
{
	setsockopt(s,IPPROTO_IP,IP_TTL,(char *)&ttl,sizeof(int));
}

static inline void set_socket_multicast(int s, unsigned char ttl, unsigned long ip)
{
	setsockopt(s,IPPROTO_IP,IP_MULTICAST_TTL,(char *)&ttl,sizeof(ttl));
    setsockopt(s,IPPROTO_IP,IP_MULTICAST_IF,(char *)&ip,sizeof(int));
}

bool is_socket_writeable(int s, unsigned int ms);

bool is_socket_readable(int s, unsigned int ms);

bool socket_connect(int s, const char * host, unsigned short port, unsigned int timeout);

bool socket_send(int s, const char * data, unsigned int len);

bool socket_recv(int s, char * data, unsigned int len);

static inline unsigned char * mem_new(int value, unsigned int len)
{
	unsigned char * p = (unsigned char *)my_malloc(len);
	if (! p)
		return NULL;
	memset(p, value, len);
	return p;
}

static inline unsigned char * mem_assign(const unsigned char * value, unsigned int len)
{
	unsigned char * p = (unsigned char *)my_malloc(len);
	if (! p)
		return NULL;
	memcpy(p, value, len);
	return p;
}

static inline unsigned char * mem_combine(const unsigned char * src1, unsigned int src1_len, const unsigned char * src2, unsigned int src2_len)
{
	unsigned char * p = (unsigned char *)my_malloc(src1_len + src2_len);
	if (! p)
		return NULL;
	memcpy(p, src1, src1_len);
	memcpy(p + src1_len, src2, src2_len);
	return p;
}

static inline unsigned char * mem_append(unsigned char * src, unsigned int src_len, const unsigned char * append, unsigned int append_len)
{
	unsigned char * p = mem_combine(src, src_len, append, append_len);
	my_free(src);
	return p;
}

static inline char * str_assign(const char * value)
{
	return (char *)mem_assign((unsigned char *)value, strlen(value) + 1);
}

static inline char * str_append(char * src, const char * append)
{
	return (char *)mem_append((unsigned char *)src, strlen(src), (const unsigned char *)append, strlen(append) + 1);
}

static inline char * str_combine(const char * src1, const char * src2)
{
	return (char *)mem_combine((const unsigned char *)src1, strlen(src1), (const unsigned char *)src2, strlen(src2) + 1);
}

static inline void trim(char ** s)
{
	char * begin = * s;
	char * end = * s + strlen(* s) - 1;
	
	while ((* begin == ' ') || (* begin == '\t'))
		begin ++;
	while ((* end == ' ') || (* end == '\t') || (* end == '\r') || (* end == '\n'))
		end --;
	* (end + 1) = 0;
	* s = begin;
}

static inline bool get_next_str_p(char ** pos, unsigned int * len, char ** out, unsigned int outlen)
{
	if (* len < outlen)
		return false;
	* out = (char *)(*pos);
	* pos += outlen;
	* len -= outlen;
	return true;
}

static inline bool get_next_int(char ** pos, unsigned int * len, unsigned int * out)
{
	if (* len < 4)
		return false;
	memcpy(out, *pos, 4);
	*pos += 4;
	* len -= 4;
	return true;
}

static inline bool get_next_short(char ** pos, unsigned int * len, unsigned short * out)
{
	if (* len < 2)
		return false;
	memcpy(out, *pos, 2);
	*pos += 2;
	* len -= 2;
	return true;
}

static inline bool get_next_char(char ** pos, unsigned int * len, unsigned char * out)
{
	if (* len < 1)
		return false;
	* out = * (unsigned char *)(*pos);
	(* pos) ++;
	(* len) --;
	return true;
}

static inline unsigned int to_http_js_string(char * buffer, const char * value)
{
	unsigned int offset = 0;
	while (* value)
	{
		if ((* value == '\\') || (* value == '\''))
			buffer[offset ++] = '\\';
		buffer[offset ++] = * value++;
	}
	buffer[offset] = 0;
	return offset;
}

static inline unsigned int to_http_js_var_string(char * buffer, const char * name, const char * value)
{
	unsigned int offset = 0;
	offset += sprintf(buffer + offset, "var %s=\'", name);
	offset += to_http_js_string(buffer + offset, value);
	offset += sprintf(buffer + offset, "\';\n");
	return offset;
}

static inline unsigned int to_http_js_var_array_string(char * buffer, const char * name, unsigned int index, const char * value)
{
	unsigned int offset = 0;
	offset += sprintf(buffer + offset, "%s[%u]=\'", name, index);
	offset += to_http_js_string(buffer + offset, value);
	offset += sprintf(buffer + offset, "\';\n");
	return offset;
}

/*inline unsigned int to_http_js_var_int(char * buffer, const char * name, int value)
{
	return sprintf(buffer, "var %s=%d;\n", name, value);
}

inline unsigned int to_http_js_var_array_int(char * buffer, const char * name, unsigned int index, int value)
{
	return sprintf(buffer, "var %s[%u]=%d;\n", name, index, value);
}

inline unsigned int to_http_js_var_array_name(char * buffer, const char * name)
{
	return sprintf(buffer, "var %s=new Array();\n", name);
}*/

static inline unsigned char hash(const char * name)
{
	unsigned int hash = 0;
	
	while (* name)
	{
		hash = 31 * hash + * name;
		name ++;
	}
	
	return (unsigned char)hash;
}

unsigned int base64_encode(const unsigned char * src, unsigned int src_len, char * dest);

unsigned int base64_decode(const char * src, unsigned int src_len, unsigned char * dest);

unsigned short parse_url(const char * url, char ** host, char ** path);

int http_client_get(const char * url, const char * user, const char * pwd, char ** result_content, unsigned int * result_content_len, unsigned int version);

int http_client_post(const char * url, const char * user, const char * pwd, const char * content_type, const unsigned char * content, unsigned int content_len, const char * extra, char ** result_content, unsigned int * result_content_len, unsigned int version);

extern unsigned long g_resolution_list[][4];

static inline unsigned long get_resolution_width(int resolution)
{
	if ((resolution < SQCIF) || (resolution > H720P)) 
		return 0;
	else
		return g_resolution_list[resolution][0];
}

static inline unsigned long get_resolution_height(int resolution)
{
	if ((resolution < SQCIF) || (resolution > H720P)) 
		return 0;
	else
		return g_resolution_list[resolution][1];
}

static inline unsigned long get_resolution_src_width(int resolution)
{
	if ((resolution < SQCIF) || (resolution > H720P)) 
		return 0;
	else
		return g_resolution_list[resolution][2];
}

static inline unsigned long get_resolution_src_height(int resolution)
{
	if ((resolution < SQCIF) || (resolution > H720P)) 
		return 0;
	else
		return g_resolution_list[resolution][3];
}

void adpcm_encode(unsigned char * raw, int len, unsigned char * encoded, int * pre_sample, int * index);

void adpcm_decode(unsigned char * raw, int len, unsigned char * decoded, int * pre_sample, int * index);

static inline unsigned long my_gethostbyname(const char * server)
{
	unsigned long ip;
	struct hostent host, * lphost;
	char * dnsbuffer;
	int rc;

	if (INADDR_NONE != (ip = inet_addr(server)))
		return ip;
	if (NULL == (dnsbuffer = (char *)my_malloc(8192)))
		return INADDR_NONE;
	if (gethostbyname_r(server, &host, dnsbuffer, 8192, &lphost, &rc) || (! lphost))
	{
		res_init();
		if (gethostbyname_r(server, &host, dnsbuffer, 8192, &lphost, &rc) || (! lphost))
		{
			printf("can not resolve ip of %s\n", server);
			ip = INADDR_NONE;
		}
		else
		{
			ip = ((struct in_addr *)(lphost->h_addr))->s_addr;
		}
	}
	else
	{
		ip = ((struct in_addr *)(lphost->h_addr))->s_addr;
	}
	
	my_free(dnsbuffer);
	return ip;
}

bool str2mac(const char * str, unsigned char * mac);

void ignore_sigpipe();

#endif
