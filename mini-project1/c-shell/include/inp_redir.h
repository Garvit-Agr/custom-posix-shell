#ifndef INP_REDIR_H
#define INP_REDIR_H

#include "parser.h"
#include <unistd.h>

int inp_redir(tknll *head, pid_t *helper_pid);

#endif