#include "hw1.h"
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/file.h>
#include <assert.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
#define BUFFER_SIZE 512
// #define MAXSTR 10

int last = 0;
record buf_record, trash, noTrash;

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[BUFFER_SIZE];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    int id;
    bool is_lock;
    int post_pos;
    struct flock wlock;
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

bool is_lock_arr[RECORD_NUM] = {};

// initailize a server, exit for error
static void init_server(unsigned short port);

// initailize a request instance
static void init_request(request* reqP);

// free resources used by a request instance
static void free_request(request* reqP, int fd);



bool have_lock(int check_pos, int board_fd)
{
    struct flock lock_info;
    lock_info.l_type = F_WRLCK; // Set the type of lock you want to check (e.g., F_RDLCK or F_WRLCK)
    lock_info.l_whence = SEEK_SET;
    lock_info.l_start = check_pos * sizeof(record); // Set the start position of the segment
    lock_info.l_len = sizeof(record); // Set the length of the segment
    int err = fcntl(board_fd, F_GETLK, &lock_info);
    // printf("boolerr:%d\n", err);
    if (lock_info.l_type == F_UNLCK) return 0;
    else return 1;

}

int set_lock(int lock_start, int board_fd, int now_fd, short lock_type) // for post:stage1
{
    // if (lock_start == -1) return;
    requestP[now_fd].wlock.l_type = lock_type, requestP[now_fd].wlock.l_whence = SEEK_SET, requestP[now_fd].wlock.l_start = lock_start * sizeof(record), requestP[now_fd].wlock.l_len = sizeof(record);
    // rlock.l_type = F_RDLCK, rlock.l_whence = SEEK_SET, rlock.l_start = lock_start * sizeof(record), rlock.l_len = sizeof(record);
    return fcntl(board_fd, F_SETLK, &(requestP[now_fd].wlock));
}

void set_trash_and_noTrash()
{
	for (int i = 0; i < FROM_LEN; i++){
		trash.From[i] = '\0';
		noTrash.From[i] = 'a';
	}
	for (int i = 0; i < CONTENT_LEN; i++){
		trash.Content[i] = '\0';
		noTrash.Content[i] = 'a';
	}
	return;
}

void set_trash(record *rd)
{
	for (int i = 0; i < FROM_LEN; i++){
		rd->From[i] = '\0';
	}
	for (int i = 0; i < CONTENT_LEN; i++){
		rd->Content[i] = '\0';
	}
	return;
}

int main(int argc, char** argv) {
    // system("rm BulletinBoard"); // remember to remove !!!!
    int board_fd = open(RECORD_PATH,  O_RDWR | O_CREAT);
	set_trash_and_noTrash();

    // Parse args.
    if (argc != 2) {
        ERR_EXIT("usage: [port]");
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[BUFFER_SIZE];
    int buf_len;

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);
    // printf("server listen fd:%d\n", svr.listen_fd);

    fd_set readfds;  
    FD_ZERO(&readfds);
    FD_SET(svr.listen_fd, &readfds);

    struct timeval timeout;
    while (1) {
        // TODO: Add IO multiplexing
        timeout.tv_sec = 0;  // Adjust the timeout as needed
        timeout.tv_usec = 300000;
        fd_set tmpfds = readfds;  // Copy the set as `select` will modify it

        // Use `select` to wait for act_num on any of the monitored file descriptors
        // printf("selecting!!\n");
        int act_num = select(maxfd, &tmpfds, NULL, NULL, &timeout);
        // printf("maxfd:%d\n", maxfd);

        if (act_num == -1) {
            perror("select");
            exit(1);
        }

        // Check new connection
        clilen = sizeof(cliaddr);
        // printf("act:%d\n", act_num);
        if (act_num == 0) continue;

        int count = 0;
        for (int fd = 0; fd < maxfd; fd++){
            if (count == act_num) break;
            if (FD_ISSET(fd, &tmpfds)){ // 有動作的fd
                // printf("now processing %d\n", fd);
                if (fd == svr.listen_fd){ // new connection
                    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
                    if (conn_fd < 0) {
                        if (errno == EINTR || errno == EAGAIN) continue;  // try again
                        if (errno == ENFILE) {
                            (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                            continue;
                        }
                        ERR_EXIT("accept");
                    }
                    requestP[conn_fd].conn_fd = conn_fd;
                    strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                    fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
                    FD_SET(conn_fd, &readfds); // 將conn_fd加入偵測行列
                    continue;
				}
				else{ // TODO: handle requests from clients
					int now_fd = requestP[fd].conn_fd;
					if (requestP[now_fd].is_lock){ // handle FROM and CONTENT
						set_trash(&buf_record);
						assert(recv(now_fd, requestP[now_fd].buf, sizeof(record), 0));
                        memcpy(&buf_record, requestP[now_fd].buf, sizeof(record));
						// printf("wait for post's content!\n");
						// printf("now_fd:%d\n", now_fd);
						// printf("receive sucess!\n");
						assert(pwrite(board_fd, &(buf_record), sizeof(record), requestP[now_fd].post_pos * sizeof(record)) != -1);
						// record tmp;
						// pread(board_fd, &tmp, sizeof(record), requestP[now_fd].post_pos * sizeof(record));
						// printf("tmp_from:%s tmp_content:%s\n", tmp.From, tmp.Content);
						// printf("pos:%d\n", requestP[now_fd].post_pos);
						fprintf(stdout, "[Log] Receive post from %s\n", buf_record.From);
						fflush(stdout);
						set_lock(requestP[now_fd].post_pos, board_fd, now_fd, F_UNLCK);
						requestP[now_fd].is_lock = 0;
						is_lock_arr[requestP[now_fd].post_pos] = 0;
						last = (requestP[now_fd].post_pos + 1) % RECORD_NUM;
					}
					else{
						int record_num = (lseek(board_fd, 0, SEEK_END) / sizeof(record));
						if(recv(now_fd, requestP[now_fd].buf, sizeof(buf), 0) <= 0) continue;
                        memcpy(buf, requestP[now_fd].buf, sizeof(buf));
						if (!strncmp(buf, "pull", 4)){
						// printf("processing pull!\n");
							int lock_num = 0;
							// printf("recordNum:%d\n", record_num);
							for (int cnt = 0; cnt < record_num; cnt++){
								if (!have_lock(cnt, board_fd) && !is_lock_arr[cnt]){
									pread(board_fd, &buf_record, sizeof(record), cnt * sizeof(record));
                                    if (strlen(buf_record.From) == 0 || strlen(buf_record.Content) == 0) continue;
									// printf("send content:%s\n", buf_record.Content);
                                    memcpy(requestP[now_fd].buf, &buf_record, sizeof(record));
									send(now_fd, requestP[now_fd].buf, sizeof(record), 0);
								}
								else lock_num++;
							}
							if (lock_num != 0){
								fprintf(stdout, "[Warning] Try to access locked post - %d\n", lock_num);
								fflush(stdout);
							}
                            memcpy(requestP[now_fd].buf, &trash, sizeof(record));
							send(now_fd, requestP[now_fd].buf, sizeof(record), 0);
							// printf("end pull while !\n");
						}
						else if (!strncmp(buf, "post", 4)){ // lock
							// printf("processing post!\n"); 
							int lock_on = -1;
							for (int i = last; i < last + RECORD_NUM; i++){
								int to_lock_pos = i % RECORD_NUM;
								if (!is_lock_arr[to_lock_pos] && set_lock(to_lock_pos, board_fd, now_fd, F_WRLCK) != -1) {
									lock_on = to_lock_pos;
									break;
								}
							}
							// printf("lock_on:%d\n", lock_on);
							if (lock_on == -1){ // send trash
                                fprintf(stderr, "send trash\n");
                                memcpy(requestP[now_fd].buf, &trash, sizeof(record));
								send(now_fd, requestP[now_fd].buf, sizeof(record), 0);
							}
							else{
								is_lock_arr[lock_on] = 1;
								requestP[now_fd].is_lock = 1;
								requestP[now_fd].post_pos = lock_on;
								// memcpy(buf, &noTrash, sizeof(record));
                                fprintf(stderr, "Send %s AND %s\n", noTrash.From, noTrash.Content);
                                memcpy(requestP[now_fd].buf, &noTrash, sizeof(record));
								send(now_fd, requestP[now_fd].buf, sizeof(record), 0);
							}

							// printf("lock_on:%d\n", lock_on);
							// int pwr = pwrite(board_fd, &trash, sizeof(record), lock_start * sizeof(record)); // set trash
							// printf("pwr:%d\n", pwr);

							// requestP[fd].is_lock = 1;
							
							// requestP[fd].is_lock = 1;


							// if (recv(now_fd, &buf_record, sizeof(record), 0) <= 0) continue;
							// pwrite(board_fd, &buf_record, sizeof(record), last * sizeof(record));
							// last++, last %= RECORD_NUM;
						}

						else if (!strncmp(buf, "exit", 4) && !requestP[now_fd].is_lock){
							// for exit
							FD_CLR(now_fd, &readfds);
							free_request(requestP, now_fd);
							close(now_fd);
						}
					}
				}
                count++;
			}
		}
	}
	free(requestP);
	return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working

static void init_request(request* reqP) {
reqP->conn_fd = -1;
reqP->buf_len = 0;
reqP->id = 0;
reqP->post_pos = -1;
reqP->is_lock = 0;
}

static void free_request(request* reqP, int now_fd) {
request *now_req = &reqP[now_fd];
init_request(now_req);
}

static void init_server(unsigned short port) {
struct sockaddr_in servaddr;
int tmp;

gethostname(svr.hostname, sizeof(svr.hostname));
svr.port = port;

svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
if (svr.listen_fd < 0) ERR_EXIT("socket");

bzero(&servaddr, sizeof(servaddr));
servaddr.sin_family = AF_INET;
servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
servaddr.sin_port = htons(port);
tmp = 1;
if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
	ERR_EXIT("setsockopt");
}
if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
	ERR_EXIT("bind");
}
if (listen(svr.listen_fd, 1024) < 0) {
	ERR_EXIT("listen");
}

// Get file descripter table size and initialize request table
maxfd = getdtablesize();
requestP = (request*) malloc(sizeof(request) * maxfd);
if (requestP == NULL) {
	ERR_EXIT("out of memory allocating all requests");
}
for (int i = 0; i < maxfd; i++) {
	init_request(&requestP[i]);
}
requestP[svr.listen_fd].conn_fd = svr.listen_fd;
strcpy(requestP[svr.listen_fd].host, svr.hostname);

return;
}
