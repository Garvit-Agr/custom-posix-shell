#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOB_PIDS 1024
#define MAX_JOB_CMDS 512

typedef struct job {
    int job_num;
    pid_t pid;
    pid_t pids[MAX_JOB_PIDS];
    int num_pids;
    char names[MAX_JOB_CMDS][64];
    char cmd[1024];
    int state;
    int is_done;
    int exit_status;
} job;

extern job bg_jobs[100];
extern int job_cnt;
extern int next_job_num;

void add_job(pid_t lead_pid, pid_t *all_pids, int num_pids, char names[][64], char *cmd);
void remove_job(pid_t pid);

#endif