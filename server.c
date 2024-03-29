#include "duckchat.h"
#include "connection_handler.h"
#include "channels.h"
#include "helper_functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define MAX_CONNECTIONS 100
#define MAX_IDS 1024
#define MIN_2 120
#define MIN_1 60

int main(int argc, char **argv){
    if (argc < 2)
    {
        printf("server <address> <port> [<address> <port> ...]\n");
        exit(EXIT_FAILURE);
    }else if((argc-1)%2 != 0){
        printf("Adrress-port miss match\n");
        exit(EXIT_FAILURE);
    }

    uint16_t my_port = (uint16_t)atoi(argv[2]);

    // Get connected servers
    int num_servers = (argc-3)/2;
    Server *srvs = (Server *)malloc(sizeof(Server) * num_servers);
    Server *cnnct_srvrs[num_servers];
    for (int i = 0; i < num_servers; i++)
    {
        cnnct_srvrs[i] = &(srvs[i]);
    }
    
    
    for (int i = 3; i < argc; i+=2) // Set up adjactent servers
    {
        cnnct_srvrs[(i-3)/2]->addr = create_sockaddr(argv[i], argv[i+1]);
        memset(cnnct_srvrs[(i-3)/2]->sub_channels, 0, MAX_CHANNELS * CHANNEL_MAX *sizeof(char));
        strcpy(cnnct_srvrs[(i-3)/2]->sub_channels[0], "Common");
        cnnct_srvrs[(i-3)/2]->num_chnnls = 1;
    }
    
    //Recently seen say IDs
    unsigned long int recent_ids[MAX_IDS];
    memset(recent_ids, 0, sizeof(unsigned long int) * MAX_IDS);
    int num_ids = 0;

    struct sockaddr_in client_addr;
    unsigned int client_addrlen = sizeof(client_addr);
    Connection_Handler *ch = create_handler(); // Create connection hanlder
    Channel *channels[MAX_USERS];
    channels[0] = create_channel((char*)"Common");
    int num_chnnls = 1;

    User *all_users[MAX_USERS];
    int num_users = 0;
    
    void *receive_line = (void *)malloc(BUFSIZ * sizeof(char));

    memset(&client_addr, 0, sizeof(client_addr));


    if(!ch->init_socket(ch, argv[1], my_port)){
        ch->destroy(ch);
        exit(EXIT_FAILURE);
    }

    struct request *rq;

    while (1)
    {
        ch->non_block(ch);
        //Check late timers
        for (int i = 0; i < num_servers; i++)
        {
            for (int j = 1; j < cnnct_srvrs[i]->num_chnnls; j++)
            {
                if (channel_elapse(cnnct_srvrs[i], cnnct_srvrs[i]->sub_channels[j]) > MIN_2)
                {
                    printf("%s:%d %s:%d Timed out channel %s\n",
                            ch->get_addr(ch), 
                            my_port, 
                            inet_ntoa(cnnct_srvrs[i]->addr.sin_addr), 
                            ntohs(cnnct_srvrs[i]->addr.sin_port),
                            cnnct_srvrs[i]->sub_channels[j]);
                    remove_adj_channel(cnnct_srvrs[i], cnnct_srvrs[i]->sub_channels[j]);
                    
                }
                
            }
            
        }
        
        //Receive data from client
        if(ch->socket_recv(ch, receive_line, BUFSIZ, &client_addr, &client_addrlen, MSG_WAITALL) < 0){
            goto resub;
        }

        rq = (struct request *)receive_line;

        switch (rq->req_type)
        {
        case REQ_LOGIN: { // Login request
            struct request_login *rq_lg = (struct request_login *)rq;
            User *usr;
            usr = channels[0]->create_user(channels[0], rq_lg->req_username, &client_addr, client_addrlen);
            channels[0]->remove_user(channels[0], usr->username);
            all_users[num_users++] = usr;
        }break;
        case REQ_SAY:{ // Say request
            struct request_say *re_say = (struct request_say *)rq;
            Channel *active_ch = find_channel(channels, num_chnnls,re_say->req_channel);
            if (active_ch == NULL)
            {
                struct text_error chnl_err = fill_error(re_say->req_channel);
                ch->socket_send(ch, &chnl_err, sizeof(chnl_err), &(client_addr));
            }
            
            for (int i = 0; i < active_ch->num_users; i++)
            {//send message to all users
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

            //Send to other servers
            User * usr = find_user(all_users, num_users, client_addr, NULL); // Get the username
            struct request_say_s2s rss = s2s_fill_say(re_say->req_channel, usr->username, re_say->req_text); // Fill s2s data
            unsigned long int tmp_id = gen_rand();
            rss.id = tmp_id;
            recent_ids[num_ids++ % MAX_IDS] = tmp_id;

            for (int i = 0; i < num_servers; i++)
            {
                printf("%s:%d %s:%d send S2S Say %s %s \"%s\"\n",
                        ch->get_addr(ch), 
                        ch->get_port(ch), 
                        inet_ntoa(cnnct_srvrs[i]->addr.sin_addr), 
                        htons(cnnct_srvrs[i]->addr.sin_port), 
                        rss.req_username,
                        rss.req_channel,
                        rss.req_text);
                if(find_channel_server(cnnct_srvrs[i], re_say->req_channel) >= 0){
                    ch->socket_send(ch, (void*)&rss, sizeof(rss), &(cnnct_srvrs[i]->addr));
                }
            }
            
        }
        break;
        case REQ_JOIN: { // Join request
            struct request_join *re_j = (struct request_join*)rq;
            printf("%s:%d recv Request Join %s\n", ch->get_addr(ch), my_port, re_j->req_channel);
            Channel *new_chnl = find_channel(channels, num_chnnls, re_j->req_channel);

            struct request_join_s2s rjs = s2s_fill_join(re_j->req_channel); // Send new channel to adjacent channels
            for (int i = 0; i < num_servers; i++)
            {
                printf("%s:%d %s:%d send S2S Join %s\n", 
                        ch->get_addr(ch),
                        ch->get_port(ch), 
                        inet_ntoa(client_addr.sin_addr), 
                        ntohs(client_addr.sin_port),
                        re_j->req_channel);
                if(ch->socket_send(ch, &rjs, sizeof(rjs), &(cnnct_srvrs[i]->addr)) <= 0){
                    printf("%d: Could not send s2s join request\n", my_port);
                }
            }

            if (new_chnl == NULL)
            {                
                new_chnl = add_chnl(channels, &num_chnnls, re_j->req_channel);
                printf("%s:%d Created a new channel %s\n", ch->get_addr(ch), my_port, new_chnl->chnl_name);
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
                printf("%s:%d \"%s\" is not a real channel\n", ch->get_addr(ch), my_port, re_who->req_channel);
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
            printf("%s:%d recv Request Leave %s\n",
                    ch->get_addr(ch),
                    ch->get_port(ch),
                    req_l->req_channel);
            Channel *chnl = find_channel(channels, num_chnnls,req_l->req_channel);
            if (chnl == NULL)
            {
                // Send Error report
                printf("%s:%d \"%s\" not a real channel\n", ch->get_addr(ch), my_port, req_l->req_channel);
                struct text_error terr = fill_error(req_l->req_channel);
                ch->socket_send(ch, &terr, sizeof(terr), &client_addr);
                continue;
            }
            
            User *usr = find_user(all_users, num_users, client_addr, NULL);
            chnl->remove_user(chnl, usr->username);
            if (chnl->num_users == 0)
            { // Removing a channel    
                struct request_leave_s2s rls = s2s_fill_leave(chnl->chnl_name);
                for (int i = 0; i < num_servers; i++)
                { // Send leave requests to all adjacent servers
                    printf("%s:%d %s:%d send S2S Leave %s\n", 
                        ch->get_addr(ch), 
                        ch->get_port(ch), 
                        inet_ntoa(cnnct_srvrs[i]->addr.sin_addr), 
                        htons(cnnct_srvrs[i]->addr.sin_port),
                        rls.req_channel);
                    ch->socket_send(ch, &rls, sizeof(rls), &(cnnct_srvrs[i]->addr));
                }

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
        case REQ_SERV_JOIN:{
            struct request_join_s2s * rjs = (struct request_join_s2s *)rq;
            Channel *active_ch;
            Server *srvr_update = find_server_address(cnnct_srvrs, num_servers, client_addr);
            if (!has_channel_server(srvr_update, rjs->req_channel))
            { // Add channel to corresponding server
                add_ch_srv(srvr_update, rjs->req_channel);
                printf("%s:%d %s:%d recv S2S Join %s\n", ch->get_addr(ch), my_port, inet_ntoa(srvr_update->addr.sin_addr), ntohs(srvr_update->addr.sin_port), rjs->req_channel);
            }else{ // Resubscribe channel
                printf("%s:%d %s:%d recv S2S Join %s\n", ch->get_addr(ch), my_port, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), rjs->req_channel);
                int pos = find_channel_server(srvr_update, rjs->req_channel);
                srvr_update->timers[pos] = time(NULL);
            }

            if ((active_ch = find_channel(channels, num_chnnls, rjs->req_channel)) == NULL){ // If this server doesn't already the channel
                add_chnl(channels, &num_chnnls, rjs->req_channel);
                struct request_join_s2s nrjs = s2s_fill_join(rjs->req_channel);
                for (int i = 0; i < num_servers; i++)
                {
                    printf("%s:%d %s:%d send S2S Join %s\n", 
                            ch->get_addr(ch), 
                            my_port, 
                            inet_ntoa(cnnct_srvrs[i]->addr.sin_addr), 
                            htons(cnnct_srvrs[i]->addr.sin_port), 
                            rjs->req_channel);
                    ch->socket_send(ch, &nrjs, sizeof(nrjs), &(cnnct_srvrs[i]->addr));
                }
                
            }

        }break;
        case REQ_SERV_LEAVE:{
            struct request_leave_s2s *rls = (struct request_leave_s2s*)rq;
            Server *srv = find_server_address(cnnct_srvrs, num_servers, client_addr);
            remove_adj_channel(srv, rls->req_channel);
            printf("%s:%d %s:%d recv S2S Leave %s\n", 
                    ch->get_addr(ch), my_port, 
                    inet_ntoa(client_addr.sin_addr), 
                    ntohs(client_addr.sin_port),
                    rls->req_channel);
        }break;
        case REQ_SERV_SAY:{

            struct request_say_s2s *sss = (struct request_say_s2s*)rq;
            printf("%s:%d %s:%d send S2S Say %s %s \"%s\"\n",
                        ch->get_addr(ch), 
                        ch->get_port(ch), 
                        inet_ntoa(client_addr.sin_addr), 
                        htons(client_addr.sin_port), 
                        sss->req_username,
                        sss->req_channel,
                        sss->req_text);
            if (has_id(recent_ids, num_ids, sss->id))
            {
                struct request_leave_s2s rls = s2s_fill_leave(sss->req_channel);
                printf("%s:%d %s:%d send S2S Leave %s\n", 
                        ch->get_addr(ch), 
                        ch->get_port(ch), 
                        inet_ntoa(client_addr.sin_addr), 
                        htons(client_addr.sin_port),
                        sss->req_channel);
                ch->socket_send(ch, &rls, sizeof(rls), &client_addr); // leave the sends channel list
                break; // do nothing else
            }
            recent_ids[num_ids++ % MAX_IDS] = sss->id;
            
            int subbed;
            Channel *active = find_channel(channels, num_chnnls, sss->req_channel);

            if (active->num_users == 0 && has_channel_servers(cnnct_srvrs, num_servers, sss->req_channel) <= 1)
            {// There's no one to forward this message to
                printf("%s:%d %s:%d send S2S Leave %s\n", 
                        ch->get_addr(ch), 
                        ch->get_port(ch), 
                        inet_ntoa(client_addr.sin_addr), 
                        htons(client_addr.sin_port),
                        sss->req_channel);
                struct request_leave_s2s rls = s2s_fill_leave(sss->req_channel);
                ch->socket_send(ch, &rls, sizeof(rls), &client_addr);
                break;
            }

            struct text_say ts = fill_text_say(sss->req_channel, sss->req_username, sss->req_text);
            // Sending to users
            for (int i = 0; i < active->num_users; i++)
            {
                ch->socket_send(ch, &ts, sizeof(ts), &(active->connected_users[i]->address));
            }

            // Sending to other servers
            for (int i = 0; i < num_servers; i++)
            {
                if ((subbed = find_channel_server(cnnct_srvrs[i], sss->req_channel)) >= 0 && !addr_cmp(cnnct_srvrs[i]->addr, client_addr))
                {
                    printf("%s:%d %s:%d send S2S Say %s %s \"%s\"\n", 
                            ch->get_addr(ch), 
                            ch->get_port(ch), 
                            inet_ntoa(cnnct_srvrs[i]->addr.sin_addr), 
                            htons(cnnct_srvrs[i]->addr.sin_port), 
                            sss->req_username,
                            sss->req_channel,
                            sss->req_text);
                    ch->socket_send(ch, sss, sizeof(*sss), &(cnnct_srvrs[i]->addr));
                }

            }

        }break;

        default:
            printf("%s:%d Not a valid request: %d\n", ch->get_addr(ch), my_port, rq->req_type);
            break;
        }
        
        resub:
        struct request_join_s2s rjs;
        for (int i = 1; i < num_chnnls; i++)
        {
            if (time(NULL) - channels[i]->tm > MIN_1)
            {
                for (int j = 0; j < num_servers; j++)
                {
                    rjs = s2s_fill_join(channels[i]->chnl_name);
                    printf("%s:%d %s:%d send S2S Join %s\n", 
                            ch->get_addr(ch),
                            ch->get_port(ch), 
                            inet_ntoa(cnnct_srvrs[i]->addr.sin_addr), 
                            ntohs(cnnct_srvrs[i]->addr.sin_port),
                            channels[i]->chnl_name);
                    ch->socket_send(ch, &rjs, sizeof(rjs), &(cnnct_srvrs[j]->addr));
                }
                channels[i]->tm = time(NULL);
            }
            
            
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