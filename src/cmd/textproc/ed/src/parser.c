#include <kayoko/textproc/ed.h>
#include <kayoko/textproc/ed/parser.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <unistd.h>
#include <regex.h>

int get_addr(Ed *e, char **s, int *addr) {
    while (isspace(**s)) (*s)++;
    if (**s == '.') { (*s)++; *addr = e->curr; }
    else if (**s == '$') { (*s)++; *addr = (int)e->count; }
    else if (isdigit(**s)) { *addr = (int)strtol(*s, s, 10); }
    else if (**s == '\'') { (*s)++; *addr = e->lines[e->curr-1].marks[**s - 'a']; (*s)++; }
    else return 0;

    while (**s == '+' || **s == '-' || isdigit(**s)) {
        int op = (**s == '-') ? -1 : 1;
        if (**s == '+' || **s == '-') (*s)++;
        int rel = isdigit(**s) ? (int)strtol(*s, s, 10) : 1;
        *addr += (op * rel);
    }
    return 1;
}
