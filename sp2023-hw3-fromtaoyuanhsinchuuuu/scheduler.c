#include "threadtools.h"
#include <sys/signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Print out the signal you received.
 * If SIGALRM is received, reset the alarm here.
 * This function should not return. Instead, call siglongjmp(sched_buf, 1).
 */
void sighandler(int signo) {
	if (signo == SIGTSTP){
		printf("caught SIGTSTP\n");
	}
    else if (signo == SIGALRM){
		printf("caught SIGALRM\n");
		alarm(timeslice);
	}
	sigprocmask(SIG_SETMASK, &base_mask, NULL);
	siglongjmp(sched_buf, 1);
}


/*
 * Prior to calling this function, both SIGTSTP and SIGALRM should be blocked.
 */
void scheduler() {
	int jmp_num = -1;
	if ((jmp_num = setjmp(sched_buf)) == 0){
		longjmp(ready_queue[rq_current]->environment, 1);
	}
	else if (jmp_num == 1){ // from sighandler
		if (bank.lock_owner == -1 && wq_size > 0){
			ready_queue[rq_size++] = waiting_queue[0];
			for (int i = 0; i < wq_size - 1; i++){
				waiting_queue[i] = waiting_queue[i + 1];
			}
			wq_size--;
		}
		rq_current = (rq_current + 1) % rq_size;
		longjmp(ready_queue[rq_current]->environment, 1);
	}
	else if (jmp_num == 2){ // from lock
		if (bank.lock_owner == -1 && wq_size > 0){
			ready_queue[rq_size++] = waiting_queue[0];
			for (int i = 0; i < wq_size - 1; i++){
				waiting_queue[i] = waiting_queue[i + 1];
			}
			wq_size--;
		}
		waiting_queue[wq_size++] = ready_queue[rq_current];
		if (rq_current != rq_size - 1){
			ready_queue[rq_current] = ready_queue[rq_size - 1];
			rq_size--;
		}
		else{
			rq_size--;
			rq_current = 0;
		}
		if (rq_size != 0) longjmp(ready_queue[rq_current]->environment, 1);
	}
	else if (jmp_num == 3){ // from thread_exit
		if (bank.lock_owner == -1 && wq_size > 0){
			ready_queue[rq_size++] = waiting_queue[0];
			for (int i = 0; i < wq_size - 1; i++){
				waiting_queue[i] = waiting_queue[i + 1];
			}
			wq_size--;
		}
		if (rq_current == rq_size - 1){ // last_element
			free(ready_queue[rq_current]);
			rq_size--;
			rq_current = 0;
		}
		else{
			free(ready_queue[rq_current]);
			ready_queue[rq_current] = ready_queue[rq_size - 1];
			rq_size--;
		}
		if (rq_size != 0) longjmp(ready_queue[rq_current]->environment, 1);
	}
	if (rq_size == 0 && wq_size == 0){
		return;
	}
}
