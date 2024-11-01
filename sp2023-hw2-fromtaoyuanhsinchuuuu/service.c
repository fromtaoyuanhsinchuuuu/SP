#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <assert.h>

#include "util.h"

#define ERR_EXIT(s) perror(s), exit(errno);

static unsigned long secret;
static char service_name[MAX_SERVICE_NAME_LEN];

static inline bool is_manager() {
    return strncmp(service_name, "Manager", MAX_SERVICE_NAME_LEN) == 0;
}

void print_not_exist(char *service_name) {
    printf("%s doesn't exist\n", service_name);
}

void print_receive_command(char *service_name, char *cmd, char *tA, char *tB) {
	if (tB == NULL || tB[0] == '\0') printf("%s has received %s %s\n", service_name, cmd, tA);
    else printf("%s has received %s %s %s\n", service_name, cmd, tA, tB);
}

void print_spawn(char *parent_name, char *child_name) {
    printf("%s has spawned a new service %s\n", parent_name, child_name);
}

void print_kill(char *target_name, int decendents_num) {
    printf("%s and %d child services are killed\n", target_name, decendents_num);
}

void print_acquire_secret(char *service_a, char *service_b, unsigned long secret) {
    printf("%s has acquired a new secret from %s, value: %lu\n", service_a, service_b, secret);
}

void print_exchange(char *service_a, char *service_b) {
    printf("%s and %s have exchanged their secrets\n", service_a, service_b);
}

void print_service_create(char *service_name, pid_t pid, unsigned long secret){
    printf("%s has been spawned, pid: %d, secret: %lu\n", service_name, pid, secret);
}

void init_now(pid_t pid, unsigned long secret, char *name, service *now)
{
    strcpy(now->name, name);
    now->pid = pid;
    now->read_fd = -1;
    now->write_fd = -1;
    now->secret = secret;
    now->child_num = 0;
    return;
}

void modify_fd()
{
	int flags = fcntl(3, F_GETFD);
	flags &= ~FD_CLOEXEC;
	fcntl(3, F_SETFD, flags);
	fcntl(4, F_SETFD, flags);
	return;
}

typedef unsigned long ul;

typedef struct Data{
    char cmd[MAXSTR];
    char targetA[MAX_SERVICE_NAME_LEN];
    char targetB[MAX_SERVICE_NAME_LEN];
	char fifo1[MAX_FIFO_NAME_LEN];
	char fifo2[MAX_FIFO_NAME_LEN];
    int exchange_find_num;
	bool spawn_valid;
} Data;

typedef struct Node{
    service sv;
    struct Node *next;
} Node;


void set_data_buf(char *cmd, char *tA, char *tB, Data *buf)
{
    strncpy(buf->cmd, cmd, MAXSTR);
    strncpy(buf->targetA, tA, MAX_SERVICE_NAME_LEN);
    if (tB != NULL) strncpy(buf->targetB, tB, MAX_SERVICE_NAME_LEN);
	else buf->targetB[0] = '\0';
    buf->exchange_find_num = 0;
	buf->spawn_valid = 0;
    return;
}

Node *genNode(pid_t pid, int read_fd, int write_fd, unsigned long secret, char *name)
{
    Node *node = (Node *) malloc(sizeof(Node));
    node->sv.pid = pid, strncpy(node->sv.name, name, MAX_SERVICE_NAME_LEN), node->sv.read_fd = read_fd, node->sv.write_fd = write_fd, node->sv.secret = secret;
    node->next = NULL;
    node->sv.child_num = 0;
    return node;
}

void print_ll(char *now_str, Node *head)
{
    printf("%s:", now_str);
    for (Node *node = head; node != NULL; node = node->next){
        printf("%s ", node->sv.name);
    }
    printf("\n");
    return;
}


Node *service_head = NULL;
Node *find_tail(Node *now)
{
    if (now == NULL) return NULL;
    if (now->next == NULL) return now;
    return (find_tail(now->next));
}

char cmd[MAXSTR];
char tA[MAX_SERVICE_NAME_LEN]; // parent
char tB[MAX_SERVICE_NAME_LEN]; // child

Data buf;


int main(int argc, char *argv[]) {
    pid_t pid = getpid();        

    if (argc != 2) {
        fprintf(stderr, "Usage: ./service [service_name]\n");
        return 0;
    }

    srand(pid);
    secret = rand();
    /* 
     * prevent buffered I/O
     * equivalent to fflush() after each stdout
     */
    setvbuf(stdout, NULL, _IONBF, 0);
    strncpy(service_name, argv[1], MAX_SERVICE_NAME_LEN);
    bool Is_manager = is_manager();
    printf("%s has been spawned, pid: %d, secret: %lu\n", service_name, pid, secret);

    service now;
    init_now(pid, secret, service_name, &now);
    while(Is_manager){ // receiveing command...
        scanf("%s", cmd);
        if (!strncmp(cmd, "spawn", MAXSTR)){
            scanf("%s%s", tA, tB);
            print_receive_command(now.name, cmd, tA, tB);
            set_data_buf(cmd, tA, tB, &buf);
            if (!strncmp(tA, "Manager", MAX_SERVICE_NAME_LEN)){ // Manager要生
				buf.spawn_valid = 1;
                int pfd1[2]; // 0:parent read, 1:child write
                int pfd2[2]; // 1:parent write, 0:child read

                pipe2(pfd1, O_CLOEXEC);
               	pipe2(pfd2, O_CLOEXEC);

                pid_t rt_pid;
                now.child_num++;
                if ((rt_pid = fork()) == 0){ // child_process
                    dup2(pfd2[0], PARENT_READ_FD);
                    dup2(pfd1[1], PARENT_WRITE_FD);
					modify_fd();
                    execl("./service", "./service", buf.targetB, NULL);
                }
                else{ // parent_process
                    assert(rt_pid > 0);
                    close(pfd2[0]);
                    close(pfd1[1]);
                    Node *tail = find_tail(service_head);
                    Node *now_handle = NULL;
                    if (tail == NULL){ // Manager沒有小孩
                        service_head = genNode(pid, pfd1[0], pfd2[1], secret, buf.targetB);
                        now_handle = service_head;
                    }
                    else{
                        tail->next = genNode(pid, pfd1[0], pfd2[1], secret, buf.targetB);
                        now_handle = tail->next;
                    }
                    write(now_handle->sv.write_fd, &buf, sizeof(Data));
                    int finish_status = 0;
                    read(now_handle->sv.read_fd, &finish_status, sizeof(int));
                    if (finish_status == FINISH_SPAWN) print_spawn(buf.targetA, buf.targetB);
                }
            }
            else{ // 從linked list找
                int find = 0;
                for (Node *node = service_head; node != NULL && !find; node = node->next){
                    while (write(node->sv.write_fd, &buf, sizeof(Data)) <= 0) printf("writing...\n");
                    while (read(node->sv.read_fd, &find, sizeof(int)) <= 0) printf("reading...\n");
                }
                if (find == 0) print_not_exist(buf.targetA);
            }
        }
        else if (!strncmp(cmd, "kill", MAXSTR)){
            scanf("%s", tA);
            print_receive_command(now.name, cmd, tA, NULL);
            set_data_buf(cmd, tA, NULL, &buf);
            if (!strncmp(tA, now.name, MAX_SERVICE_NAME_LEN)){ // kill Manager
                strncpy(buf.cmd, "die", MAXSTR);
				int des_num = 0;
				int return_num = 0;
                for (Node *node = service_head; node != NULL; node = node->next){
					// strncpy(buf.targetA, node->sv.name, MAX_SERVICE_NAME_LEN);
                    while (write(node->sv.write_fd, &buf, sizeof(Data)) <= 0) printf("writing...\n");
					while (read(node->sv.read_fd, &return_num, sizeof(int)) <= 0) printf("reading..\n");
					// printf("return_num:%d\n", return_num);
					des_num += return_num;
					waitpid(node->sv.pid, NULL, 0);
                    close(node->sv.read_fd), close(node->sv.write_fd);
                }
				print_kill(now.name, des_num);
				exit(0);
            }
            else{ // 從linked-list找要kill的對象
                int total_num = 0;

                Node *prev = NULL;
                Node *target = NULL;
                for (Node *node = service_head; node != NULL && total_num == 0; node = node->next){
                    // printf("asdasd\n");
				
                    while (write(node->sv.write_fd, &buf, sizeof(Data)) <= 0);
                    while (read(node->sv.read_fd, &total_num, sizeof(int)) <= 0);
                    if (total_num > 0){
                        target = node;
                        break;
                    }
                    prev = node;
                }
                if (total_num == 0){ // DNE
					assert(target == NULL);
                    print_not_exist(buf.targetA);
                    continue;
                }
                if (!strncmp(target->sv.name, buf.targetA, MAX_SERVICE_NAME_LEN)){ // target is direct children
                	now.child_num--;
					waitpid(target->sv.pid, NULL, 0);
                	close(target->sv.read_fd), close(target->sv.write_fd); // close pipe
					if (prev == NULL){ 
						service_head = target->next;
						free(target);
					}
					else{
						prev->next = target->next;
						free(target);
					}
				}
				print_kill(buf.targetA, total_num - 1);
            }
        }
        else if (!strncmp(cmd, "exchange", MAXSTR)){
            scanf("%s%s", tA, tB);
			print_receive_command(now.name, cmd, tA, tB);
            set_data_buf(cmd, tA, tB, &buf);
			char fifo1[MAX_FIFO_NAME_LEN], fifo2[MAX_FIFO_NAME_LEN];
			snprintf(fifo1, MAX_FIFO_NAME_LEN, "%s_to_%s.fifo", buf.targetA, buf.targetB);
			snprintf(fifo2, MAX_FIFO_NAME_LEN, "%s_to_%s.fifo", buf.targetB, buf.targetA);
			mkfifo(fifo1, 0666);
            mkfifo(fifo2, 0666);
			strncpy(buf.fifo1, fifo1, MAX_FIFO_NAME_LEN);
			strncpy(buf.fifo2, fifo2, MAX_FIFO_NAME_LEN);

			bool ex_now = !strncmp(buf.targetA, now.name, MAX_SERVICE_NAME_LEN) || !strncmp(buf.targetB, now.name, MAX_SERVICE_NAME_LEN);
			buf.exchange_find_num = ex_now;
			int ori_find = buf.exchange_find_num;
			int return_num = 0;
			for (Node *node = service_head; node != NULL && buf.exchange_find_num != 2; node = node->next){
				while (write(node->sv.write_fd, &buf, sizeof(Data)) <= 0); 
				while (read(node->sv.read_fd, &return_num, sizeof(int)) <= 0); 
				buf.exchange_find_num += return_num;
			}
			assert(buf.exchange_find_num == 2); // 保證exchange的對象存在
			int wr_fd = -1, rd_fd = -1;
			if (ex_now){ 
				ul send_secret = now.secret, receive_secret = 1;
				wr_fd = open(buf.fifo1, O_WRONLY);
				write(wr_fd, &send_secret, sizeof(ul));

				rd_fd = open(buf.fifo2, O_RDONLY);
				read(rd_fd, &receive_secret, sizeof(ul));
				now.secret = receive_secret;

				if (!strncmp(now.name, buf.targetA, MAX_SERVICE_NAME_LEN)){
					print_acquire_secret(buf.targetA, buf.targetB, receive_secret);
					int A_done = 1;
					write(wr_fd, &A_done, sizeof(int));
				}
				else{
					assert(!strncmp(now.name, buf.targetB, MAX_SERVICE_NAME_LEN));
					int A_done = 0;
					read(rd_fd, &A_done, sizeof(int));
					if (A_done) print_acquire_secret(buf.targetB, buf.targetA, receive_secret);
				}
				assert(wr_fd != -1 && rd_fd != -1);
				close(wr_fd);
				close(rd_fd);
			}
			/* 發廣播問exchange完成了沒 */

			strncpy(buf.cmd, "check_ex", MAXSTR);
			buf.exchange_find_num = ex_now;

			return_num = 0;
			for (Node *node = service_head; node != NULL && buf.exchange_find_num != 2; node = node->next){
				while (write(node->sv.write_fd, &buf, sizeof(Data)) <= 0);
				while (read(node->sv.read_fd, &return_num, sizeof(int)) <= 0);
				buf.exchange_find_num += return_num;
			}
			assert(buf.exchange_find_num == 2); // 保證換完
			remove(fifo1);
			remove(fifo2);
			print_exchange(buf.targetA, buf.targetB);
        }
        // print_ll(now.name, service_head);
    }

    while (!Is_manager){
        while (read(PARENT_READ_FD, &buf, sizeof(Data)) <= 0);
        if (!strncmp(buf.cmd, "spawn", MAXSTR)){
            if (!strncmp(buf.targetB, now.name, MAX_SERVICE_NAME_LEN) && buf.spawn_valid){ // 自己是要被生的
                int finish_status = FINISH_SPAWN;
                write(PARENT_WRITE_FD, &finish_status, sizeof(int));
            }
            else if (!strncmp(buf.targetA, now.name, MAX_SERVICE_NAME_LEN)){ // 自己是要生別人的
                print_receive_command(now.name, buf.cmd, buf.targetA, buf.targetB);
                int pfd1[2]; // 0:parent read, 1:child write
                int pfd2[2]; // 1:parent write, 0:child read
                pipe2(pfd1, O_CLOEXEC);
                pipe2(pfd2, O_CLOEXEC);

                pid_t rt_pid;
                now.child_num++;
                if ((rt_pid = fork()) == 0){ // child_process
                    dup2(pfd2[0], PARENT_READ_FD);
                    dup2(pfd1[1], PARENT_WRITE_FD);
					modify_fd();
                    execl("./service", "./service", buf.targetB, NULL);
                }
                else{ // parent_process
                    assert(rt_pid > 0);
					buf.spawn_valid = 1;
                    
                    close(pfd2[0]);
                    close(pfd1[1]);
                    write(pfd2[1], &buf, sizeof(Data));
                    Node *tail = find_tail(service_head);
                    Node *now_handle = NULL;
                    if (tail == NULL){ // now沒有小孩
                        service_head = genNode(pid, pfd1[0], pfd2[1], secret, buf.targetB);
                        now_handle = service_head;
                    }
                    else{
                        tail->next = genNode(pid, pfd1[0], pfd2[1], secret, buf.targetB);
                        now_handle = tail->next;
                    }
                    int finish_status = 0;
                    read(now_handle->sv.read_fd, &finish_status, sizeof(int));
                    if (!strncmp(buf.targetA, now.name, MAX_SERVICE_NAME_LEN) && finish_status == FINISH_SPAWN){
                        print_spawn(buf.targetA, buf.targetB);
                    }
                    write(PARENT_WRITE_FD, &finish_status, sizeof(int));
                }
            }
            else{ // 從linked-list開始找誰要生
                print_receive_command(now.name, buf.cmd, buf.targetA, buf.targetB);
                int find = 0;
                for (Node *node = service_head; node != NULL && !find; node = node->next){
                    while (write(node->sv.write_fd, &buf, sizeof(Data)) <= 0);
                    while (read(node->sv.read_fd, &find, sizeof(int)) <= 0);
                }
                while(write(PARENT_WRITE_FD, &find, sizeof(int)) <= 0);
            }
        }
        else if (!strncmp(buf.cmd, "kill", MAXSTR)){
            print_receive_command(now.name, buf.cmd, buf.targetA, buf.targetB);
			int des_num = 1;
			int return_num = 0;
            if (!strncmp(buf.targetA, now.name, MAX_SERVICE_NAME_LEN)){ // 自己是要被殺的
                strncpy(buf.cmd, "die", MAXSTR);
                for (Node *node = service_head; node != NULL; node = node->next){ // 讓小孩全部去自殺
                    while (write(node->sv.write_fd, &buf, sizeof(Data)) <= 0);
					while (read(node->sv.read_fd, &return_num, sizeof(int)) <= 0);
					des_num += return_num;
					waitpid(node->sv.pid, NULL, 0);
                    close(node->sv.read_fd), close(node->sv.write_fd);
                }
                while(write(PARENT_WRITE_FD, &des_num, sizeof(int)) <= 0);
                exit(0);
            }
            else{ // 從linked-list找
                Node *prev = NULL;
                Node *target = NULL;
                int total_num = 0;
                for (Node *node = service_head; node != NULL && total_num == 0; node = node->next){
                    while (write(node->sv.write_fd, &buf, sizeof(Data)) <= 0);
                    while (read(node->sv.read_fd, &total_num, sizeof(int)) <= 0);
                    if (total_num > 0){
                        target = node;
                        break;
                    }
                    prev = node;
                }
                if (target != NULL && !strncmp(target->sv.name, buf.targetA, MAX_SERVICE_NAME_LEN)){ // 要殺的是direct child
                	now.child_num--;
                	close(target->sv.read_fd), close(target->sv.write_fd); // close pipe
					waitpid(target->sv.pid, NULL, 0); // wait target to die
					if (prev == NULL){ 
						service_head = target->next;
						free(target);
					}
					else{
						prev->next = target->next;
						free(target);
					}
				}
                while(write(PARENT_WRITE_FD, &total_num, sizeof(int)) <= 0);
            }

        }
        else if (!strncmp("exchange", buf.cmd, MAXSTR)){
			print_receive_command(now.name, buf.cmd, buf.targetA, buf.targetB);
			bool ex_now = !strncmp(buf.targetA, now.name, MAX_SERVICE_NAME_LEN) || !strncmp(buf.targetB, now.name, MAX_SERVICE_NAME_LEN);

			int ori_find = buf.exchange_find_num;
			buf.exchange_find_num += ex_now;
			int find_num = ex_now;

			int return_num = 0;
			for (Node *node = service_head; node != NULL && buf.exchange_find_num != 2; node = node->next){
				while (write(node->sv.write_fd, &buf, sizeof(Data)) <= 0);
				while (read(node->sv.read_fd, &return_num, sizeof(int)) <= 0); 
				buf.exchange_find_num += return_num;
				find_num += return_num;
			}
			write(PARENT_WRITE_FD, &find_num, sizeof(int));
			if (ex_now){
				ul send_secret = now.secret, receive_secret = 1;
				int rd_fd = -1, wr_fd = -1;
				if (ori_find == 0){ // now是先被找到的
					wr_fd = open(buf.fifo1, O_WRONLY);
					write(wr_fd, &send_secret, sizeof(ul));

					rd_fd = open(buf.fifo2, O_RDONLY);
					read(rd_fd, &receive_secret, sizeof(ul));
					now.secret = receive_secret;
				}
				else{ // now是後被找到的
					assert(ori_find == 1);
					rd_fd = open(buf.fifo1, O_RDONLY);
					read(rd_fd, &receive_secret, sizeof(ul));

					wr_fd = open(buf.fifo2, O_WRONLY);
					write(wr_fd, &send_secret, sizeof(ul));
					now.secret = receive_secret;
					
				}
				if (!strncmp(now.name, buf.targetA, MAX_SERVICE_NAME_LEN)){
					print_acquire_secret(buf.targetA, buf.targetB, receive_secret);
					int A_done = 1;
					write(wr_fd, &A_done, sizeof(int));
				}
				else{
					assert(!strncmp(now.name, buf.targetB, MAX_SERVICE_NAME_LEN));
					int A_done = 0;
					read(rd_fd, &A_done, sizeof(int));
					if (A_done) print_acquire_secret(buf.targetB, buf.targetA, receive_secret);
				}

				assert(rd_fd != -1 && wr_fd != -1);
				close(rd_fd);
				close(wr_fd);
			}
        }
        else if (!strncmp("die", buf.cmd, MAXSTR)){ // for child to kill himself qq
			int des_num = 1; // 包含自己
			int return_num = 0;
			for (Node *node = service_head; node != NULL; node = node->next){
				while(write(node->sv.write_fd, &buf, sizeof(Data)) <= 0);
				while(read(node->sv.read_fd, &return_num, sizeof(int)) <= 0);
				waitpid(node->sv.pid, NULL, 0);
				close(node->sv.read_fd);
				close(node->sv.write_fd);
				des_num += return_num;
			}
			while(write(PARENT_WRITE_FD, &des_num, sizeof(int)) <= 0);
            exit(0);
        }
		else if (!strncmp("check_ex", buf.cmd, MAXSTR)){ // 在這收到廣播代表已經ex完
			bool ex_now = !strncmp(buf.targetA, now.name, MAX_SERVICE_NAME_LEN) || !strncmp(buf.targetB, now.name, MAX_SERVICE_NAME_LEN);
			buf.exchange_find_num += ex_now;
			int find_num = ex_now;

			int return_num = 0;
			for (Node *node = service_head; node != NULL && buf.exchange_find_num != 2; node = node->next){
				while (write(node->sv.write_fd, &buf, sizeof(Data)) <= 0);
				while (read(node->sv.read_fd, &return_num, sizeof(int)) <= 0);
				buf.exchange_find_num += return_num;
				find_num += return_num;
			}
			while(write(PARENT_WRITE_FD, &find_num, sizeof(int)) <= 0);
		}
    }
    return 0;
}
