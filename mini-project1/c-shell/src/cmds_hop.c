#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <dirent.h> // used for DIR commands like opendir and closedir

#include "parser.h"
#include "cmds_hop.h"

void record_frecency(char *target_dir, char *homewd) {
    char db_path[PATH_MAX+25];
    snprintf(db_path, sizeof(db_path), "%s/.hop_history.csv", homewd);

    FILE *f=fopen(db_path, "r");
    
    int cap=10;
    int count=0;
    char **paths=malloc(cap*sizeof(char*));
    int *freqs=malloc(cap*sizeof(int));
    time_t *timest=malloc(cap*sizeof(time_t));
    
    int fnd=0;
    time_t curtime=time(NULL);

    if (f!=NULL) {
        char line[PATH_MAX+100];
        while (fgets(line, sizeof(line), f)!=NULL) {
            line[strcspn(line, "\n")]='\0';
            
            char *comma1=strchr(line, ',');
            if (comma1!=NULL) {
                *comma1='\0';
                int freq=atoi(line);
                
                char *comma2=strchr(comma1+1, ',');
                if (comma2!=NULL) {
                    *comma2='\0';
                    time_t t=(time_t)atol(comma1+1);
                    char *stored_path=comma2+1;
                    
                    if (count>=cap) {
                        cap*=2;
                        paths=realloc(paths, cap*sizeof(char*));
                        freqs=realloc(freqs, cap*sizeof(int));
                        timest=realloc(timest, cap*sizeof(time_t));
                    }
                    
                    if (strcmp(stored_path, target_dir)==0) {
                        freqs[count]=freq+1;
                        timest[count]=curtime;
                        fnd=1;
                    }
                    else {
                        freqs[count]=freq;
                        timest[count]=t;
                    }
                    paths[count]=strdup(stored_path);
                    count++;
                }
            }
        }
        fclose(f);
    }

    if (fnd==0) {
        if (count>=cap) {
            cap*=2;
            paths=realloc(paths, cap*sizeof(char*));
            freqs=realloc(freqs, cap*sizeof(int));
            timest=realloc(timest, cap*sizeof(time_t));
        }
        paths[count]=strdup(target_dir);
        freqs[count]=1;
        timest[count]=curtime;
        count++;
    }

    f=fopen(db_path, "w");
    if (f!=NULL) {
        for (int i=0; i<count; i++) {
            fprintf(f, "%d,%ld,%s\n", freqs[i], (long)timest[i], paths[i]);
            free(paths[i]);
        }
        fclose(f);
    }
    
    free(paths);
    free(freqs);
    free(timest);
}


int resolve_frecency(char *name, char *best, char *homewd) {
    char db_path[PATH_MAX+25];
    snprintf(db_path, sizeof(db_path), "%s/.hop_history.csv", homewd);

    FILE *f=fopen(db_path, "r");
    if (f==NULL) return 0;

    double best_score=-1;
    time_t best_time=0;
    int matched=0;
    char line[PATH_MAX+100];
    time_t cur_time=time(NULL);

    while (fgets(line, sizeof(line), f)!=NULL) {
        line[strcspn(line, "\n")]='\0';
        
        char *comma1=strchr(line, ',');
        if (comma1!=NULL) {
            *comma1='\0';
            int freq=atoi(line);
            
            char *comma2=strchr(comma1+1, ',');
            if (comma2!=NULL) {
                *comma2='\0';
                time_t t=(time_t)atol(comma1+1);
                char *curpath=comma2+1;

                if (strstr(curpath, name)!=NULL) {
                    
                    DIR *dir=opendir(curpath);
                    
                    if (dir!=NULL) {
                        closedir(dir);
                        
                        double score=(double)freq;
                        double dt=difftime(cur_time, t);
                        if(dt<3600) score*=4.0;
                        else if(dt<86400) score*=2.0;
                        else if(dt<604800) score/=2.0;
                        else score/=4.0;
                        
                        if (!matched) {
                            best_score=score;
                            best_time=t;
                            strcpy(best, curpath);
                            matched=1;
                        }
                        else {
                            if (score>best_score) {
                                best_score=score;
                                best_time=t;
                                strcpy(best, curpath);
                            }
                            else if (score==best_score && t>best_time) {
                                best_time=t;
                                strcpy(best, curpath);
                            }
                        }
                    }
                }
            }
        }
    }
    fclose(f);
    return matched;
}


void hop(tknll *head, char *homwd, char *prevwd) {
    tknll *ptr=head;

    if(strcmp(ptr->tkn, "hop")!=0) return;
    
    ptr=ptr->next;

    if (ptr==NULL) {
        char curwd[PATH_MAX+5];
        getcwd(curwd, PATH_MAX+5);
        if (chdir(homwd)==0) {
            strcpy(prevwd, curwd);
            record_frecency(homwd, homwd);
        }
        return;
    }
    
    //fixing builtins ignoring operators, because of problem statement part A3
    while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP && ptr->type!=OP_LT && ptr->type!=OP_GT && ptr->type!=OP_GTGT) {
        char curwd[PATH_MAX+5];
        getcwd(curwd, PATH_MAX+5);
        int success=0;
        
        if(strcmp(ptr->tkn, "~")==0) {
            if(chdir(homwd)==0) {
                strcpy(prevwd, curwd);
                success=1;
            }
        }
        else if(strcmp(ptr->tkn, ".")==0) {
            //fixing hop . should not record frecency, because of doubt doc q23
        }
        else if(strcmp(ptr->tkn, "..")==0) {
            if(strcmp(curwd, "/")!=0 && chdir("..")==0) {
                strcpy(prevwd, curwd);
                success=1;
            }
        }
       else if(strcmp(ptr->tkn, "-")==0) {
            if(prevwd[0]!='\0') {
                char target[PATH_MAX+5];
                strcpy(target, prevwd);

                if (chdir(target)==0) {
                    strcpy(prevwd, curwd);
                    success=1;
                }
            }
        }
        else {
            if(chdir(ptr->tkn)==0) {
                strcpy(prevwd, curwd);
                success=1;
            }
            else {
                char best[PATH_MAX+5];
                if (resolve_frecency(ptr->tkn, best, homwd)) {
                    if(chdir(best)==0) {
                        strcpy(prevwd, curwd);
                        success=1;
                    }
                }
                else {
                    printf("hop: no such directory\n");
                    return;
                }
            }
        }
        
        if (success) {
            char new_cwd[PATH_MAX+5];
            getcwd(new_cwd, PATH_MAX+5);
            record_frecency(new_cwd, homwd);
        }

        ptr=ptr->next;
    }
}