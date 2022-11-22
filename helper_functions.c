#include "helper_functions.h"
#include "duckchat.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define TO_SEC(x) x * 1000000 / CLOCKS_PER_SEC

void say_text_output(char *channel, char *username, char *text){
    write(STDOUT_FILENO, "[", 1);
    write(STDOUT_FILENO, channel, CHANNEL_MAX);
    write(STDOUT_FILENO, "]", 1);
    write(STDOUT_FILENO, "[", 1);
    write(STDOUT_FILENO, username, USERNAME_MAX);
    write(STDOUT_FILENO, "]: ", 3);
    write(STDOUT_FILENO, text, SAY_MAX);
}

int decode_input(char *inp){
    if (inp[0] != '/')
    {
        return REQ_SAY;
    }else if (strncmp(inp, "/join", 5) == 0)
    {
        return REQ_JOIN;
    }else if (strncmp(inp, "/list", 5) == 0)
    {
        return REQ_LIST;
    }else if(strncmp(inp, "/who", 4) == 0)
    {
        return REQ_WHO;
    }else if (strncmp(inp, "/leave", 6) == 0)
    {
        return REQ_LEAVE;
    }else if(strncmp(inp, "/switch", 7) == 0)
    {
        return REQ_SWITCH;
    }else if(strncmp(inp, "/exit", 5) == 0)
    {
        return REQ_LOGOUT;
    }
    return -1;
}

char *get_command_arg(char *buf, int *chnl_len){
    int i = 0;
    int chnl_start = -1;
    while (buf[i] != '\0')
    {
      if (buf[i] == ' ')
      {
        if (chnl_start == -1)
        {
            chnl_start = i + 1;
        }
      }
      i++;
    }
    if (chnl_start < 0)
    {
        *chnl_len = 0;
    }else{
        *chnl_len = i - chnl_start;
    }

    return chnl_start + buf;
}

void clear_stdout(int clear_chars){
    for (int i = 0; i < clear_chars; i++)
    {
        write(STDOUT_FILENO, "\b", 1);
    }
}

void print_channel_list(struct text *inp_t){
    struct text_list *tl = (struct text_list *)inp_t;
    write(STDOUT_FILENO, "Existing Channels:", 19);
    for (int i = 0; i < tl->txt_nchannels; i++)
    {
        write(STDOUT_FILENO, "\n\t", 2);
        write(STDOUT_FILENO, (tl->txt_channels + i)->ch_channel, CHANNEL_MAX);
    }

}

void print_user_list(struct text *inp_t){
    struct text_who *tl = (struct text_who *)inp_t;
    
    write(STDOUT_FILENO, "All users in ", 14);
    write(STDOUT_FILENO, tl->txt_channel, CHANNEL_MAX);
    for (int i = 0; i < tl->txt_nusernames; i++)
    {
        write(STDOUT_FILENO, "\n\t", 2);
        write(STDOUT_FILENO, (tl->txt_users + i)->us_username, USERNAME_MAX);
    }

}

int send_join_req(char *buff, int socketfd, struct sockaddr_in *server_addr, int len, char *active_channel){
    struct request_join req_jn;
    memset(&req_jn, 0, sizeof(req_jn));
    int chnl_len;
    char *chnl = get_command_arg(buff, &chnl_len);
    if (chnl_len == 0)
    {
        clear_stdout(50);
        write(STDOUT_FILENO, "Not enough arguments\n", 20);
        return 0;
    }
    strncpy(req_jn.req_channel, chnl, chnl_len);
    req_jn.req_type = REQ_JOIN;

    if(sendto(socketfd, &req_jn, sizeof(req_jn), 0, (struct sockaddr *)server_addr, len) < 0){
        write(STDOUT_FILENO, "Could not send join request\n", 29);
        return 0;
    }
    strcpy(active_channel, chnl);
    return 1;
}

int send_say_req(int socketfd, struct sockaddr_in *s_addr, int len, char *active_channel, char *buff){
    struct request_say rs;
    memset(&rs, 0, sizeof(rs));
    strcpy(rs.req_channel, active_channel);
    rs.req_type = REQ_SAY;
    strcpy(rs.req_text, buff);
    
    if(sendto(socketfd, &rs, sizeof(rs), 0, (struct sockaddr *)s_addr, len) < 0){
        clear_stdout(50);
        write(STDOUT_FILENO, "Could not send say request\n", 28);
        return 0;
    }
    return 1;
}

int send_list_req(int socketfd, struct sockaddr_in *s_addr, int len){
    struct request_list rl;
    memset(&rl, 0, sizeof(rl));
    rl.req_type = REQ_LIST;
    if(sendto(socketfd, &rl, sizeof(rl), 0, (struct sockaddr *)s_addr, len) < 0){
        clear_stdout(50);
        write(STDOUT_FILENO, "Could not send list request\n", 28);
        return 0;
    }
    return 1;
}

int send_who_req(int socketfd, struct sockaddr_in *s_addr, int len, char *buff){
    int chnl_len;
    char *chnl_name = get_command_arg(buff, &chnl_len);
    if (chnl_len == 0)
    {
        clear_stdout(50);
        write(STDOUT_FILENO, "Not enough arguments\n", 20);
        return 0;
    }
    struct request_who re_who;
    memset(&re_who, 0, sizeof(re_who));
    re_who.req_type = REQ_WHO;
    strncpy(re_who.req_channel, chnl_name, chnl_len);
    if(sendto(socketfd, &re_who, sizeof(re_who), 0, (struct sockaddr *)s_addr, len) < 0){
        clear_stdout(50);
        write(STDOUT_FILENO, "Could not send who request\n", 28);
        return 0;
    }
    return 1;
}

int send_leave_req(int socketfd, struct sockaddr_in *s_addr, int len, char *buff, char *active_channel){
    struct request_leave req_l;
    memset(&req_l, 0, sizeof(req_l));
    req_l.req_type = REQ_LEAVE;
    int chnl_len;
    char *chnl = get_command_arg(buff, &chnl_len);
    if (chnl_len == 0)
    {
        clear_stdout(50);
        write(STDOUT_FILENO, "Not enough arguments\n", 20);
        return 0;
    }
    
    strncpy(req_l.req_channel, chnl, chnl_len);
    if (strncmp(active_channel, chnl, chnl_len) == 0)
    {
        strcpy(active_channel, "Common");
    }
    
    if(sendto(socketfd, &req_l, sizeof(req_l), 0, (struct sockaddr *)s_addr, len) < 0){
        clear_stdout(50);
        write(STDOUT_FILENO, "Could not send who request\n", 28);
        return 0;
    }
    return 1;
}

int send_logout_req(int socketfd, struct sockaddr_in *s_addr, int len){
    struct request_logout req_lg;
    memset(&req_lg, 0, sizeof(req_lg));
    req_lg.req_type = REQ_LOGOUT;
    if(sendto(socketfd, &req_lg, sizeof(req_lg), 0, (struct sockaddr *)s_addr, len) < 0){
        clear_stdout(50);
        write(STDOUT_FILENO, "Could not send logout reqeust\n", 30);
        return 0;
    }
    return 1;
}

Channel *find_channel(Channel **chnls, int chnls_len, char* channel_name){
    for (int i = 0; i < chnls_len; i++)
    {
        if (strcmp(chnls[i]->chnl_name, channel_name) == 0)
        {
            return chnls[i];
        }
        
    }
    return NULL;
}

User *find_user(User **usrs, int usrs_len, struct sockaddr_in addr, int *position){
    for (int i = 0; i < usrs_len; i++)
    {
        if (comp_sockaddr(usrs[i]->address, addr))
        {
            if (position != NULL)
            {
                *position = i;
            }
            return usrs[i];
        }
    }
    return NULL;
}

Channel *add_chnl(Channel **chnls, int *num_chnls, char *chnl_name){
    Channel *chnl = create_channel(chnl_name);
    chnls[(*num_chnls)++] = chnl;
    return chnl;
}

void *neat_remove(void *list[], int *list_len, void *id, int (*cmp)(void*,void*), int *pos){
    void *ret;
    int move = 0;
    int i;
    for (i = 0; i < *list_len; i++)
    {
        if (move)
        {
            list[i-1] = list[i];

        }else if((pos == NULL || *pos < 0) && cmp(list[i], id)){
            ret = list[i];
            move = 1;
        }else if(pos != NULL && *pos >= 0 && i == *pos){
            ret = list[i];
            move = 1;
        }
        
    }
    list[i] = NULL;
    *list_len -= 1;
    return ret;
}

int chnl_cmp(void *chnl, void*name){
    Channel *channel = (Channel *)chnl;
    char *ch_name = (char *)name;
    return strcmp(channel->chnl_name, ch_name) == 0;
}

Channel *remove_chnl(Channel **chnls, int *num_chnls, char *chnl_name){
    return (Channel *)neat_remove((void**)chnls, num_chnls, chnl_name, chnl_cmp, NULL);

}

struct text_error fill_error(char *chnl_name){
    struct text_error t;
    memset(&t, 0, sizeof(t));
    t.txt_type = TXT_ERROR;
    sprintf(t.txt_error, "Error: Cannot find channel: %s", chnl_name);
    return t;
}

struct sockaddr_in create_sockaddr(char *addr, char *port){
    struct sockaddr_in ret;
    memset(&ret, 0, sizeof(ret));
    ret.sin_family = AF_INET;
    ret.sin_port = htons((uint16_t)atoi(port));
    if (strcmp(addr, lclhst) == 0)
    {
        strcpy(addr, "127.0.0.1");
    }
    ret.sin_addr.s_addr = inet_addr(addr);
    return ret;
}

struct request_say_s2s s2s_fill_say(char *channel, char *username, char *text){
    struct request_say_s2s rss;
    memset(&rss, 0, sizeof(rss));
    rss.req_type = REQ_SERV_SAY;
    strcpy(rss.req_channel, channel);
    strcpy(rss.req_username, username);
    strcpy(rss.req_text, text);
    return rss;
}

struct request_join_s2s s2s_fill_join(char *channel){
    struct request_join_s2s rjs;
    memset(&rjs, 0, sizeof(rjs));
    rjs.req_type = REQ_SERV_JOIN;
    strcpy(rjs.req_channel, channel);
    return rjs;
}

int find_channel_server(Server *srvr, char *channel){
    for (int i = 0; i < srvr->num_chnnls; i++)
    {
        if (strcmp(srvr->sub_channels[i], channel) == 0)
        {
            return i;
        }
        
    }
    return -1;
}

int addr_cmp(struct sockaddr_in a, struct sockaddr_in b){
    return (a.sin_addr.s_addr == b.sin_addr.s_addr) && 
            (a.sin_port == b.sin_port);
}

Server *find_server_address(Server **srvr_list, int list_len, struct sockaddr_in addr){
    for (int i = 0; i < list_len; i++)
    {
        if(addr_cmp(srvr_list[i]->addr, addr)){
            return srvr_list[i];
        }
    }
    return NULL;
    
}

struct text_say fill_text_say(char *channel, char *username, char *text){
    struct text_say ts;
    memset(&ts, 0, sizeof(ts));
    ts.txt_type = TXT_SAY;
    strcpy(ts.txt_channel, channel);
    strcpy(ts.txt_username, username);
    strcpy(ts.txt_text, text);
    return ts;
}

int channel_cmp(void *sub_name, void *ch_name){
    char *subbed_channel = (char*)sub_name; // TODO: Fix this seg fault error(keeps passing a single character)
    char *channel_name = (char *) ch_name;
    return strcmp(subbed_channel, channel_name) == 0;
}

void remove_adj_channel(Server *srvr, char *channel){
    void *ptrs[srvr->num_chnnls];
    void *c_ptrs[srvr->num_chnnls];
    for (int i = 0; i < srvr->num_chnnls; i++)
    {
        ptrs[i] = &(srvr->sub_channels[i]);
        c_ptrs[i] = &(srvr->timers[i]);
    }
    int pos = -1;
    neat_remove(ptrs, &srvr->num_chnnls, channel, channel_cmp, &pos);
    srvr->num_chnnls++;
    neat_remove(c_ptrs, &srvr->num_chnnls, NULL, NULL, &pos);
}

struct request_leave_s2s s2s_fill_leave(char *channel){
    struct request_leave_s2s rls;
    memset(&rls, 0, sizeof(rls));
    rls.req_type = REQ_SERV_LEAVE;
    strcpy(rls.req_channel, channel);
    return rls;
}

unsigned long int gen_rand(){
    srand(time(NULL));
    return (unsigned long int)rand();
}

int has_id(unsigned long int ids[], int ids_len, unsigned long int id){
    for (int i = 0; i < ids_len; i++)
    {
        if (ids[i] == id)
        {
            return 1;
        }
    }
    return 0;
}

int has_channel_servers(Server **srvsr, int len, char *channel){
    int ret = 0;
    for (int i = 0; i < len; i++)
    {
        for (int j = 0; j < srvsr[i]->num_chnnls; j++)
        {
            if (strcmp(srvsr[i]->sub_channels[j], channel) == 0)
            {
                ret++;
                break;
            }
            
        }
        
    }
    return ret;
}

int has_channel_server(Server* srvr, char *channel){
    for (int i = 0; i < srvr->num_chnnls; i++)
    {
        if (strcmp(srvr->sub_channels[i], channel)==0)
        {
            return 1;
        }
        
    }
    return 0;
}

int channel_elapse(Server *srvr, char *channel){
    int pos = find_channel_server(srvr, channel);
    if (pos >= 0)
    {
        clock_t diff = clock() - srvr->timers[pos];
        return TO_SEC(diff);
    }
    return -1;
    
}

void add_ch_srv(Server *srvr, char *channel){
    strcpy(srvr->sub_channels[srvr->num_chnnls], channel);
    srvr->timers[srvr->num_chnnls++] = clock();
}

int remove_ch_srv(Server *srvr, char *channel){
    remove_adj_channel(srvr, channel);
    return 1;
}