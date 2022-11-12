#include "server_struc.h"
#include "helper_functions.h"
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

/**
 * Checks to see if a server in srvr_list
 * @param address the address of the server to look for
 * @param port the port number, as a string, of the server to look for
 * @param list_len the length of the server list
 * @return 1 if it is in the list, 0 otherwise*/
int in_array(char *address, char *port, struct sockaddr_in **srvr_list, int list_len){
    struct sockaddr_in inp = create_sockaddr(address, port);
    for (int i = 0; i < list_len; i++)
    {
        if (srvr_list[i]->sin_addr.s_addr == inp.sin_addr.s_addr &&
            srvr_list[i]->sin_port == inp.sin_port)
        {
            return 1;
        }
    }
    return 0;
}