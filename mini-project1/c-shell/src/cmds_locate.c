#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/param.h>

#include "parser.h"
#include "cmds_locate.h"

void locate(tknll *head, char *homwd, char *prevwd) {
    (void)homwd;
    (void)prevwd;

    tknll *ptr=head->next;

    if(ptr==NULL) {
        printf("locate: invalid syntax\n");
        return;
    }

    char cwd[MAXPATHLEN+5];
    getcwd(cwd, MAXPATHLEN+5);

    //fixing builtins ignoring operators, because of problem statement part A3
    while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP && ptr->type!=OP_LT && ptr->type!=OP_GT && ptr->type!=OP_GTGT) {
        char *tgt=ptr->tkn;
        int fnd=0;
        
        char full_path[(2*MAXPATHLEN)+10]; 

        snprintf(full_path, sizeof(full_path), "%s/%s", cwd, tgt);
        
        DIR *dir_check=opendir(full_path);
        if(dir_check!=NULL) closedir(dir_check);

        else if(access(full_path, X_OK)==0) {
                printf("%s\n", full_path);
                fnd=1;
        }

        if(!fnd) {
            char *path_env=getenv("PATH");
            if(path_env!=NULL) {
                char *path_dup=strdup(path_env);
                char *dir=strtok(path_dup, ":");

                while(dir!=NULL) {
                    if(dir[0]=='/') snprintf(full_path, sizeof(full_path), "%s/%s", dir, tgt);
                    
                    else snprintf(full_path, sizeof(full_path), "%s/%s/%s", cwd, dir, tgt);
                    

                    DIR *d_check=opendir(full_path);
                    if(d_check!=NULL) closedir(d_check);

                    else if(access(full_path, X_OK)==0) {
                            printf("%s\n", full_path);
                            fnd=1;
                            break;
                    }
                    
                    dir=strtok(NULL, ":");
                }
                free(path_dup);
            }
        }

        if(fnd==0) printf("locate: command not found (%s)\n", tgt);

        ptr=ptr->next;
    }
}