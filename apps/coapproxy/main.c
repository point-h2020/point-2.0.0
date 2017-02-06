/*
 * coap_proxy.h
 *
 *  Author: Hasan Mahmood Aminul Islam <hasan02.buet@gmail.com>
 *
 * This file is part of Blackadder.
 *
 * Blackadder is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Blackadder is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Blackadder.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <poll.h>

#include "coap_proxy.h"

static struct logger* l=NULL;

void usage(){

  printf("\n Usage: proxy -i <proxy_address> -s <client subscribe port>\n\n");
}

int parse_parameters(int argc, char *argv[],
                     char* hostname, in_port_t* client_port)
{
  int c = 0;

  while((c = getopt(argc,argv,"hi:p:s:")) != -1){
    switch(c){
    case 'i':
      if( strlen(optarg) < HOSTNAME_SIZE ) 
        strcpy(hostname, optarg);
      else{
        log_error(l, "Error: given hostname/ip_address is too long.\n");
        return -1;
      }
      break;
    case 's':
      if( (*client_port=atoi(optarg)) == 0 ){
        log_error(l, "Error: invalid client subscribe port.\n");
        return -1;
      }
      break;

    case 'h':
    default:
      usage();
      return -1;
      break;
    }
  }

  return 0;
}

void init_systems(){

  l = init_logger(stdout, stderr, stderr);
  init_clients();
  init_coap_proxy();
}

void shutdown_systems(){

  shutdown_logger(l);
  shutdown_clients();
  shutdown_coap_proxy();	
 }

void signal_handler( int signum ){
  log_print(l, "Shutdown.\n");
  shutdown_systems();
  exit(EXIT_SUCCESS);
}

int server_loop(int client_sd, char* hostname){

  struct pollfd fds[1];
  int timeout_msecs = RETRANSMIT_TIMEOUT;
  int ret = 0;
  struct client_node* client = NULL;
  char* recvMsg;
  int  len;	

  fds[0].fd = client_sd;
  fds[0].events = POLLIN;
 

  for(;;)
  {
    ret = poll( fds, 1, timeout_msecs );
    /**< libcoap client : receive coap packet and invoke NAP method;*/	
    if( fds[0].revents & POLLIN ) {
      printf("Here \n");	
      recvMsg=NULL;
      read_client(client_sd, &client, &recvMsg, &len);
      if( on_client_msg(client, client_sd, hostname, recvMsg, len) == -1 ){
         log_error(l, "Error constructing a response for the client.\n");
         free(recvMsg);	          
      }
    }
   
    if( ret == -1 ) {
      log_error(l, "Poll errors.\n");	
      exit(EXIT_FAILURE);
    }
  }
  
}

int main(int argc, char *argv[]){
  char hostname[HOSTNAME_SIZE] = "localhost";
  in_port_t client_port = 5683;
  int client_sd=-1;
 
  init_systems();
 
  if( signal(SIGINT, signal_handler) == SIG_IGN )
    signal(SIGINT, SIG_IGN);
 
  if( parse_parameters(argc, argv, hostname, &client_port) < 0 ) exit(EXIT_FAILURE);

  log_print(l, "Will bind to: %s.\n", hostname);
  log_print(l, "Port for client subscriptions: %u.\n", client_port);

  client_sd = client_socket(hostname, client_port);
  if( client_sd == -1 ) exit(EXIT_FAILURE);

  server_loop(client_sd, hostname);

  close(client_sd);
  shutdown_systems();
  return EXIT_SUCCESS;
}
