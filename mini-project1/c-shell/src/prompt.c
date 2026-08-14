#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h> //included for "(sysconf(_SC_HOST_NAME_MAX))"
#include <sys/param.h> //included for "MAXPATHLEN"
#include <string.h>

#include "prompt.h"

void display_prompt(char *homewd) {
    char *usr;
    usr=getlogin();

    char hst[(sysconf(_SC_HOST_NAME_MAX))+5]; //found this in description of "man 2 gethostname"
    char cwd[MAXPATHLEN+5]; //found this in description of "man 2 getcwd"

    gethostname(hst,(sysconf(_SC_HOST_NAME_MAX))+5);
    getcwd(cwd,MAXPATHLEN+5);
    
    
    if(strcmp(cwd,homewd)==0) strcpy(cwd,"~\0");
    else if(strlen(cwd)>strlen(homewd)) {

        if(strncmp(cwd,homewd,strlen(homewd))==0) strcpy(cwd,strcat("~/",cwd+strlen(homewd)));

    }

    printf("<%s@%s:%s> ",usr,hst,cwd);

}