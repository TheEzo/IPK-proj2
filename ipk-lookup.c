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

void reformat_text(char *);

/* www-inf.int-evry.fr/~hennequi/CoursDNS/NOTES-COURS_eng/msg.html */
struct HEADER{
    short id; // identification // short is 2 bytes long

    // control - according to non-functional code and link 'bit numbering standarts' was each block of 8 bits reversed
    // reason is still unknown, but it works
    unsigned char rd : 1; // recursion desired
    unsigned char tc : 1; // truncated
    unsigned char aa : 1; // authorative answer
    unsigned char opcode : 4; // request type
    unsigned char qr : 1; // request/response

    unsigned char rcode : 4; // error codes
    unsigned char cd : 1; // checking disabled
    unsigned char ad : 1; // authenticated data
    unsigned char zeros : 1; // zeros
    unsigned char ra : 1; // recursion available

    // other fields
    short question_count;
    short answer_count;
    short authority_count;
    short additional_count;
};

/* http://www.zytrax.com/books/dns/ch15/ */
struct QUESTION{
    short type;
    short class;
};

struct ANSWER{
	unsigned short name;
	unsigned short type;
	unsigned short class;
	unsigned long ttl;
	unsigned short rdlength;
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
    char *type = NULL;

    struct HEADER *header = NULL;
    struct QUESTION *question = NULL;
    struct ANSWER *answer = NULL;

    unsigned char buffer[65536];
    struct sockaddr_in server_address;
    char *name = NULL;

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
                type = (char *) malloc(strlen(optarg) * sizeof(char));
                break;
            case 'i':
                flag_i = 1;
                break;
            case '?':
                if (optopt == 's' || optopt == 'T' || optopt == 't')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    // alloc space for name and add one extra byte for reformatting
    name = (char *) malloc(strlen(argv[argc-1]) * sizeof(char) + 1);
    strcpy(name, "\1"); // insert extra space
    strcat(name, argv[argc-1]);
    reformat_text(name);


    // map struct header to buffer memory space
    header = (struct HEADER *)&buffer;
    header->id = htons(0); // sending only one request, no identification needed

    header->qr = 0; // request = 0, response = 1
    header->opcode = 0; // standard query?
    header->aa = 0; // data are not authoritative
    header->tc = 0; // data are not truncated
    // recursion/iterative
    if (flag_i){
        header->rd = 0;
        header->ra = 0;
    }
    else{
        header->rd = 1;
        header->ra = 0;
    }

    // rest is used by server in response
    header->zeros = 0;
    header->ad = 0;
    header->cd = 0;
    header->rcode = 0;

    header->question_count = htons(1); // number of questions - only 1 on this example
    header->answer_count = 0;
    header->authority_count = 0;
    header->additional_count = 0;

    // move name to buffer
    int i = sizeof(struct HEADER);
    int j = 0;
    while(j <= strlen(name)){
        buffer[i] = name[j];
        j++;
        i++;
    }
    question = (struct QUESTION *)&buffer[sizeof(struct HEADER) + strlen(name)+1];
    question->type = htons(A);
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

    /* odeslani zpravy na server */
    serverlen = sizeof(server_address);
    bytestx = sendto(client_socket, (char *) buffer, sizeof(struct HEADER) + strlen(name)+1 + sizeof(struct QUESTION), 0, (struct sockaddr *) &server_address, serverlen);
    if (bytestx < 0) 
      perror("ERROR: sendto");
    
    /* prijeti odpovedi a jeji vypsani */
    bytesrx = recvfrom(client_socket, (char *) buffer, 65536, 0, (struct sockaddr *) &server_address, &serverlen);
    if (bytesrx < 0) 
      perror("ERROR: recvfrom");

  	answer = (struct ANSWER*)&buffer[sizeof(struct HEADER) + strlen(name)+1 + sizeof(struct QUESTION)];
  	printf("%d\n", answer->name);


    printf("Echo from server: %s", buffer);
    return 0;
}

void reformat_text(char *url){
    int i = 0;
    while(i < strlen(url)){
        int count = 1;
        while(url[i+count] != '.'){
            if(i+count >= strlen(url))
                break;
            count++;
        }
        count--;
        url[i] = (char) count;
        i += count + 1;
    }
}