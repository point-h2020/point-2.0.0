/*
 * hasan02.buet@gmail.com
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <cstring>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <inttypes.h>
#include "coap.hpp"

void print(const uint8_t *buf, size_t buflen){
     while(buflen--)
		printf("%02X%s", *buf++, (buflen > 0) ? " " : "");
        printf("\n");
}


CoAP_packet::CoAP_packet() {
	/**<CoAP Packet*/
	__coappkt = (uint8_t*)calloc(4,sizeof(uint8_t));
	__pktlen= 4;
	__optionCount = 0;
	__maxDelta = 0;
	__data = NULL;
	__dataLen = 0;
	__isProxy = 0;
	__options = NULL;
	setVersion(1);
}
/*!
 * initialize the coap packet from the received packet
 */

CoAP_packet::CoAP_packet(uint8_t *buf, int buflen){

	if(buflen < 4 && buflen!=0) {
		std::cout<< "Packet size should not be less than 4"<< std::endl;
	}
	/**< payload */
	__coappkt = (uint8_t*)calloc(buflen,sizeof(uint8_t));
	memcpy(__coappkt, buf, buflen);
	__pktlen = buflen;

	/**< parse the packet from received and set all the parameters*/
	__optionCount=getOptionCountFromRecv();
	__options = parseOptionsFromRcv();
	
}

CoAP_packet::~CoAP_packet() {
	free(__options);
	free(__coappkt);
}

CoAP_packet::CoAP_options* CoAP_packet::parseOptionsFromRcv(){
	
	uint16_t delta =0, optionNumber = 0, optionValueLength = 0;
	int totalLength = 0, numOptions = 0, optionCount=0;
	
	if(__optionCount==0)
       		optionCount=getOptionCountFromRecv();
	else
        	optionCount= __optionCount;

    	if(optionCount==0) return NULL;  	

	CoAP_options *options = (CoAP_options*) malloc(__optionCount*sizeof(CoAP_options));
	
	if(options==NULL) return NULL;
        /**< first option follows CoAP mandatory header and Token (if any) */
	int optionPos = COAP_HDR_SIZE + getTKL();
	for(int i=0; i<__optionCount; i++) {
		int extendedDelta=0, extendedLen=0;
		delta= (__coappkt[optionPos] & 0xF0) >> 4;
		
		
	    	if(delta==13) {
			extendedDelta=1;
			delta = __coappkt[optionPos+1] + 13;
		
			
		} else if(delta==14) {
                   /**< Handle two-byte value: First, the MSB + 269 is stored as delta value.
		    * After that, the option pointer is advanced to the LSB which is handled
		    * just like case delta == 13. */
		
			extendedDelta=2;
			delta = ((__coappkt[optionPos+1] << 8) | __coappkt[optionPos+2]) + 269;
		
		} 
		
		optionNumber += delta;
		optionValueLength = (__coappkt[optionPos] & 0x0F);
		
		if(optionValueLength==13) {
			extendedLen=1;
			optionValueLength = __coappkt[extendedDelta+optionPos+1] + 13;
		
		} else if(optionValueLength==14){
			extendedLen=2;
			optionValueLength = ((__coappkt[extendedDelta+optionPos+1] << 8) | __coappkt[optionPos+2]) + 269;
		
		} 		
		totalLength=1+extendedDelta+extendedLen+ optionValueLength;
                /**< got an option and record it*/
		if(optionNumber==COAP_OPTION_PROXY_URI){
		  __isProxy = 1;
		} 		
		options[i].delta = delta;
		options[i].optionNumber = optionNumber;	
		options[i].optionValueLength = optionValueLength;
		options[i].optionLength = totalLength;
		options[i].optionValue = &__coappkt[optionPos+totalLength-optionValueLength];

		optionPos += totalLength; 
        numOptions++;
	}
	__optionCount=numOptions;
  
	return options;
}
/*
 * check if Proxy URI found in the coap packet
 */

int CoAP_packet::isProxy(){
  return __isProxy;
}

/*
 * return the hostURI from coap packet
 */

void CoAP_packet::getHostfromOptions(char** host, int* hostLen){

	CoAP_packet::CoAP_options* options = getOptions();
	if(options==NULL) return;
    
    	int len=0;
		
	for(int i=0; i< __optionCount; i++) {
	
		if(options[i].optionNumber == COAP_OPTION_URI_HOST) {
		  len = options[i].optionValueLength;
		  *host=(char*)malloc((len+1)*(sizeof(char)));
		  memcpy(*host, (options+i)->optionValue, len);
		  *(*host + len ) = '\0';
		 	
		}
	}
	*hostLen=len;
}

/*
 * return the Path of the resource from coap packet
 */

void CoAP_packet::getPathfromOptions(char** path, int* pathLen){

	CoAP_packet::CoAP_options* options = getOptions();
	if(options==NULL) return;
    
    	int len=0;
		
	for(int i=0; i< __optionCount; i++) {
	
		if(options[i].optionNumber ==  COAP_OPTION_URI_PATH) {
		  len = options[i].optionValueLength;
		  *path=(char*)malloc((len+1)*(sizeof(char)));
		  memcpy(*path, (options+i)->optionValue, len);
		  *(*path + len ) = '\0';
		 	
		}
	}
	*pathLen=len;
}


void CoAP_packet::getProxyURI(char** proxyURI, int* proxyURILen){

	CoAP_packet::CoAP_options* options = getOptions();
	if(options==NULL) return;
    
    	int urilen=0;
		
	for(int i=0; i< __optionCount; i++) {
	
		if(options[i].optionNumber == COAP_OPTION_PROXY_URI) {
		  urilen = options[i].optionValueLength;
		  *proxyURI=(char*)realloc(*proxyURI, (urilen+1)*(sizeof(char)));
		  memcpy(*proxyURI, (options+i)->optionValue, urilen);
		  *(*proxyURI + urilen) = '\0';
		 	
		}
	}
	*proxyURILen=urilen;
}

int CoAP_packet::getOptionCountFromRecv(){

	uint16_t delta =0, optionNumber = 0, optionValueLength = 0;
	int totalLength = 0;

	
	int optionPos = COAP_HDR_SIZE + getTKL();
    	int parseBytes = __pktlen-optionPos;
	int numOptions = 0;
	
	while(1) {
		
		int extendedDelta=0, extendedLen=0;
		
		if(parseBytes>0) {
			uint8_t optionHeader = __coappkt[optionPos];						
			if(optionHeader==0xFF) {
				std::cout<< "Payload Starts"<< std::endl;
				return 1;
			}			
		} else{
			 __optionCount=numOptions;
			 return numOptions;
		}	  		
		
		delta= (__coappkt[optionPos] & 0xF0) >> 4;
		
	   	 if(delta==13) {
			extendedDelta=1;
			delta = __coappkt[optionPos+1] + 13;
		
		} else if(delta==14) {
			extendedDelta=2;
			delta = ((__coappkt[optionPos+1] << 8) | __coappkt[optionPos+2]) + 269;

		
		} 
		
		optionNumber += delta;
		optionValueLength = (__coappkt[optionPos] & 0x0F);
		
		
		if(optionValueLength==13) {
			extendedLen=1;
			optionValueLength = __coappkt[extendedDelta+optionPos+1] + 13;
			

		} else if(optionValueLength==14) {
		
			extendedLen=2;
			optionValueLength = ((__coappkt[extendedDelta+optionPos+1] << 8) | __coappkt[optionPos+2]) + 269;

			
		} 
		
		totalLength=1+extendedLen+extendedDelta+ optionValueLength;
		
		parseBytes -= totalLength;
		optionPos += totalLength; 
		numOptions++;
	}
    __optionCount = numOptions;
	
	return numOptions;
}
 
int CoAP_packet::getResourceURIfromOptions(char** resultURI, int* len){

	int tmplen=0, queryflag = 1, urilen=0;
	char separator = '/';
	char resURI[50]="";
	
	
	if(__optionCount==0) {
		*resURI = '\0';
		*len = 0;
		return 0;
	}

	CoAP_packet::CoAP_options* options = getOptions();
	
	if(options==NULL) {
		*resURI = '\0';
		*len = 0;
		return 0;
	}
	
	CoAP_options* temp = NULL;
   	
	
	for(int i=0; i<__optionCount; i++) {
		temp = &options[i];
		tmplen = temp->optionValueLength;
		
	    if(temp->optionNumber==COAP_OPTION_URI_PATH||temp->optionNumber==COAP_OPTION_URI_QUERY)
		{
			if(temp->optionNumber==COAP_OPTION_URI_QUERY){
				if(queryflag) {
					*(resURI+strlen(resURI)-1) = '?';
					queryflag = 0;
				}
				separator = '&';
			}
			urilen += tmplen;
			strncat(resURI, (char*) temp->optionValue,tmplen);
			resURI[urilen++] = separator;			
		}
	}
	
	resURI[urilen] ='\0';
	urilen--;
	*len=urilen;
	*resultURI = (char*)realloc(*resultURI, urilen*(sizeof(char)));
	memcpy(*resultURI, resURI, urilen);
	return 1;	
}

uint8_t CoAP_packet::getCode() {
	
	printf("Return the code from the function: %d \n",  __coappkt[1]);
	return __coappkt[1];
}


uint8_t CoAP_packet::getVersion() {
	return (__coappkt[0]&0xC0)>>6;
}


int CoAP_packet::getTKL() {
	return (__coappkt[0] & 0x0F);
}


/*
 * Returns the 16 bit message ID.
 * mesasge ID is stored in network byteorder 
 */ 
uint16_t CoAP_packet::getMessageID() {
	
	uint8_t msb = __coappkt[2];
	uint8_t lsb= __coappkt[3];
	uint16_t messageID = ((uint16_t)msb << 8) | lsb;
	return messageID;
}
/*
 * Print human readable coap options 
 */

CoAP_packet::CoAP_options* CoAP_packet:: getOptions(){
 return __options;
}

/*
 * print options which are found in the coap packet
 */

void CoAP_packet::printOption(){

	CoAP_packet::CoAP_options* options = getOptions();
	if(options==NULL) return;
    
    
		
	for(int i=0; i< __optionCount; i++) {
		std::cout   << " option: " << i+1 << std:: endl
					<< " option number:   " <<  options[i].optionNumber << std:: endl	
					<< " option delta:   " << options[i].delta  << std:: endl
					<< " option name :   "  << std:: endl;
	
		switch(options[i].optionNumber) {
			case COAP_OPTION_IF_MATCH: std::cout   << "IF_MATCH" << std:: endl;
				break;
			case COAP_OPTION_URI_HOST: std::cout   <<"URI_HOST" << std:: endl;
				break;
			case COAP_OPTION_ETAG: std::cout   << "ETAG" << std:: endl;
				break;
			case COAP_OPTION_IF_NONE_MATCH: std::cout   << "IF_NONE_MATCH"<< std:: endl;
				break;
			case COAP_OPTION_OBSERVE: std::cout   << "OBSERVE" << std:: endl;
				break;
			case COAP_OPTION_URI_PORT: std::cout   <<"URI_PORT" << std:: endl;
				break;
			case COAP_OPTION_LOCATION_PATH: std::cout   <<"LOCATION_PATH"<< std:: endl;
				break;
			case COAP_OPTION_URI_PATH: std::cout   <<"URI_PATH"<< std:: endl;
				break;
			case COAP_OPTION_CONTENT_FORMAT: std::cout   <<"CONTENT_FORMAT"<< std:: endl;
				break;
			case COAP_OPTION_MAX_AGE: std::cout   << "MAX_AGE"<< std:: endl;
				break;
			case COAP_OPTION_URI_QUERY: std::cout   << "URI_QUERY"<< std:: endl;
				break;
			case COAP_OPTION_ACCEPT: std::cout   << "ACCEPT"<< std:: endl;
				break;
			case COAP_OPTION_PROXY_URI: std::cout   << "PROXY_URI"<< std:: endl;
				break;
			case COAP_OPTION_PROXY_SCHEME: std::cout   << "PROXY_SCHEME"<< std:: endl;
				break;
			
			default:
				printf("Unknown option");
				break;
		}
		std::cout << "Option lenght: " << options[i].optionValueLength << std::endl;
		printf("Value: ");
		for(int j=0; j<options[i].optionValueLength; j++) {
			char c = options[i].optionValue[j];
			if (options[i].optionNumber == 7) {
				printf("\\%02X ",c);	
				continue;
			}
			
			if(c>='!'&&c<='~') {
				printf("%c",c);
			}
			else printf("\\%02X ",c);			
		}
		printf("\n");
	}
	
}


/*
 * set version in coap packet
 */

int CoAP_packet::setVersion(uint8_t version) {
	if(version>3) return 0;
	__coappkt[0] &= 0x3F;
	__coappkt[0] |= (version << 6);
	return 1;
}

/*
 * get CoAP message type NON, CON, ACK, RESET
 */

CoAP_packet::CoAP_msg_type CoAP_packet::getType() {
	return (CoAP_packet::CoAP_msg_type)(__coappkt[0]&0x30);
}


/*
 * set type (CON, NON, RST, ACK) in coap packet
 */

void CoAP_packet::setType(CoAP_packet::CoAP_msg_type msgType) {
	__coappkt[0] &= 0xCF;
	__coappkt[0] |= msgType;
}

/*
 * set Token lenght in coap packet
 */

int CoAP_packet::setTKL(uint8_t toklen) {
	if(toklen>8) return 1;
	__coappkt[0] &= 0xF0;
	__coappkt[0] |= toklen;
	return 0;
}

/*
 * set message id (network byte order) in coap packet
 */

int CoAP_packet::setMessageID(uint16_t messageID) {
	__coappkt[3] = messageID & 0xFF ;
	__coappkt[2] = (messageID >> 8) & 0xFF;
	
	return 0;
}


/*
 * set response code in coap packet
 */

void CoAP_packet::setResponseCode(CoAP_packet::CoAP_response_code responseCode) {
	__coappkt[1] = responseCode;
}

void CoAP_packet::setMethodCode(CoAP_packet::CoAP_method_code methodCode){
	
	__coappkt[1] = (uint8_t)methodCode;
	printf("setmethodcode:  %02X \n",__coappkt[1]);
}

/*
 * get Token from coap packet
 */

unsigned char* CoAP_packet::getToken() {
	if(getTKL()==0) return NULL;
	return &__coappkt[4];
}

/*
 * set Token in coap packet
 */ 

int CoAP_packet::setTokenValue(uint8_t *token, uint8_t tokenLength){
	if (!token) return 0;
	if (tokenLength > 8)
		return 0;
	/** Token just follows the CoAP fixed header **/
	__pktlen+=tokenLength;
	__coappkt=(uint8_t *)realloc(__coappkt, __pktlen);
	memset(&__coappkt[COAP_HDR_SIZE], 0 , tokenLength*sizeof(uint8_t));
	memcpy((void*)&__coappkt[COAP_HDR_SIZE],token,tokenLength);
	

	return 1;
}



/*
 * add payload into coap packet
 */

int CoAP_packet::addPayload(uint8_t *content, int length){
   assert(content != NULL);
	
   int index=0;
   if(length==0) return 0;
	
    __coappkt=(uint8_t *)realloc(__coappkt, __pktlen+length+1);


    index= COAP_HDR_SIZE + getTKL();
    __coappkt[index++] = COAP_PAYLOAD_START;
   
    __pktlen+=1; 
    memcpy(__coappkt+index, content, length);
    __pktlen+=length;
  
    return 1;
}

/*
 * return the coap packet that is built 
 */

uint8_t* CoAP_packet::getCoapPacket() {
	return __coappkt;
}

/*
 * parse a given uri  
 */
CoAP_packet::CoAP_URI* CoAP_packet::parse(char* uri){

 
	const char *parser_ptr;
	const char *curstr;
        int len=0;

	/* Allocate space for the URI*/
	CoAP_packet::CoAP_URI *uri_comp = (CoAP_packet::CoAP_URI*)calloc(1, sizeof(CoAP_packet::CoAP_URI));
	if (uri_comp==NULL) {
		return NULL;
	}
	uri_comp->scheme = NULL;
	uri_comp->host = NULL;
	uri_comp->port = NULL;
	uri_comp->path = NULL;
	uri_comp->query = NULL;
    
	curstr = uri;

	std::cout << curstr << std:: endl;

	/* Scheme */

	parser_ptr = strchr(curstr, ':');

	if ( parser_ptr == NULL) {  

		parser_ptr = strchr(curstr, '/');   
		//return NULL;
	}else{
		len=parser_ptr-curstr;
		uri_comp->scheme = (char*)malloc((len+1)*sizeof(char));
    
		if(uri_comp->scheme == NULL){
			return NULL;
		}
		strncpy (uri_comp->scheme, curstr, len);	
	   *(uri_comp->scheme + len) = '\0';
	    parser_ptr++; 
        curstr=parser_ptr;
 
    }
	


	while(*curstr == '/') 
		curstr++;
	parser_ptr=curstr;

	
	  
	if(*parser_ptr == '['){
		curstr++;
		while(*parser_ptr == ']') parser_ptr++;
	}else{
	        
		while(*parser_ptr != ':' && *parser_ptr != '/') parser_ptr++;
	}   
	
	len=parser_ptr-curstr;
	uri_comp->host= (char*)malloc((len+1)*sizeof(char));
	strncpy (uri_comp->host, curstr, len);	
	*(uri_comp->host + len) = '\0'; 
  
	curstr=parser_ptr;
	
	/*URI-Port*/
	if(*parser_ptr == ':') {
	    curstr=++parser_ptr;
	    
	    while(*parser_ptr != '/') parser_ptr++;

	    len=parser_ptr-curstr;
	    uri_comp->port= (char*)malloc((len+1)*sizeof(char));
	    strncpy (uri_comp->port, curstr, len);	
	    *(uri_comp->port + len) = '\0'; 
	    parser_ptr++;	
        }else{
	     parser_ptr++;
             uri_comp->port= (char*)malloc(sizeof(char));
	     uri_comp->port= NULL;	
        }
 
	
	  
	/* URI-Path */

	curstr=parser_ptr;

	
	while ( *parser_ptr != '\0' && *parser_ptr != '#' && *parser_ptr != '?') {
        parser_ptr++;
	}
		
	len=parser_ptr-curstr;
	uri_comp->path= (char*)malloc((len+1)*sizeof(char));
	strncpy (uri_comp->path, curstr, len);	
	*(uri_comp->path + len) = '\0'; 
  
		
 	curstr=parser_ptr;
	if ( *parser_ptr != '\0' && *parser_ptr == '?') {      
           parser_ptr++;
	   curstr=parser_ptr;
           while ( *parser_ptr != '\0' && *parser_ptr != '#') parser_ptr++;
	   
          len=parser_ptr-curstr;
	  uri_comp->query= (char*)malloc((len+1)*sizeof(char));
	  strncpy (uri_comp->query, curstr, len);	
	  *(uri_comp->query + len) = '\0'; 
   	}
 
          
	  
	return uri_comp;

	
}

/*
 * return the parsed CoAP_URI 
 */

CoAP_packet::CoAP_URI* CoAP_packet::getParsedURI(){
	return __coapuri;
}


size_t CoAP_packet::coap_option_encode(uint8_t *opt, uint16_t delta, uint8_t optionLength){
 
	size_t index=0;
 	
	if (delta < 13) {
		opt[0] = delta << 4;
	} else if (delta < 270) {
		opt[index++] = 0xd0;
		opt[index] = delta - 13;
	} else {
		opt[index++] = 0xe0;
		opt[index++] = ((delta - 269) >> 8) & 0xff;
		opt[index] = (delta - 269) & 0xff;   
	}
    
	if (optionLength < 13) {
		opt[0] |= optionLength & 0x0f;
	} else if (optionLength < 270) {
		opt[0] |= 0x0d;
		opt[++index] = optionLength - 13;
	} else {
		opt[0] |= 0x0e;
		opt[++index] = ((optionLength - 269) >> 8) & 0xff;
		opt[++index] = (optionLength - 269) & 0xff;    
	}
	
	return index+1;
}

/*
 * insert CoAP options into CoAP packet
 */ 

int CoAP_packet::insertOption(uint16_t optionType, uint16_t optionLength, uint8_t *optionValue) {

	size_t optionSize=0;
	uint8_t* opt;
	int oldlen=__pktlen;
    
	/**Check if options are in the ascending order**/  
	if (optionType < __maxDelta) {
		return 0; 
	} 
	opt= (uint8_t*)calloc(2,sizeof(uint8_t*));
	
	//opt= __coappkt +__pktlen;
	uint16_t delta = optionType-__maxDelta; 

	optionSize= coap_option_encode(opt, delta, optionLength);
	
	__coappkt=(uint8_t *)realloc(__coappkt, __pktlen+optionSize);
	memset(&__coappkt[oldlen], 0 , optionSize);
	memcpy((void*)&__coappkt[oldlen],opt,optionSize);
	
	__pktlen+=optionLength+optionSize;
	
	__coappkt=(uint8_t *)realloc(__coappkt, __pktlen);	
	memset(&__coappkt[oldlen+optionSize], 0 , optionLength);	
	memcpy((void*)&__coappkt[oldlen+optionSize], optionValue, optionLength); 
	
	__maxDelta=optionType;
	free(opt);
	return 0;
}

/*
 * Returns the size of Response type in byte 
*/	
unsigned int CoAP_packet::coap_encode_var_bytes(unsigned char *buf, unsigned int resTypeVal) {
	unsigned int byteCount, i;

	for (byteCount = 0, i = resTypeVal; i && byteCount < sizeof(resTypeVal); ++byteCount)
		i >>= 8;
	i = byteCount;
	while (i>0) {
		buf[i--] = resTypeVal & 0xff;
		resTypeVal >>= 8;
	}

	return byteCount;
}

/*
 *  Returns the length of the PDU.
 */ 
int CoAP_packet::getpktLen() {
	return __pktlen;
}
/*
 * get number of options of the coap packet
 */

int CoAP_packet::getOptionCount() {
	return __optionCount;
}


void CoAP_packet::print_coap_packet(const uint8_t *buf, size_t buflen)
{
    while(buflen--)
		printf("%02X%s", *buf++, (buflen > 0) ? " " : "");
            //printf("%c%s", (unsigned char) *buf++, (buflen > 0) ? " " : "");
        printf("\n");
	
    std::cout<< "Printing coap packet " << std:: endl;	
	std::cout<< "Version: " << (int) getVersion() << std:: endl;
	std::cout<< "Type: " << getType() << std:: endl;
	std::cout<< "Code: " << (uint8_t) getCode() << std:: endl;
	std::cout<< "TKL: " << getTKL() << std:: endl;
	std::cout<< "ID: " << getMessageID() << std:: endl;
	
	
		
	std::cout<< "Option: " << getOptionCount() << std:: endl;
	

}


