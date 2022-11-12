#ifndef SRVR_STR
#define SRVR_STR

#include "duckchat.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

typedef struct srvr_str{
    u_int32_t address; 
    int16_t port;
} Server;

int in_array(char *address, char *port, struct sockaddr_in **srvr_list, int list_len);

#endif