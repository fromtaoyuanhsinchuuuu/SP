#include <sys/types.h>

#define PARENT_READ_FD 3
#define PARENT_WRITE_FD 4
#define MAX_CHILDREN 8
#define MAX_FIFO_NAME_LEN 64
#define MAX_SERVICE_NAME_LEN 16
#define MAX_CMD_LEN 128

#define FINISH_SPAWN 1
#define FINISH_KILL 2
#define FINISH_EXCHANGE 3
#define MAXSTR 10



typedef struct {
    pid_t pid;
    int child_num;
    int read_fd; // read from child
    int write_fd; // write to child
    unsigned long secret;
    char name[MAX_SERVICE_NAME_LEN];
} service;

