#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h> //included for "MAXPATHLEN"

#include "prompt.h"

#define MAX_USER_INPUT_ALLOWED 50 //used because the max input length in not mentioned


int main() {

    char homewd[MAXPATHLEN+5]; //found this in description of "man 2 getcwd"
    getcwd(homewd,MAXPATHLEN+5);

    
    while(1) {
        display_prompt(homewd);


    }


    return 0;
}