#ifndef NWM_CONFIG
#define NWM_CONFIG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
//#include <signal.h>
#include <pthread.h>

int startserver(void);
int killserver(void);

#endif
