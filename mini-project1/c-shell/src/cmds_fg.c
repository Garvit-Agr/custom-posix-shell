#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "parser.h"
#include "cmds_fg.h"
#include "jobs.h"

pid_t fg_active_pid = -1;
int fg_timed_out = 0;

void fg_alarm_handler(int sig) {
    if(fg_active_pid > 0) {
        printf("\ncshell: fg job %d timed out after 15 seconds\n", fg_active_pid);
        kill(-fg_active_pid, SIGKILL);
        fg_timed_out = 1;
    }
}

void fg_cmd(tknll *head) {
    if(head==NULL || head->next==NULL || head->next->type!=WORD) {
        printf("cshell: fg: invalid arguments\n");
        return;
    }

    char *j_str=head->next->tkn;
    if(j_str[0]=='%') j_str++;
    int j_num=atoi(j_str);
    int found_idx=-1;

    for(int i=0; i<job_cnt; i++) {
        if(bg_jobs[i].job_num==j_num && bg_jobs[i].is_done==0) {
            found_idx=i;
            break;
        }
    }

    if(found_idx==-1) {
        printf("cshell: fg: job not found\n");
        return;
    }

    job fg_job=bg_jobs[found_idx];
    remove_job(fg_job.pid);

    printf("%s\n", fg_job.cmd);

    tcsetpgrp(STDIN_FILENO, fg_job.pid);

    if(kill(-fg_job.pid, SIGCONT)<0) {
        perror("kill");
    }

    fg_active_pid = fg_job.pid;
    fg_timed_out = 0;
    
    void (*old_alrm)(int) = signal(SIGALRM, fg_alarm_handler);
    alarm(15); 

    int stopped=0;
    int status;
    
    while(waitpid(fg_job.pid, &status, WUNTRACED) < 0) {
        if(errno != EINTR) break; 
    }

    alarm(0); 
    signal(SIGALRM, old_alrm);
    fg_active_pid = -1;

    tcsetpgrp(STDIN_FILENO, getpid());

    if(WIFSTOPPED(status) && fg_timed_out==0) {
        stopped=1;
    }

    if(stopped) {
        add_job_with_id(fg_job.job_num, fg_job.pid, fg_job.pids, fg_job.num_pids, fg_job.cmd);
        printf("\n[%d] %d\n", fg_job.job_num, fg_job.pid);
    }
}