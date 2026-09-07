#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

typedef struct job {
    int job_num;
    pid_t pid;
    char cmd[1024];
    int state; 
} job;

extern job bg_jobs[100];
extern int job_cnt;
extern int next_job_num;

void add_job(pid_t pid, char *cmd);
void remove_job(pid_t pid);

#endif