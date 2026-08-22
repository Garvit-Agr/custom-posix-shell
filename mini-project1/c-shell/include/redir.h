#ifndef REDIR_H
#define REDIR_H

#include "parser.h"
#include "unistd.h"

int inp_redir(tknll *head, pid_t *helper_pid);

#endif