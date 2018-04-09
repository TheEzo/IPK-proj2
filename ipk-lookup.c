#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <getopt.h>
#include <unistd.h>

// type of message
#define A 1 // ipv4
#define NS 2
#define CNAME 5
#define PTR 12
#define AAAA 28

#define EXIT_ARGS 2

void reformat_text(char *);

int foo(unsigned char*, char*);
void val(unsigned char*, int*, int);
void types(int, char *);

/* www-inf.int-evry.fr/~hennequi/CoursDNS/NOTES-COURS_eng/msg.html */
struct DNS_MESSAGE{
    short id; // identification // short is 2 bytes long

    // control - according to non-functional code and link 'bit numbering standarts' was each block of 8 bits reversed
    // reason is still unknown, but it works
    char rd : 1; // recursion desired
    char tc : 1; // truncated
    char aa : 1; // authorative answer
    char opcode : 4; // request type
    char qr : 1; // request/response

    char rcode : 4; // error codes
    char cd : 1; // checking disabled
    char ad : 1; // authenticated data
    char zeros : 1; // zeros
    char ra : 1; // recursion available

    // other fields
    unsigned short question_count;
    unsigned short answer_count;
    unsigned short authority_count;
    unsigned short additional_count;
};

/* http://www.zytrax.com/books/dns/ch15/ */
struct QUESTION{
    unsigned short type;
    unsigned short class;
};

struct ANSWER{
	unsigned short type;
	unsigned short class;
	unsigned int ttl;
	unsigned short rdlength;
};

struct TIMEOUT{
    int tv_sec;
    int tv_usec;
};

int main (int argc, char * argv[]) {
	int client_socket, port_number;
    ssize_t bytesrx;
    ssize_t bytestx;
    socklen_t serverlen;
    const char *server_hostname;
    struct hostent *server;

    int c;
    int flag_i = 0;
    char *server_addr = NULL;
    int timeout = 5;
    int type = A;

    struct DNS_MESSAGE *message = NULL;
    struct QUESTION *question = NULL;
    struct ANSWER *answer = NULL;
    char answer_name[512];
    struct TIMEOUT *tv = (struct TIMEOUT*) malloc(sizeof(struct TIMEOUT));

    unsigned char buffer[512];
    struct sockaddr_in server_address;
    char name[512];

    // arguments checking
    while((c = getopt(argc, argv, "hs:T:t:i")) != -1){
        switch(c){
            case 'h':
                // print help...
                exit(0);
            case 's':
                server_addr = (char *) malloc(strlen(optarg) * sizeof(char));
                strcpy(server_addr, optarg);
                break;
            case 'T':
                timeout = atoi(optarg);
                break;
            case 't':
                if(!strcmp(optarg, "A"))
                    type = A;
                else if(!strcmp(optarg, "NS"))
                    type = NS;
                else if(!strcmp(optarg, "CNAME"))
                    type = CNAME;
                else if(!strcmp(optarg, "PTR"))
                    type = PTR;
                else if(!strcmp(optarg, "AAAA"))
                    type = AAAA;
                else{

                    exit(EXIT_ARGS);
                }
                break;
            case 'i':
                flag_i = 1;
                break;
            case '?':
                if (optopt == 's' || optopt == 'T' || optopt == 't')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                exit(EXIT_ARGS);
        }
    }

    // alloc space for name and add one extra byte for reformatting
//    name = (char *)malloc(512 * sizeof(char));
    strcpy(name, "\1"); // insert extra space
    strcat(name, argv[argc-1]);
    char saved_name[strlen(name)];
    strcpy(saved_name, name+1);
    reformat_text(name);


    // map struct DNS_MESSAGE to buffer memory space
    message = (struct DNS_MESSAGE *)&buffer;
    message->id = htons(0); // sending only one request, no identification needed

    message->qr = 0; // request = 0, response = 1
    message->opcode = 0; // standard query?
    message->aa = 0; // data are not authoritative
    message->tc = 0; // data are not truncated
    // recursion/iterative
    if (flag_i){
        message->rd = 0;
        message->ra = 0;
    }
    else{
        message->rd = 1;
        message->ra = 0;
    }

    // rest is used by server in response
    message->zeros = 0;
    message->ad = 0;
    message->cd = 0;
    message->rcode = 0;

    message->question_count = htons(1); // number of questions - only 1 on this example
    message->answer_count = 0;
    message->authority_count = 0;
    message->additional_count = 0;

    // move name to buffer
    int i = sizeof(struct DNS_MESSAGE);
    int j = 0;
    while(j <= strlen(name)){
        buffer[i] = name[j];
        j++;
        i++;
    }
    question = (struct QUESTION *)&buffer[sizeof(struct DNS_MESSAGE) + strlen(name)+1];
    question->type = htons(type);
    question->class = htons(1);

    // initialize server address struct
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(53);
    server_address.sin_addr.s_addr = inet_addr(server_addr);


    // socket creation
	if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) <= 0)
	{
		perror("Socket failed to create");
		exit(EXIT_FAILURE);
	}
	// set timeout
    tv->tv_sec  = timeout;
    tv->tv_usec = 0;
    setsockopt( client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));


    /* odeslani zpravy na server */
    serverlen = sizeof(server_address);
    bytestx = sendto(client_socket, (char *) buffer, sizeof(struct DNS_MESSAGE) + strlen(name) + 1 + sizeof(struct QUESTION), 0, (struct sockaddr *) &server_address, serverlen);
    if (bytestx < 0) {
        perror("ERROR: sendto");
        exit(EXIT_FAILURE);
    }

    /* prijeti odpovedi a jeji vypsani */
    bytesrx = recvfrom(client_socket, (char *) buffer, 512, 0, (struct sockaddr *) &server_address, &serverlen);
    if (bytesrx < 0){
        perror("ERROR: recvfrom");
        exit(EXIT_FAILURE);
    }

    char type_c[6];
    int magic = 2;
    int answer_pos = sizeof(struct DNS_MESSAGE) + strlen(name) + 1 + sizeof(struct QUESTION) + magic;

    answer = (struct ANSWER*)&buffer[answer_pos];
    if(ntohs(answer->type) != A && ntohs(answer->type) != AAAA && ntohs(answer->type) != NS &&
       ntohs(answer->type) != PTR && ntohs(answer->type) != CNAME){

        exit(EXIT_FAILURE);
    }
    int len = 0;
    for(int m = 0; m < ntohs(message->answer_count); m++){
        if(ntohs(answer->type) == CNAME){
            foo(&buffer[answer_pos + sizeof(struct ANSWER)-magic], name);
            types(ntohs(answer->type), &type_c);
            printf("%s IN %s %s\n", saved_name, type_c, name);
        }
        else if (ntohs(answer->type) == A){
            int ip[4];
            val(&buffer[answer_pos + sizeof(struct ANSWER)-magic ], &ip, ntohs(answer->type));
            types(ntohs(answer->type), &type_c);
            printf("%s IN %s %d.%d.%d.%d\n", saved_name, type_c, ip[0], ip[1], ip[2], ip[3]);
            if (type != A && answer->type != type){
                exit(EXIT_FAILURE);
            }
        }
        else {

        }
        if((m+1) < ntohs(message->answer_count)){
            answer_pos += sizeof(struct ANSWER) + ntohs(answer->rdlength);
            answer = (struct ANSWER*)&buffer[answer_pos];
        }
    }
    if(type != ntohs(answer->type)){

        exit(EXIT_FAILURE);
    }

    return 0;
}

void val(unsigned char *buffer, int *ip, int type){
    int i = 0;
    while(i++ < 4){
        ip[i-1] = (int)*buffer;
        buffer++;
    }
}

int foo(unsigned char* buffer, char *name){
    int compressed = 0;
    buffer++;
    int i = 0, j=0,len = 0;
    char old_name[512];
    strcpy(old_name, name+1);
    bzero(name, 512);
    while (*buffer != '\0' && !compressed) {
        if (((int) *buffer) < 192) {
            name[i++] = *buffer;
        } else {
            compressed = 1;

        }
        buffer++;
        len++;
    }

    if(compressed)
        len += 2;
    if (compressed) {
        name[i++] = '.';
        while(old_name[j] != '\0'){
            if(compressed)
                while(old_name[j++] > 'a');
            if(old_name[j] < 'a' || old_name[j] > 'z'){
                if(compressed)
                    compressed = 0;
                else
                    name[i++] = '.';
            }
            else
                name[i++] = old_name[j];
            j++;
        }
    }
    name[j+2] = '\0';

    return len;
}

void types(int type, char *type_c){
    switch(type){
        case A:
            strcpy(type_c, "A");
            break;
        case AAAA:
            strcpy(type_c, "AAAA");
            break;
        case NS:
            strcpy(type_c, "NS");
            break;
        case PTR:
            strcpy(type_c, "PTR");
            break;
        case CNAME:
            strcpy(type_c, "CNAME");
            break;
    }
}
            
/* make some strange format from ulr */
void reformat_text(char *url){
    int i = 0;
    while(i < strlen(url)){
        int count = 1;
        while(url[i+count] != '.'){
            if(url[i+count] == ':')
                break;
            if(i+count >= strlen(url))
                break;
            count++;
        }
        count--;
        url[i] = (char) count;
        i += count + 1;
    }
}