#include "duckchat.h"
#include "connection_handler.h"
#include "channels.h"
#include "helper_functions.h"
#include "server_struc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_CONNECTIONS 100
#define PORT 8000

int main(int argc, char **argv){
    if (argc < 2)
    {
        printf("Too few arguments\n");
        exit(EXIT_FAILURE);
    }else if((argc-1)%2 != 0){
        printf("Adrress-port miss match\n");
        exit(EXIT_FAILURE);
    }

    // Get connected servers
    struct sockaddr_in *cnnctd_srvrs = (struct sockaddr_in*)malloc(((argc - 3)/2) * sizeof(struct sockaddr_in));
    struct sockaddr_in **server_list = &cnnctd_srvrs;

    for (int i = 3; i < argc; i+=2)
    {
        inet_pton(AF_INET, argv[i], server_list[(i-3)/2]);
    }
    
    
    struct sockaddr_in client_addr;
    unsigned int client_addrlen = sizeof(client_addr);
    Connection_Handler *ch = create_handler();
    Channel *channels[MAX_USERS];
    channels[0] = create_channel((char*)"Common");
    int num_chnnls = 1;

    User *all_users[MAX_USERS];
    int num_users = 0;
    
    void *receive_line = (void *)malloc(BUFSIZ * sizeof(char));

    memset(&client_addr, 0, sizeof(client_addr));

    if(!ch->init_socket(ch, argv[1], (uint16_t)atoi(argv[2]))){
        ch->destroy(ch);
        exit(EXIT_FAILURE);
    }

    struct request *rq;

    //TODO: Have a list of adjacent servers subscribed channels

    while (1)
    {
        //Receive data from client
        if(ch->socket_recv(ch, receive_line, BUFSIZ, &client_addr, &client_addrlen, MSG_WAITALL) < 0){
            printf("Error with recieving\n");
        }

        rq = (struct request *)receive_line;
        //TODO: see if the incoming message is a server that has a say message for a channel you have. ALso pass this message to any other servers that have the same channel. And send that message to all users in said channel
        if ()
        {
            /* code */
        }
        
        switch (rq->req_type)
        {
        case REQ_LOGIN: { // Login request
            struct request_login *rq_lg = (struct request_login *)rq;
            User *usr;
            usr = channels[0]->create_user(channels[0], rq_lg->req_username, &client_addr, client_addrlen);
            channels[0]->remove_user(channels[0], usr->username);
            all_users[num_users++] = usr;
        }
            break;
        case REQ_SAY:{ // Say request
            //TODO: If this server receives a say message with no other server to forward it to and no users from that channel then respond with a leave message to the sender. (Only if that one server that is the only subscribed server to this channel, send leave)
            //TODO: Add a random unique identifier to a say message
            //TODO: Have a list of recent say identifiers
            //TODO: If the say message already has a identifier and it's in the list of recent identifiers; Discard the say message and send a leave request to the sender
            struct request_say *re_say = (struct request_say *)rq;
            Channel *active_ch = find_channel(channels, num_chnnls,re_say->req_channel);
            if (active_ch == NULL)
            {
                struct text_error chnl_err = fill_error(re_say->req_channel);
                ch->socket_send(ch, &chnl_err, sizeof(chnl_err), &(client_addr));
            }
            
            for (int i = 0; i < active_ch->num_users; i++)
            {
                struct text_say ts;
                memset(&ts, 0, sizeof(ts));
                ts.txt_type = TXT_SAY;
                strcpy(ts.txt_channel, re_say->req_channel);
                strcpy(ts.txt_text, re_say->req_text);
                strcpy(ts.txt_username,
                 active_ch->find_byaddr(active_ch,
                                         client_addr)->username);
                ch->socket_send(ch, (void*)&ts, sizeof(ts), &(active_ch->connected_users[i]->address));
                
            }
        }
        break;
        case REQ_JOIN: { // Join request
            //TODO: Check if the users join channel is already in this servers list of channels. Just add that user in that channel if so
            //TODO: If the users join channel isn't apart of this servers channel list; Send a join message to all adjacent servers
            //TODO: If this server recieves a join request form another server and this server has that channel. Do the regular join and don't send another join message.
            //TODO: If the incoming join message from a channel is not apart of this servers channel list, add that channel and forward the join message.
            //TODO: Renew any channel subscriprions by sending a join message to the channels again to the adjacent servers every minute.
            //TODO: If a server hasn't sent a resubscription to a channel in two minutes that server must leave the channel in this server.
            struct request_join *re_j = (struct request_join*)rq;
            Channel *new_chnl = find_channel(channels, num_chnnls, re_j->req_channel);
            if (new_chnl == NULL)
            {
                new_chnl = add_chnl(channels, &num_chnnls, re_j->req_channel);
                printf("Created a new channel %s\n", new_chnl->chnl_name);
            }
            User *usr = find_user(all_users, num_users, client_addr, NULL); // Get user
            new_chnl->add_user(new_chnl, usr); // Add user to channel
        }break;
        case REQ_LIST:{
            int re_len = sizeof(struct text_list) + (num_chnnls * sizeof(struct channel_info));
            struct text_list *re_l = (struct text_list *)malloc(re_len);
            re_l->txt_type = TXT_LIST;
            re_l->txt_nchannels = num_chnnls;
            for (int i = 0; i < num_chnnls; i++)
            {
                strcpy((re_l->txt_channels + i)->ch_channel, channels[i]->chnl_name);
            }
            ch->socket_send(ch, re_l, re_len, &(client_addr));
            free(re_l);
        }break; 
        case REQ_WHO:{
            struct request_who *re_who = (struct request_who *)rq;
            int who_len = sizeof(struct request_who) + num_users * sizeof(struct user_info);
            struct text_who *who = (struct text_who *)malloc(who_len);
            memset(who, 0, who_len);
            who->txt_type = TXT_WHO; // Set type
            strcpy(who->txt_channel, re_who->req_channel); // Get channel
            Channel *chnl = find_channel(channels, num_chnnls, re_who->req_channel); // Get channel for who req
            if (chnl == NULL)
            {
                free(who);
                printf("Not a real channel\n");
                struct text_error te;
                memset(&te, 0, sizeof(te));
                te.txt_type = TXT_ERROR;
                strcpy(te.txt_error, "Error: Channel \"");
                strcat(te.txt_error, re_who->req_channel);
                strcat(te.txt_error, "\" does not exist");
                ch->socket_send(ch, &te, sizeof(te), &client_addr);
                continue;
            }
            
            who->txt_nusernames = chnl->num_users;
            for (int i = 0; i < chnl->num_users; i++)// copy all usernames
            {
                strcpy((who->txt_users+i)->us_username, chnl->connected_users[i]->username);
            }
            ch->socket_send(ch, who, who_len, &(client_addr));
            free(who);
        }break;
        case REQ_LEAVE:{
            struct request_leave *req_l = (struct request_leave *)rq;
            Channel *chnl = find_channel(channels, num_chnnls,req_l->req_channel);
            if (chnl == NULL)
            {
                // Send Error report
                printf("Not a real channel\n");
                struct text_error terr = fill_error(req_l->req_channel);
                ch->socket_send(ch, &terr, sizeof(terr), &client_addr);
                continue;
            }
            
            User *usr = find_user(all_users, num_users, client_addr, NULL);
            chnl->remove_user(chnl, usr->username);
            if (chnl->num_users == 0)
            {
               remove_chnl(channels, &num_chnnls, chnl->chnl_name);
            }
        }break;
        case REQ_LOGOUT:{
            int u_pos;
            User *usr = find_user(all_users, num_users, client_addr, &u_pos);
            for (int i = 0; i < num_chnnls; i++) // Remove user from channels
            {
                (void*)channels[i]->remove_user(channels[i], usr->username);
            }
            for (int i = u_pos; i < num_users-1; i++) // Move all still connected users down
            {
                all_users[i] = all_users[i+1];
            }
            num_users--;
           free(usr); 
        }break;

        default:
            printf("Not a valid request: %d\n", rq->req_type);
            break;
        }
        
    }
    


    // Free all channels
    for (int i = 0; i < num_chnnls; i++)
    {
        free(channels[i]);
    }
    
    // Free connection handler
    ch->destroy(ch);

    // Free the receive line
    free(receive_line);
    return 0;
}