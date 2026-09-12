#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include "parser.h"
#include "exec.h"
#include "inp_redir.h"
#include "out_redir.h"
#include "cmds_hop.h"
#include "cmds_reveal.h"
#include "cmds_peek.h"
#include "cmds_locate.h"
#include "cmds_activities.h"
#include "jobs.h"

void add_cmd_part(char *dst, size_t cap, const char *part) {
    size_t len=strlen(dst);
    if(len>=cap-1) return;
    snprintf(dst+len, cap-len, "%s ", part);
}

void add_tkn_txt(char *dst, size_t cap, tknll *tmp) {
    if(tmp->tkn!=NULL) add_cmd_part(dst, cap, tmp->tkn);
    else if(tmp->type==OP_PIPE) add_cmd_part(dst, cap, "|");
    else if(tmp->type==OP_LT) add_cmd_part(dst, cap, "<");
    else if(tmp->type==OP_GT) add_cmd_part(dst, cap, ">");
    else if(tmp->type==OP_GTGT) add_cmd_part(dst, cap, ">>");
}

int execute(tknll *head, char *homwd, char *prevwd, int bg) {
    if(head==NULL) return 0;

    sigset_t block_mask;
    sigset_t old_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

    int prev_pipe[2]={-1, -1};
    pid_t pids[MAX_JOB_CMDS];
    int num_cmds=0;
    pid_t helpers[2*MAX_JOB_CMDS];
    int num_helpers=0;
    int exec_err_fds[MAX_JOB_CMDS];
    memset(exec_err_fds, -1, sizeof(exec_err_fds));
    char names[MAX_JOB_CMDS][64];
    memset(names, 0, sizeof(names));

    tknll *pipe_st=head;
    pid_t lead_pgid=-1;

    while(pipe_st!=NULL && pipe_st->type!=OP_SEMI && pipe_st->type!=OP_AMP) {
        int argc=0;
        tknll *ptr=pipe_st;
        int exp_redir=0;

        while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP) {
            if(ptr->type==OP_LT || ptr->type==OP_GT || ptr->type==OP_GTGT) exp_redir=1;
            else if(ptr->type==WORD) {
                if(exp_redir) exp_redir=0;
                else argc++;
            }
            ptr=ptr->next;
        }

        if(argc==0) break;
        if(num_cmds>=MAX_JOB_CMDS) {
            if(lead_pgid>0) {
                kill(-lead_pgid, SIGTERM);
                while(waitpid(-lead_pgid, NULL, 0)>0) {}
                if(bg==0) tcsetpgrp(STDIN_FILENO, getpid());
            }
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            return 1;
        }

        char **argv=malloc((argc+1)*sizeof(char *));
        if(argv==NULL) {
            if(lead_pgid>0) {
                kill(-lead_pgid, SIGTERM);
                while(waitpid(-lead_pgid, NULL, 0)>0) {}
                if(bg==0) tcsetpgrp(STDIN_FILENO, getpid());
            }
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            return 1;
        }

        ptr=pipe_st;
        exp_redir=0;
        int idx=0;

        while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP) {
            if(ptr->type==OP_LT || ptr->type==OP_GT || ptr->type==OP_GTGT) exp_redir=1;
            else if(ptr->type==WORD) {
                if(exp_redir) exp_redir=0;
                else argv[idx++]=ptr->tkn;
            }
            ptr=ptr->next;
        }
        argv[argc]=NULL;

        strncpy(names[num_cmds], argv[0], 63);
        names[num_cmds][63]='\0';

        pid_t in_pid=-1;
        int in_fd=inp_redir(pipe_st, &in_pid);
        pid_t out_pid=-1;
        int out_fd=out_redir(pipe_st, &out_pid);

        int curr_pipe[2]={-1, -1};
        if(ptr!=NULL && ptr->type==OP_PIPE) {
            if(pipe(curr_pipe)==-1) {
                perror("pipe");
                if(in_fd!=STDIN_FILENO && in_fd!=-1) close(in_fd);
                if(out_fd!=STDOUT_FILENO && out_fd!=-1) close(out_fd);
                if(prev_pipe[0]!=-1) close(prev_pipe[0]);
                if(prev_pipe[1]!=-1) close(prev_pipe[1]);
                if(in_pid!=-1) { kill(in_pid, SIGTERM); waitpid(in_pid, NULL, 0); }
                if(out_pid!=-1) { kill(out_pid, SIGTERM); waitpid(out_pid, NULL, 0); }
                if(lead_pgid>0) {
                    kill(-lead_pgid, SIGTERM);
                    while(waitpid(-lead_pgid, NULL, 0)>0) {}
                    if(bg==0) tcsetpgrp(STDIN_FILENO, getpid());
                }
                free(argv);
                sigprocmask(SIG_SETMASK, &old_mask, NULL);
                return 1;
            }
        }

        if(in_fd==-1 || out_fd==-1) {
            if(in_fd!=-1 && in_fd!=STDIN_FILENO) close(in_fd);
            if(out_fd!=-1 && out_fd!=STDOUT_FILENO) close(out_fd);
            if(prev_pipe[0]!=-1) close(prev_pipe[0]);
            if(prev_pipe[1]!=-1) close(prev_pipe[1]);
            if(curr_pipe[0]!=-1) close(curr_pipe[0]);
            if(curr_pipe[1]!=-1) close(curr_pipe[1]);
            if(in_pid!=-1) { kill(in_pid, SIGTERM); waitpid(in_pid, NULL, 0); }
            if(out_pid!=-1) { kill(out_pid, SIGTERM); waitpid(out_pid, NULL, 0); }
            if(lead_pgid>0) {
                kill(-lead_pgid, SIGTERM);
                while(waitpid(-lead_pgid, NULL, 0)>0) {}
                if(bg==0) tcsetpgrp(STDIN_FILENO, getpid());
            }
            free(argv);
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            return 1;
        }

        int err_pipe[2]={-1,-1};
        if(bg==0) {
            if(pipe(err_pipe)==-1) {
                perror("pipe");
                if(in_fd!=STDIN_FILENO) close(in_fd);
                if(out_fd!=STDOUT_FILENO) close(out_fd);
                if(prev_pipe[0]!=-1) close(prev_pipe[0]);
                if(prev_pipe[1]!=-1) close(prev_pipe[1]);
                if(curr_pipe[0]!=-1) close(curr_pipe[0]);
                if(curr_pipe[1]!=-1) close(curr_pipe[1]);
                if(in_pid!=-1) { kill(in_pid, SIGTERM); waitpid(in_pid, NULL, 0); }
                if(out_pid!=-1) { kill(out_pid, SIGTERM); waitpid(out_pid, NULL, 0); }
                if(lead_pgid>0) {
                    kill(-lead_pgid, SIGTERM);
                    while(waitpid(-lead_pgid, NULL, 0)>0) {}
                    if(bg==0) tcsetpgrp(STDIN_FILENO, getpid());
                }
                free(argv);
                sigprocmask(SIG_SETMASK, &old_mask, NULL);
                return 1;
            }

            int flags=fcntl(err_pipe[1], F_GETFD);
            if(flags!=-1) fcntl(err_pipe[1], F_SETFD, flags|FD_CLOEXEC);
        }

        char *cmd=argv[0];
        char exec_path[PATH_MAX+5]="";
        int use_path=0;

        if(cmd[0]=='%') {
            cmd++;
            argv[0]=cmd;
            use_path=1;
        }
        else if(strchr(cmd, '/')!=NULL) strcpy(exec_path, cmd);
        else {
            char cwd_path[PATH_MAX+5];
            snprintf(cwd_path, sizeof(cwd_path), "./%s", cmd);

            if(access(cwd_path, X_OK)==0) strcpy(exec_path, cwd_path);
            else use_path=1;
        }

        pid_t pid=fork();
        if(pid==-1) {
            perror("fork");
            if(in_fd!=STDIN_FILENO) close(in_fd);
            if(out_fd!=STDOUT_FILENO) close(out_fd);
            if(prev_pipe[0]!=-1) close(prev_pipe[0]);
            if(prev_pipe[1]!=-1) close(prev_pipe[1]);
            if(curr_pipe[0]!=-1) close(curr_pipe[0]);
            if(curr_pipe[1]!=-1) close(curr_pipe[1]);
            if(in_pid!=-1) { kill(in_pid, SIGTERM); waitpid(in_pid, NULL, 0); }
            if(out_pid!=-1) { kill(out_pid, SIGTERM); waitpid(out_pid, NULL, 0); }
            if(lead_pgid>0) {
                kill(-lead_pgid, SIGTERM);
                while(waitpid(-lead_pgid, NULL, 0)>0) {}
                if(bg==0) tcsetpgrp(STDIN_FILENO, getpid());
            }
            if(err_pipe[0]!=-1) close(err_pipe[0]);
            if(err_pipe[1]!=-1) close(err_pipe[1]);
            free(argv);
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            return 1;
        }

        if(pid==0) {
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            signal(SIGCHLD, SIG_DFL);
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            if(err_pipe[0]!=-1) close(err_pipe[0]);
            if(lead_pgid==-1) lead_pgid=getpid();
            setpgid(0, lead_pgid);

            if(prev_pipe[0]!=-1) dup2(prev_pipe[0], STDIN_FILENO);
            if(curr_pipe[1]!=-1) dup2(curr_pipe[1], STDOUT_FILENO);

            if(in_fd!=STDIN_FILENO) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            } else if(bg!=0 && prev_pipe[0]==-1) {
                int null_fd=open("/dev/null", O_RDONLY);
                if(null_fd!=-1) {
                    dup2(null_fd, STDIN_FILENO);
                    close(null_fd);
                }
            }

            if(out_fd!=STDOUT_FILENO) {
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }

            if(prev_pipe[0]!=-1) close(prev_pipe[0]);
            if(prev_pipe[1]!=-1) close(prev_pipe[1]);
            if(curr_pipe[0]!=-1) close(curr_pipe[0]);
            if(curr_pipe[1]!=-1) close(curr_pipe[1]);

            if(strcmp(cmd, "hop")==0) { hop(pipe_st, homwd, prevwd); fflush(stdout); _exit(0); }
            else if(strcmp(cmd, "reveal")==0) { reveal(pipe_st, homwd, prevwd); fflush(stdout); _exit(0); }
            else if(strcmp(cmd, "peek")==0) { peek(pipe_st); fflush(stdout); _exit(0); }
            else if(strcmp(cmd, "locate")==0) { locate(pipe_st); fflush(stdout); _exit(0); }
            else if(strcmp(cmd, "activities")==0) { activities(pipe_st); fflush(stdout); _exit(0); }

            if(use_path!=0) execvp(cmd, argv);
            else execv(exec_path, argv);

            if(err_pipe[1]!=-1) {
                char failed=1;
                write(err_pipe[1], &failed, 1);
            }
            fprintf(stderr, "cshell: command not found (%s)\n", cmd);
            fflush(stdout);
            free(argv);
            _exit(1);
        }

        if(err_pipe[1]!=-1) close(err_pipe[1]);
        if(err_pipe[0]!=-1) exec_err_fds[num_cmds]=err_pipe[0];

        if(lead_pgid==-1) lead_pgid=pid;
        setpgid(pid, lead_pgid);

        if(in_pid!=-1) {
            setpgid(in_pid, lead_pgid);
            helpers[num_helpers++]=in_pid;
        }

        if(out_pid!=-1) {
            setpgid(out_pid, lead_pgid);
            helpers[num_helpers++]=out_pid;
        }

        if(bg==0) tcsetpgrp(STDIN_FILENO, lead_pgid);

        pids[num_cmds++]=pid;

        if(in_fd!=STDIN_FILENO) close(in_fd);
        if(out_fd!=STDOUT_FILENO) close(out_fd);
        if(prev_pipe[0]!=-1) close(prev_pipe[0]);
        if(prev_pipe[1]!=-1) close(prev_pipe[1]);

        prev_pipe[0]=curr_pipe[0];
        prev_pipe[1]=curr_pipe[1];
        free(argv);

        if(ptr!=NULL && ptr->type==OP_PIPE) pipe_st=ptr->next;
        else break;
    }

    if(prev_pipe[0]!=-1) close(prev_pipe[0]);
    if(prev_pipe[1]!=-1) close(prev_pipe[1]);

    int failed=0;

    if(bg==0) {
        int stopped=0;
        int done_count=0;
        int total_count=num_cmds+num_helpers;
        int status=0;

        while(done_count<total_count) {
            pid_t waited=waitpid(-lead_pgid, &status, WUNTRACED);
            if(waited<0) {
                if(errno==EINTR) continue;
                break;
            }

            if(WIFSTOPPED(status)) {
                stopped=1;
                break;
            }

            if(WIFEXITED(status) || WIFSIGNALED(status)) {
                done_count++;
                for(int i=0;i<num_cmds;i++) {
                    if(pids[i]==waited) {
                        pids[i]=-1;
                        break;
                    }
                }
                for(int i=0;i<num_helpers;i++) {
                    if(helpers[i]==waited) {
                        helpers[i]=-1;
                        break;
                    }
                }
            }
        }

        for(int i=0;i<num_cmds;i++) {
            if(exec_err_fds[i]!=-1) {
                char failed_byte=0;
                ssize_t n=read(exec_err_fds[i], &failed_byte, 1);
                if(n>0 && failed_byte!=0) failed=1;
                close(exec_err_fds[i]);
                exec_err_fds[i]=-1;
            }
        }

        tcsetpgrp(STDIN_FILENO, getpid());

        if(stopped) {
            char full_cmd[1024]="";
            tknll *tmp=head;
            while(tmp!=NULL && tmp->type!=OP_SEMI && tmp->type!=OP_AMP) {
                add_tkn_txt(full_cmd, sizeof(full_cmd), tmp);
                tmp=tmp->next;
            }

            int len=strlen(full_cmd);
            if(len>0 && full_cmd[len-1]==' ') full_cmd[len-1]='\0';

            if(lead_pgid>0) {
                int job_pids_cnt=0;
                pid_t job_pids[MAX_JOB_PIDS];
                for(int i=0;i<num_cmds;i++) {
                    if(pids[i]>0) job_pids[job_pids_cnt++]=pids[i];
                }
                add_job(lead_pgid, job_pids, job_pids_cnt, names, full_cmd);
                bg_jobs[job_cnt-1].state=0;
                printf("\n[%d] + Stopped    %s\n", next_job_num-1, full_cmd);
            }
        }
    } else {
        char full_cmd[1024]="";
        tknll *tmp=head;
        while(tmp!=NULL && tmp->type!=OP_SEMI && tmp->type!=OP_AMP) {
            add_tkn_txt(full_cmd, sizeof(full_cmd), tmp);
            tmp=tmp->next;
        }

        if(num_cmds>0) {
            int len=strlen(full_cmd);
            if(len>0 && full_cmd[len-1]==' ') full_cmd[len-1]='\0';

            pid_t job_pids[MAX_JOB_PIDS];
            int job_pids_cnt=0;
            for(int i=0;i<num_cmds;i++) job_pids[job_pids_cnt++]=pids[i];
            add_job(lead_pgid, job_pids, job_pids_cnt, names, full_cmd);
            printf("[%d] %d\n", next_job_num-1, lead_pgid);
        }
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    return failed;
}