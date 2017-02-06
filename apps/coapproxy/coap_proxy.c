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



#include "coap_proxy.h"

static struct logger* l=NULL;

void init_coap_proxy(){
   l = init_logger(stdout, stderr, stderr);
}

void shutdown_coap_proxy()
{
  shutdown_logger(l);
}



void print_coap_packet_details(char *msg ,int len){

  CoAP_packet *p = new CoAP_packet((uint8_t *)msg,len);
  p->print_coap_packet((const uint8_t*)msg, len);
}


void wait( int seconds )
{  
    #ifdef _WIN32
        Sleep( 1000 * seconds );
    #else
        sleep( seconds );
    #endif
}

int get_resource(char* hostname, char *msg, int len){
  int sockfd,n;
  char buf[BUF_SIZE + 1];
  struct hostent *server;
  int serverlen, tokenLen=0;
  struct sockaddr_in serveraddr;
  int portno = 5683; 
  unsigned char* token;
	
  
  socklen_t slen = sizeof(serveraddr);
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
     log_error(l,"ERROR opening socket");

  server = gethostbyname(hostname);
  if (server == NULL) {
     printf("gethosbyname error\n");
     exit(0);
   }
 
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
  serveraddr.sin_port = htons(portno);

  serverlen = sizeof(serveraddr);
  
  n = sendto(sockfd, msg, len, 0, (sockaddr*) &serveraddr, serverlen);
  if (n < 0) 
     log_error(l,"ERROR in sendto");
  
  n = recvfrom(sockfd, buf,  BUF_SIZE, 0, (sockaddr*) &serveraddr, &slen);
  if (n < 0)
     log_error(l,"ERROR in recvfrom\n");
	
  buf[n] = '\0';

  CoAP_packet *pkt=new CoAP_packet((uint8_t *)buf,n);
  tokenLen=pkt->getTKL();
  token=(unsigned char*)malloc(tokenLen*sizeof(unsigned char));
  memcpy(token, pkt->getToken(), tokenLen);
  token[tokenLen]='\0';
 
  response_to_client(token, tokenLen, (unsigned char*)buf, n); 
  free(token);
  return 0;
}

int on_client_msg(struct client_node* node,  int client_sd, char* hostname,  char *msg, int len)
{ 
  unsigned char *token;  
  int tokenLen=0;
  int hostLen=0, pathLen=0, urilen=0;
  /**< __coappkt from the IP endpoint (client)*/
  CoAP_packet *pkt = new CoAP_packet((uint8_t *)msg,len);
    
  /**< Check if Packet contains  Proxy option*/
  if((pkt->isProxy()) == 1){
      char * proxy_uri;
      int purilen=0;
      proxy_uri=(char*)malloc(sizeof(char));
      pkt->getProxyURI(&proxy_uri,&purilen);	  
      CoAP_packet::CoAP_URI* tmp= pkt->parse(proxy_uri);
      CoAP_packet *coap_request_from_puri = new CoAP_packet();
      coap_request_from_puri->setType((CoAP_packet::CoAP_msg_type)pkt->getType());
      coap_request_from_puri->setTKL(pkt->getTKL());
      coap_request_from_puri->setMethodCode((CoAP_packet::CoAP_method_code)pkt->getCode());	
      coap_request_from_puri->setMessageID(pkt->getMessageID());

      if(pkt->getTKL() !=0){ 
         coap_request_from_puri->setTokenValue(pkt->getToken(), pkt->getTKL());
      }   
      if(tmp->host!=NULL){
         coap_request_from_puri->insertOption(CoAP_packet::COAP_OPTION_URI_HOST, (uint16_t) strlen(tmp->host), (uint8_t*) tmp->host);	
      }
      if(tmp->port!=NULL){
	 unsigned char portbuf[2];
         int port= strtol(tmp->port, NULL, 10);
         coap_request_from_puri->insertOption(CoAP_packet::COAP_OPTION_URI_PORT, coap_request_from_puri->coap_encode_var_bytes(portbuf, port), portbuf);
      } 	
      if(tmp->path!=NULL){
         coap_request_from_puri->insertOption(CoAP_packet::COAP_OPTION_URI_PATH, (uint16_t) strlen(tmp->path), (uint8_t*)tmp->path);
      }    
      if(tmp->query!=NULL){
         coap_request_from_puri->insertOption(CoAP_packet::COAP_OPTION_URI_QUERY, (uint16_t) strlen(tmp->query), (uint8_t*)tmp->query);	
      }
      char *request= (char*) coap_request_from_puri->getCoapPacket();
      int rlen= coap_request_from_puri->getpktLen();
	

      get_resource(tmp->host, request, rlen);
	
      delete(pkt);
      delete(coap_request_from_puri);
      free(proxy_uri);
      free(tmp->scheme);
      free(tmp->host);
      free(tmp->port);
      free(tmp->path);
      free(tmp->query);	
      free(tmp);
	
      
  }else{
      /**< -P option is not set, forward the request to reverse proxy  */
      char* ruri;
      char* host;
      char* path; 
      ruri=(char*)malloc(sizeof(char));
      pkt-> getResourceURIfromOptions(&ruri, &urilen);
      
	
      host=(char*)calloc(1, sizeof(char));
      pkt->getHostfromOptions(&host, &hostLen);
	
      path=(char*)calloc(1,sizeof(char));
      pkt->getPathfromOptions(&path, &pathLen);
   
	/**<Token*/
      CoAP_packet *pkt=new CoAP_packet((uint8_t *)msg,len);
      tokenLen=pkt->getTKL();
      token=(unsigned char*)malloc((tokenLen+1)*sizeof(unsigned char));
      memcpy(token, pkt->getToken(), tokenLen);
      token[tokenLen]='\0';
	
      get_resource(hostname,msg,len);
      
      free(token);
      free(ruri);
      free(host);
      free(path);
   }
 
  return 0;
}

void response_to_client( unsigned char* token,
                         int tokenLen,
                         unsigned char* coapPacket,
                         int coapPacketLen )
{
 
   forward_response(coapPacket, coapPacketLen, token, tokenLen);

}



