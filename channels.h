#ifndef CHNNLS
#define CHNNLS

#include "duckchat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


#define MAX_USERS 32
#define MAX_CHANNELS 64

typedef struct usr
{
    char username[USERNAME_MAX];
    struct sockaddr_in address;
    unsigned int addresslen;
} User;

typedef struct srv{
    struct sockaddr_in addr;
    char sub_channels[MAX_CHANNELS][CHANNEL_MAX];
    int num_chnnls;
} Server;

typedef struct chnnl{
    char chnl_name[CHANNEL_MAX];
    User *connected_users[MAX_USERS];
    int num_users;
    User *(*find_user)(struct chnnl *chnl, char *username);
    User *(*create_user)(struct chnnl *chnl, char *username, struct sockaddr_in *addr, unsigned int len);
    User *(*remove_user)(struct chnnl *chnl, char *username);
    User *(*find_byaddr)(struct chnnl *, struct sockaddr_in addr);
    void (*add_user)(struct chnnl *, User *usr);
    void (*destroy)(struct chnnl *chnl);

} Channel;

Channel *create_channel(char *chnl_name);

void init_channel(Channel *chnl);

int comp_sockaddr(struct sockaddr_in a, struct sockaddr_in b);

/**
 * Adds a channel name to an adjacent server
 * @param srvr the server to add the channel to
 * @param channel the string name of the channel
*/
void add_ch_srv(Server *srvr, char *channel);

/**
 * Removes the channel name from an adjacent server
 * @param srvr the server to remove the channel from
 * @param channel the channel name to be removed
 * @return 1 if succesfully removed channel, 0 otherwise
*/
int remove_ch_srv(Server *srvr, char *channel);

#endif