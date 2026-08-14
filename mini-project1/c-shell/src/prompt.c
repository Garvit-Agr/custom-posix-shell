#include <stdio.h>
#include <unistd.h>
#include <sys/param.h> //included for "MAXPATHLEN"
#include <string.h>

#include "prompt.h"

void display_prompt(char *homewd) {
    char *usr;
    usr=getlogin();

    char hst[(sysconf(_SC_HOST_NAME_MAX))+5]; //found this in description of "man gethostname"
    char cwd[MAXPATHLEN+5]; //found this in description of "man getcwd"

    gethostname(hst,(sysconf(_SC_HOST_NAME_MAX))+5);
    getcwd(cwd,MAXPATHLEN+5);
    
    if(usr==NULL) usr="unknown";
    
    if(strcmp(cwd,homewd)==0) strcpy(cwd,"~\0");
    else if(strlen(cwd)>strlen(homewd)) {

        if(strncmp(cwd,homewd,strlen(homewd))==0 && cwd[strlen(homewd)]=='/') {
            char temp[MAXPATHLEN+5];
            snprintf(temp, sizeof(temp), "~%s", cwd+strlen(homewd));
            strcpy(cwd,temp);
        }

    }

    printf("<%s@%s:%s> ",usr,hst,cwd);

}