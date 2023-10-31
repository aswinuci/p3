/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * shell.h
 */

#ifndef _SHELL_H_
#define _SHELL_H_

typedef int (*shell_fnc_t)(void *arg, const char *s);

void shell(shell_fnc_t fnc, void *arg);  /* If the above function returns 0 , keep going and read line , if non zero ,stop */

void shell_strtrim(char *s);

#endif /* _SHELL_H_ */