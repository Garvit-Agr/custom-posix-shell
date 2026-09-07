#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h> //included for "MAXPATHLEN"
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>

#include "prompt.h"
#include "parser.h"
#include "cmds_hop.h"
#include "cmds_reveal.h"
#include "cmds_peek.h"
#include "cmds_locate.h"
#include "exec.h"
#include "inp_redir.h"
#include "out_redir.h"

#define MAX_USER_INPUT_ALLOWED 1024


int main() {

    char homewd[MAXPATHLEN+5]; //found this in description of "man getcwd"
    getcwd(homewd,MAXPATHLEN+5);
    
    char prevwd[MAXPATHLEN+5]={0}; //found this in description of "man getcwd"
    
    while(1) {
        display_prompt(homewd);
        char input[MAX_USER_INPUT_ALLOWED+5];
        
        if (fgets(input, sizeof(input), stdin)==NULL) { // used this instead if scanf, so that multi-word sentences can be taken easily as an input
                                                        // put "if" to bypass ctrl+d issue.
            printf("\n");
            break;
        }
        input[strcspn(input,"\n")]='\0';

        tknll *head=lexer(input);

        if(head==NULL) continue;

        if(valid_grmr(head)==false) {
            printf("cshell: invalid syntax\n");
            free_tkn_ll(head);
            continue;
        }


        
        tknll *curr_cmd=head;

        while(curr_cmd!=NULL) {
            if(curr_cmd->type==OP_SEMI) {
                curr_cmd=curr_cmd->next;
                continue;
            }
            if(curr_cmd->type==OP_AMP) {
                curr_cmd=curr_cmd->next;
                continue;
            }

            int bg=0;
            tknll *check_bg=curr_cmd;
            while(check_bg!=NULL && check_bg->type!=OP_SEMI) {
                if(check_bg->type==OP_AMP) {
                    bg=1;
                    break;
                }
                check_bg=check_bg->next;
            }

            int is_builtin=(strcmp(curr_cmd->tkn,"hop")==0 || strcmp(curr_cmd->tkn,"reveal")==0 || strcmp(curr_cmd->tkn,"peek")==0 || strcmp(curr_cmd->tkn,"locate")==0);
            
            int has_pipe=0;
            tknll *tmp=curr_cmd;
            while(tmp!=NULL && tmp->type!=OP_SEMI && tmp->type!=OP_AMP) {
                if(tmp->type==OP_PIPE) has_pipe=1;
                tmp=tmp->next;
            }

            int exec_res=0;

            if (is_builtin && !has_pipe) {
                pid_t in_pid=-1, out_pid=-1;
                int in_fd=inp_redir(curr_cmd, &in_pid);
                int out_fd=out_redir(curr_cmd, &out_pid);
                
                if (in_fd!=-1 && out_fd!=-1) {
                    int saved_stdin=dup(STDIN_FILENO);
                    int saved_stdout=dup(STDOUT_FILENO);

                    if (in_fd!=STDIN_FILENO) dup2(in_fd, STDIN_FILENO);
                    if (out_fd!=STDOUT_FILENO) dup2(out_fd, STDOUT_FILENO);

                    if(strcmp(curr_cmd->tkn,"hop")==0) hop(curr_cmd, homewd, prevwd);
                    else if(strcmp(curr_cmd->tkn,"reveal")==0) reveal(curr_cmd, homewd, prevwd);
                    else if(strcmp(curr_cmd->tkn,"peek")==0) peek(curr_cmd, homewd, prevwd);
                    else if(strcmp(curr_cmd->tkn,"locate")==0) locate(curr_cmd, homewd, prevwd);

                    fflush(stdout);

                    dup2(saved_stdin, STDIN_FILENO);
                    dup2(saved_stdout, STDOUT_FILENO);
                    close(saved_stdin);
                    close(saved_stdout);
                }
                if(in_fd!=-1 && in_fd!=STDIN_FILENO) close(in_fd);
                if(out_fd!=-1 && out_fd!=STDOUT_FILENO) close(out_fd);
                if(in_pid!=-1) waitpid(in_pid, NULL, 0);
                if(out_pid!=-1) waitpid(out_pid, NULL, 0);
            }
            else {
                exec_res=execute(curr_cmd, homewd, prevwd, bg);
            }

            if(exec_res==1) break;

            while(curr_cmd!=NULL && curr_cmd->type!=OP_SEMI && curr_cmd->type!=OP_AMP) {
                curr_cmd=curr_cmd->next;
            }
            
            if(curr_cmd!=NULL && (curr_cmd->type==OP_SEMI || curr_cmd->type==OP_AMP)) {
                curr_cmd=curr_cmd->next;
            }
        }





        free_tkn_ll(head);
    }


    return 0;
}