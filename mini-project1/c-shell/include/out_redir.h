#ifndef OUT_REDIR_H
#define OUT_REDIR_H

#include "parser.h"
#include <unistd.h>

int out_redir(tknll *head, pid_t *helper_pid);

#endif