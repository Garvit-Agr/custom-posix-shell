#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/param.h> // used for MAXPATHLEN
#include <dirent.h> // used for DIR commands like opendir and closedir

#include "parser.h"
#include "cmds_hop.h"

void record_frecency(char *target_dir, char *homewd) {
    char db_path[MAXPATHLEN+25];
    snprintf(db_path, sizeof(db_path), "%s/.hop_history.csv", homewd);

    FILE *f=fopen(db_path, "r");
    
    int cap=10;
    int count=0;
    char **paths=malloc(cap * sizeof(char*));
    int *freqs=malloc(cap * sizeof(int));
    time_t *timest=malloc(cap * sizeof(time_t));
    
    int fnd=0;
    time_t curtime=time(NULL);

    if (f!=NULL) {
        char line[MAXPATHLEN+100];
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
                        paths=realloc(paths, cap * sizeof(char*));
                        freqs=realloc(freqs, cap * sizeof(int));
                        timest=realloc(timest, cap * sizeof(time_t));
                    }
                    
                    if (strcmp(stored_path, target_dir)==0) {
                        freqs[count]=freq + 1;
                        timest[count]=curtime;
                        fnd=1;
                    } else {
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
            paths=realloc(paths, cap * sizeof(char*));
            freqs=realloc(freqs, cap * sizeof(int));
            timest=realloc(timest, cap * sizeof(time_t));
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


int resolve_frecency(char *name, char *best_match, char *homewd) {
    char db_path[MAXPATHLEN+25];
    snprintf(db_path, sizeof(db_path), "%s/.hop_history.csv", homewd);

    FILE *f=fopen(db_path, "r");
    if (f==NULL) return 0;

    int best_freq=-1;
    time_t best_time=0;
    int matched=0;
    char line[MAXPATHLEN+100];

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
                        
                        if (!matched) {
                            best_freq=freq;
                            best_time=t;
                            strcpy(best_match, curpath);
                            matched=1;
                        } else {
                            if (freq>best_freq) {
                                best_freq=freq;
                                best_time=t;
                                strcpy(best_match, curpath);
                            }
                            else if (freq==best_freq && t > best_time) {
                                best_time=t;
                                strcpy(best_match, curpath);
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
        getcwd(prevwd, MAXPATHLEN+5);
        chdir(homwd);
        record_frecency(homwd, homwd);
        return;
    }
    
    while(ptr!=NULL) {
        char curwd[MAXPATHLEN+5];
        getcwd(curwd, MAXPATHLEN+5);
        int success=0;
        
        if(strcmp(ptr->tkn, "~")==0) {
            strcpy(prevwd, curwd);
            chdir(homwd);
            success=1;
        }
        else if(strcmp(ptr->tkn, ".")==0) {
            success=1; 
        }
        else if(strcmp(ptr->tkn, "..")==0) {
            strcpy(prevwd, curwd);
            chdir("..");
            success=1;
        }
       else if(strcmp(ptr->tkn, "-")==0) {
            if(prevwd[0]!='\0') {
                char target[MAXPATHLEN+5];
                strcpy(target, prevwd);
                
                strcpy(prevwd, curwd);
                
                chdir(target);
                success=1;
            }
        }
        else {
            if(chdir(ptr->tkn)==0) {
                strcpy(prevwd, curwd);
                success=1;
            }
            else {
                char best_match[MAXPATHLEN+5];
                if (resolve_frecency(ptr->tkn, best_match, homwd)) {
                    strcpy(prevwd, curwd);
                    chdir(best_match);
                    success=1;
                } else {
                    printf("hop: no such directory\n");
                }
            }
        }
        
        if (success) {
            char new_cwd[MAXPATHLEN+5];
            getcwd(new_cwd, MAXPATHLEN+5);
            record_frecency(new_cwd, homwd);
        }

        ptr=ptr->next;
    }
}