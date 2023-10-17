#include "hw1.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
#define BUFFER_SIZE 512

#define DIVIDER "==============================\n"
#define WELCOME "Welcome to CSIE Bulletin board\n"


record record_buf;

typedef struct {
    char* ip; // server's ip
    unsigned short port; // server's port
    int conn_fd; // fd to talk with server
    char buf[BUFFER_SIZE]; // data sent by/to server
    size_t buf_len; // bytes used by buf
} client;

client cli;
static void init_client(char** argv);

bool is_trash(record a)
{
    for (int i = 0; i < FROM_LEN; i++){
        if (a.From[i] != '\0') return 0;
    }
    for (int i = 0; i < CONTENT_LEN; i++){
        if (a.Content[i] != '\0') return 0;
    }
    return 1;
}


void set_trash(record *rd)
{
	int from_len = strlen(rd->From), content_len = strlen(rd->Content);
	for (int i = 0; i < FROM_LEN; i++){
		rd->From[i] = '\0';
	}
	for (int i = 0; i < CONTENT_LEN; i++){
		rd->Content[i] = '\0';
	}
	return;
}

void post()
{	
	recv(cli.conn_fd, cli.buf, sizeof(record), 0); // receive trash or not
    memcpy(&record_buf, cli.buf, sizeof(record));
	if (is_trash(record_buf)){
        fprintf(stderr, "Recv: %s AND %s\n", record_buf.From, record_buf.Content);
		fprintf(stdout, "[Error] Maximum posting limit exceeded\n");
		fflush(stdout);
		return;
	}
	set_trash(&record_buf);
	fprintf(stdout, "FROM: ");
	fflush(stdout);
    scanf("%s", record_buf.From);
	fprintf(stdout,"CONTENT:\n");	
	fflush(stdout);
    scanf("%s", record_buf.Content);
    memcpy(cli.buf, &record_buf, sizeof(record));
    send(cli.conn_fd, cli.buf, sizeof(record), 0);
	// printf("cli.buf:i\n");
	return;
}

void pull()
{
	
	fprintf(stdout, DIVIDER);
	fflush(stdout);
    while (1){
		set_trash(&record_buf);
        recv(cli.conn_fd, cli.buf, sizeof(record), 0);
		memcpy(&record_buf, cli.buf, sizeof(record));
        if (is_trash(record_buf)){ // receive trash
            break; 
        }
		fprintf(stdout, "FROM: %s\nCONTENT:\n%s\n", record_buf.From, record_buf.Content);
		fflush(stdout);
    }
	fprintf(stdout, DIVIDER);
	fflush(stdout);
    return;
}

void Exit()
{
	exit(1);
    return;
}

void init_welcome()
{
	fprintf(stdout, "%s%s", DIVIDER, WELCOME);
	fflush(stdout);
    strncpy(cli.buf, "pull", 4);
    send(cli.conn_fd, cli.buf, sizeof(cli.buf), 0);
    pull();
    return;
}


int main(int argc, char** argv){
    
    // Parse args.
    if(argc!=3){
        ERR_EXIT("usage: [ip] [port]");
    }

    // Handling connection
    init_client(argv);
    fprintf(stderr, "connect to %s %d\n", cli.ip, cli.port);
    init_welcome();

    char cmd[BUFFER_SIZE];
    while(1){
        // TODO: handle user's input
		fprintf(stdout, "Please enter your command (post/pull/exit): ");
		fflush(stdout);
        scanf("%s", cmd);
        memcpy(cli.buf, cmd, sizeof(cli.buf));
        send(cli.conn_fd,  cmd, sizeof(cmd), 0);
        if (!strncmp(cmd, "post", 4)) post();
        else if (!strncmp(cmd, "pull", 4)) pull();
        else if (!strncmp(cmd, "exit", 4)) Exit();
    }
}
static void init_client(char** argv){
    
    cli.ip = argv[1];

    if(atoi(argv[2])==0 || atoi(argv[2])>65536){
        ERR_EXIT("Invalid port");
    }
    cli.port=(unsigned short)atoi(argv[2]);

    struct sockaddr_in servaddr;
    cli.conn_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(cli.conn_fd<0){
        ERR_EXIT("socket");
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(cli.port);

    if(inet_pton(AF_INET, cli.ip, &servaddr.sin_addr)<=0){
        ERR_EXIT("Invalid IP");
    }

    if(connect(cli.conn_fd, (struct sockaddr*)&servaddr, sizeof(servaddr))<0){
        ERR_EXIT("connect");
    }

    return;
}
