#ifndef KAYOKO_TEXTPROC_COMMANDS_H
#define KAYOKO_TEXTPROC_COMMANDS_H

#include <kayoko/textproc/ed.h>

/* Block Operations */
void move_lines(Ed *e, int a, int b, int t);
void transfer_lines(Ed *e, int a, int b, int t);

/* Complex Logic */
void do_global(Ed *e, int a, int b, char *p, int invert);
void do_shell(Ed *e, char *cmd_line);

/* The Dispatcher */
void exec_cmd(Ed *e, int a, int b, char *p, int *is_q, int has_a);
/* Join lines in range [a, b] into a single line */
void join_lines(Ed *e, int a, int b);

/* Move lines in range [a, b] to destination t (m) */
void move_lines(Ed *e, int a, int b, int t);

/* Copy/Transfer lines in range [a, b] to destination t (t) */
void transfer_lines(Ed *e, int a, int b, int t);

#endif
