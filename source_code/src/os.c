
#include "cpu.h"
#include "timer.h"
#include "sched.h"
#include "loader.h"
#include "mem.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int time_slot;
static int num_cpus;
static int done = 0;

static struct ld_args{
	char ** path;
	unsigned long * start_time;
} ld_processes;
int num_processes;

struct cpu_args {
	struct timer_id_t * timer_id;
	int id;
};

static void * cpu_routine(void * args) {
	struct timer_id_t * timer_id = ((struct cpu_args*)args)->timer_id;
	int id = ((struct cpu_args*)args)->id;
	/* Check for new process in ready queue */
	int time_left = 0;
	struct pcb_t * proc = NULL;
	while (1) {
		/* Check the status of current process */
		if (proc == NULL) {
			/* No process is running, the we load new process from
		 	* ready queue */
			proc = get_proc();
		}else if (proc->pc == proc->code->size) {
			/* The porcess has finish it job */
			printf("\tCPU %d: Processed %2d has finished\n",
				id ,proc->pid);
			free(proc);
			proc = get_proc();
			time_left = 0;
		}else if (time_left == 0) {
			/* The process has done its job in current time slot */
			printf("\tCPU %d: Put process %2d to run queue\n",
				id, proc->pid);
			put_proc(proc);
			proc = get_proc();
		}
		
		/* Recheck process status after loading new process */
		if (proc == NULL && done) {
			/* No process to run, exit */
			printf("\tCPU %d stopped\n", id);
			break;
		}else if (proc == NULL) {
			/* There may be new processes to run in
			 * next time slots, just skip current slot */
			next_slot(timer_id);
			continue;
		}else if (time_left == 0) {
			printf("\tCPU %d: Dispatched process %2d\n",
				id, proc->pid);
			time_left = time_slot;
		}
		
		/* Run current process */
		run(proc);
		time_left--;
		next_slot(timer_id);
	}
	detach_event(timer_id);
	pthread_exit(NULL);
}

static void * ld_routine(void * args) {
	struct timer_id_t * timer_id = (struct timer_id_t*)args;
	int i = 0;
	while (i < num_processes) {
		struct pcb_t * proc = load(ld_processes.path[i]);
		while (current_time() < ld_processes.start_time[i]) {
			next_slot(timer_id);
		}
		printf("\tLoaded a process at %s, PID: %d\n",
			ld_processes.path[i], proc->pid);
		add_proc(proc);
		free(ld_processes.path[i]);
		i++;
		next_slot(timer_id);
	}
	free(ld_processes.path);
	free(ld_processes.start_time);
	done = 1;
	detach_event(timer_id);
	pthread_exit(NULL);
}

static void read_config(const char * path) {
	FILE * file;
	if ((file = fopen(path, "r")) == NULL) {
		printf("Cannot find configure file at %s\n", path);
		exit(1);
	}
	fscanf(file, "%d %d %d\n", &time_slot, &num_cpus, &num_processes);
	ld_processes.path = (char**)malloc(sizeof(char*) * num_processes);
	ld_processes.start_time = (unsigned long*)
		malloc(sizeof(unsigned long) * num_processes);
	int i;
	for (i = 0; i < num_processes; i++) {
		ld_processes.path[i] = (char*)malloc(sizeof(char) * 100);
		ld_processes.path[i][0] = '\0';
		strcat(ld_processes.path[i], "input/proc/");
		char proc[100];
		fscanf(file, "%lu %s\n", &ld_processes.start_time[i], proc);
		strcat(ld_processes.path[i], proc);
	}
}

int main(int argc, char * argv[]) {
	/* Read config */
	if (argc != 2) {
		printf("Usage: os [path to configure file]\n");
		return 1;
	}
	char path[100];
	path[0] = '\0';
	strcat(path, "input/");
	strcat(path, argv[1]);
	read_config(path);

	pthread_t * cpu = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
	struct cpu_args * args =
		(struct cpu_args*)malloc(sizeof(struct cpu_args) * num_cpus);
	pthread_t ld;
	
	/* Init timer */
	int i;
	for (i = 0; i < num_cpus; i++) {
		args[i].timer_id = attach_event();
		args[i].id = i;
	}
	struct timer_id_t * ld_event = attach_event();
	start_timer();

	/* Init scheduler */
	init_scheduler();

	/* Run CPU and loader */
	pthread_create(&ld, NULL, ld_routine, (void*)ld_event);
	for (i = 0; i < num_cpus; i++) {
		pthread_create(&cpu[i], NULL,
			cpu_routine, (void*)&args[i]);
	}

	/* Wait for CPU and loader finishing */
	for (i = 0; i < num_cpus; i++) {
		pthread_join(cpu[i], NULL);
	}
	pthread_join(ld, NULL);

	/* Stop timer */
	stop_timer();

	printf("\nMEMORY CONTENT: \n");
	dump();

	return 0;

}



/*
./os os_0
        Loaded a process at input/proc/p0, PID: 1
Time slot   0
Time slot   1
        CPU 0: Dispatched process  1
Time slot   2
        Loaded a process at input/proc/p1, PID: 2
Time slot   3
        CPU 1: Dispatched process  2
        Loaded a process at input/proc/p1, PID: 3
Time slot   4
        Loaded a process at input/proc/p1, PID: 4
Time slot   5
Time slot   6
Time slot   7
        CPU 0: Put process  1 to run queue
        CPU 0: Dispatched process  3
Time slot   8
Time slot   9
        CPU 1: Put process  2 to run queue
        CPU 1: Dispatched process  4
Time slot  10
Time slot  11
Time slot  12
Time slot  13
        CPU 0: Put process  3 to run queue
        CPU 0: Dispatched process  1
Time slot  14
Time slot  15
        CPU 1: Put process  4 to run queue
        CPU 1: Dispatched process  2
Time slot  16
Time slot  17
        CPU 0: Processed  1 has finished
        CPU 0: Dispatched process  3
Time slot  18
Time slot  19
        CPU 1: Processed  2 has finished
        CPU 1: Dispatched process  4
Time slot  20
Time slot  21
        CPU 0: Processed  3 has finished
        CPU 0 stopped
Time slot  22
Time slot  23
        CPU 1: Processed  4 has finished
        CPU 1 stopped

MEMORY CONTENT: 
000: 00000-003ff - PID: 03 (idx 000, nxt: 001)
001: 00400-007ff - PID: 03 (idx 001, nxt: 002)
002: 00800-00bff - PID: 03 (idx 002, nxt: 003)
003: 00c00-00fff - PID: 03 (idx 003, nxt: -01)
004: 01000-013ff - PID: 04 (idx 000, nxt: 005)
005: 01400-017ff - PID: 04 (idx 001, nxt: 006)
006: 01800-01bff - PID: 04 (idx 002, nxt: 012)
007: 01c00-01fff - PID: 02 (idx 000, nxt: 008)
008: 02000-023ff - PID: 02 (idx 001, nxt: 009)
009: 02400-027ff - PID: 02 (idx 002, nxt: 010)
        025e7: 0a
010: 02800-02bff - PID: 02 (idx 003, nxt: 011)
011: 02c00-02fff - PID: 02 (idx 004, nxt: -01)
012: 03000-033ff - PID: 04 (idx 003, nxt: -01)
014: 03800-03bff - PID: 03 (idx 000, nxt: 015)
015: 03c00-03fff - PID: 03 (idx 001, nxt: 016)
016: 04000-043ff - PID: 03 (idx 002, nxt: 017)
        041e7: 0a
017: 04400-047ff - PID: 03 (idx 003, nxt: 018)
018: 04800-04bff - PID: 03 (idx 004, nxt: -01)
023: 05c00-05fff - PID: 02 (idx 000, nxt: 024)
024: 06000-063ff - PID: 02 (idx 001, nxt: 025)
025: 06400-067ff - PID: 02 (idx 002, nxt: 026)
026: 06800-06bff - PID: 02 (idx 003, nxt: -01)
047: 0bc00-0bfff - PID: 01 (idx 000, nxt: -01)
        0bc14: 64
057: 0e400-0e7ff - PID: 04 (idx 000, nxt: 058)
058: 0e800-0ebff - PID: 04 (idx 001, nxt: 059)
059: 0ec00-0efff - PID: 04 (idx 002, nxt: 060)
        0ede7: 0a
060: 0f000-0f3ff - PID: 04 (idx 003, nxt: 061)
061: 0f400-0f7ff - PID: 04 (idx 004, nxt: -01)
NOTE: Read file output/os_0 to verify your result
----- OS TEST 1 ----------------------------------------------------
./os os_1
Time slot   0
Time slot   1
        Loaded a process at input/proc/p0, PID: 1
Time slot   2
        CPU 0: Dispatched process  1
        Loaded a process at input/proc/s3, PID: 2
Time slot   3
        CPU 1: Dispatched process  2
Time slot   4
        CPU 0: Put process  1 to run queue
        CPU 0: Dispatched process  1
        Loaded a process at input/proc/m1, PID: 3
Time slot   5
        CPU 1: Put process  2 to run queue
        CPU 1: Dispatched process  2
        CPU 2: Dispatched process  3
Time slot   6
        CPU 0: Put process  1 to run queue
        CPU 0: Dispatched process  1
        Loaded a process at input/proc/s2, PID: 4
Time slot   7
        CPU 1: Put process  2 to run queue
        CPU 1: Dispatched process  2
        CPU 2: Put process  3 to run queue
        CPU 2: Dispatched process  3
        Loaded a process at input/proc/m0, PID: 5
        CPU 3: Dispatched process  4
Time slot   8
        CPU 0: Put process  1 to run queue
        CPU 0: Dispatched process  5
Time slot   9
        CPU 1: Put process  2 to run queue
        CPU 1: Dispatched process  2
        CPU 2: Put process  3 to run queue
        CPU 2: Dispatched process  1
        Loaded a process at input/proc/p1, PID: 6
        CPU 3: Put process  4 to run queue
        CPU 3: Dispatched process  6
Time slot  10
        CPU 0: Put process  5 to run queue
        CPU 0: Dispatched process  4
Time slot  11
        CPU 1: Put process  2 to run queue
        CPU 1: Dispatched process  3
        CPU 3: Put process  6 to run queue
        CPU 3: Dispatched process  5
        Loaded a process at input/proc/s0, PID: 7
        CPU 2: Put process  1 to run queue
        CPU 2: Dispatched process  7
Time slot  12
        CPU 0: Put process  4 to run queue
        CPU 0: Dispatched process  4
Time slot  13
        CPU 1: Put process  3 to run queue
        CPU 1: Dispatched process  2
        CPU 3: Put process  5 to run queue
        CPU 3: Dispatched process  6
        CPU 2: Put process  7 to run queue
        CPU 2: Dispatched process  1
Time slot  14
        CPU 0: Put process  4 to run queue
        CPU 0: Dispatched process  4
Time slot  15
        CPU 1: Put process  2 to run queue
        CPU 1: Dispatched process  7
        CPU 2: Processed  1 has finished
        CPU 2: Dispatched process  3
        CPU 3: Put process  6 to run queue
        CPU 3: Dispatched process  5
Time slot  16
        CPU 0: Put process  4 to run queue
        CPU 0: Dispatched process  4
        Loaded a process at input/proc/s1, PID: 8
Time slot  17
        CPU 1: Put process  7 to run queue
        CPU 1: Dispatched process  8
        CPU 2: Processed  3 has finished
        CPU 2: Dispatched process  2
        CPU 3: Put process  5 to run queue
        CPU 3: Dispatched process  6
Time slot  18
        CPU 0: Put process  4 to run queue
        CPU 0: Dispatched process  4
        CPU 2: Processed  2 has finished
        CPU 2: Dispatched process  7
Time slot  19
        CPU 1: Put process  8 to run queue
        CPU 1: Dispatched process  5
        CPU 3: Put process  6 to run queue
        CPU 3: Dispatched process  8
Time slot  20
        CPU 1: Processed  5 has finished
        CPU 1: Dispatched process  6
        CPU 0: Processed  4 has finished
        CPU 0 stopped
        CPU 2: Put process  7 to run queue
        CPU 2: Dispatched process  7
Time slot  21
        CPU 3: Put process  8 to run queue
        CPU 3: Dispatched process  8
Time slot  22
        CPU 1: Put process  6 to run queue
        CPU 1: Dispatched process  6
        CPU 2: Put process  7 to run queue
        CPU 2: Dispatched process  7
Time slot  23
        CPU 3: Put process  8 to run queue
        CPU 3: Dispatched process  8
Time slot  24
        CPU 1: Processed  6 has finished
        CPU 1 stopped
        CPU 2: Put process  7 to run queue
        CPU 2: Dispatched process  7
        CPU 3: Processed  8 has finished
        CPU 3 stopped
Time slot  25
Time slot  26
        CPU 2: Put process  7 to run queue
        CPU 2: Dispatched process  7
Time slot  27
Time slot  28
        CPU 2: Put process  7 to run queue
        CPU 2: Dispatched process  7
Time slot  29
        CPU 2: Processed  7 has finished
        CPU 2 stopped

MEMORY CONTENT: 
000: 00000-003ff - PID: 05 (idx 000, nxt: 001)
        003e8: 15
001: 00400-007ff - PID: 05 (idx 001, nxt: -01)
002: 00800-00bff - PID: 05 (idx 000, nxt: 003)
003: 00c00-00fff - PID: 05 (idx 001, nxt: 004)
004: 01000-013ff - PID: 05 (idx 002, nxt: 005)
005: 01400-017ff - PID: 05 (idx 003, nxt: 006)
006: 01800-01bff - PID: 05 (idx 004, nxt: -01)
011: 02c00-02fff - PID: 06 (idx 000, nxt: 012)
012: 03000-033ff - PID: 06 (idx 001, nxt: 013)
013: 03400-037ff - PID: 06 (idx 002, nxt: 014)
014: 03800-03bff - PID: 06 (idx 003, nxt: -01)
021: 05400-057ff - PID: 01 (idx 000, nxt: -01)
        05414: 64
024: 06000-063ff - PID: 05 (idx 000, nxt: 025)
        06014: 66
025: 06400-067ff - PID: 05 (idx 001, nxt: -01)
026: 06800-06bff - PID: 06 (idx 000, nxt: 027)
027: 06c00-06fff - PID: 06 (idx 001, nxt: 028)
028: 07000-073ff - PID: 06 (idx 002, nxt: 029)
        071e7: 0a
029: 07400-077ff - PID: 06 (idx 003, nxt: 030)
030: 07800-07bff - PID: 06 (idx 004, nxt: -01)
*/
