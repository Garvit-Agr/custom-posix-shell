#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h> //included for "MAXPATHLEN"
#include <string.h>

#include "prompt.h"

#define MAX_USER_INPUT_ALLOWED 50 //used because the max input length in not mentioned


int main() {

    char homewd[MAXPATHLEN+5]; //found this in description of "man 2 getcwd"
    getcwd(homewd,MAXPATHLEN+5);

    
    while(1) {
        display_prompt(homewd);
        char input[MAX_USER_INPUT_ALLOWED+5];
        fgets(input, sizeof(input), stdin); // used this instead if scanf, so that multi-word sentences can be taken easily as an input
        input[strcspn(input,"\n")] = '\0';  // removed trailing "\n" in the "input" string

    }


    return 0;
}