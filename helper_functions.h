#ifndef HLPRFUNCS
#define HLPRFUNCS

#include "duckchat.h"
#include "channels.h"

#define REQ_SWITCH 100
#define MAX_JOINED 32

// Show a say user message to stdout
void say_text_output(char *channel, char *username, char *text);

// Decode the users input and return the correct response code
int decode_input(char *inp);

// Get the second argument of the users command
char *get_command_arg(char *buf, int *arg_len);

// Clearout the stdout line
void clear_stdout(int clear_chars);

// List all channels from datagram
void print_channel_list(struct text *inp_t);

// List all joined users in channel
void print_user_list(struct text *inp_t);

// Send a join request
int send_join_req(char *buff, int socketfd, struct sockaddr_in *server_addr, int len, char *active_channel);

// Send a say request
int send_say_req(int socketfd, struct sockaddr_in *s_addr, int len, char *active_channel, char *buff);

// Send a list request
int send_list_req(int socketfd, struct sockaddr_in *s_addr, int len);

// Send a who request
int send_who_req(int socketfd, struct sockaddr_in *s_addr, int len, char *buff);

// Send a leave request
int send_leave_req(int socketfd, struct sockaddr_in *s_addr, int len, char *buff, char *active_channel);

// Send a logout request
int send_logout_req(int socketfd, struct sockaddr_in *s_addr, int len);

// Finds a channel_name within a list of channels
Channel *find_channel(Channel **chnls, int chnls_len, char* channel_name);

// Finds a user by its address in a list of users
User *find_user(User **usrs, int usrs_len, struct sockaddr_in addr, int *position);

// Adds a channel of name chnl_name into a list of channels
Channel *add_chnl(Channel **chnls, int *num_chnls, char *chnl_name);

// Removes a channel from a list of channels
Channel *remove_chnl(Channel **chnls, int *num_chnls, char *chnl_name);

// Given the channel name, return a filled error structure to send
struct text_error fill_error(char *chnl_name);

/** create a sockaddr_in structure based on string address and port
 * @param address the string version of the IP address
 * @param port the string version of the port number
 * @return a sockaddr_in structure filled with the relavent information
*/
struct sockaddr_in create_sockaddr(char *address, char *port);

/**
 * Fill a server to server say request with appropriate data
 * @param channel the name of the channel that this message should be shown in
 * @param username the username of the user that sent the message
 * @param text what the message actually said
 * @return a server to server say request with all data added
*/
request_say_s2s s2s_fill_say(char *channel, char *username, char *text);

/**
 * Fill a server to server join request with appropriate data
 * @param channel the channel name that this server wants to join
 * @return a filled server to server join request
*/
request_join_s2s s2s_fill_join(char *channel);

/**
 * Search and compare to find a channel in a channel list
 * @param channel_list the list of subscribed channels
 * @param list_len the size of the channel list
 * @param channel the name of the channel being looked for
 * @return 1 if the channel is in the channel list, 0 otherwise
*/
int find_channel(char **channel_list, int list_len, char *channel);

#endif