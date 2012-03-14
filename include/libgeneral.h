#ifndef _LIB_GENERAL_H_
#define _LIB_GENERAL_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/times.h>
#include <sys/timex.h>	//	for ntpc
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/vfs.h>
#include <dirent.h>
#include <signal.h>

#include "list.h"
#include "data_type.h"
#include "utility.h"
#include "msg_queue.h"
#include "thread.h"
#include "params.h"

#endif
