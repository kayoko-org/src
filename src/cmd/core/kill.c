#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>

struct {
    const char *name;
    int num;
} sigs[] = {
    {"HUP", SIGHUP}, {"INT", SIGINT}, {"QUIT", SIGQUIT},
    {"ILL", SIGILL}, {"TRAP", SIGTRAP}, {"ABRT", SIGABRT},
    {"FPE", SIGFPE}, {"KILL", SIGKILL}, {"USR1", SIGUSR1},
    {"SEGV", SIGSEGV}, {"USR2", SIGUSR2}, {"PIPE", SIGPIPE},
    {"ALRM", SIGALRM}, {"TERM", SIGTERM}, {"CHLD", SIGCHLD},
    {"CONT", SIGCONT}, {"STOP", SIGSTOP}, {"TSTP", SIGTSTP},
    {NULL, 0}
};

void list_signals() {
    for (int i = 0; sigs[i].name; i++) {
        printf("%s%c", sigs[i].name, (i + 1) % 8 == 0 ? '\n' : ' ');
    }
    printf("\n");
}

int name_to_sig(const char *name) {
    if (isdigit(*name)) return atoi(name);
    if (strncasecmp(name, "SIG", 3) == 0) name += 3;
    for (int i = 0; sigs[i].name; i++) {
        if (strcasecmp(name, sigs[i].name) == 0) return sigs[i].num;
    }
    return -1;
}

int main(int argc, char *argv[]) {
    int sig = SIGTERM;
    int i = 1;

    if (argc < 2) {
        fprintf(stderr, "usage: kill [-s signal_name | -signal_number] pid ...\n");
        return 2;
    }

    if (strcmp(argv[1], "-l") == 0) {
        list_signals();
        return 0;
    }

    if (argv[1][0] == '-') {
        const char *sname = argv[1] + 1;
        if (strcmp(sname, "s") == 0 && argc > 2) {
            sig = name_to_sig(argv[2]);
            i = 3;
        } else {
            sig = name_to_sig(sname);
            i = 2;
        }
    }

    if (sig < 0) {
        fprintf(stderr, "kill: unknown signal\n");
        return 2;
    }

    if (i >= argc) return 2;

    int ret = 0;
    for (; i < argc; i++) {
        pid_t pid = (pid_t)atol(argv[i]);
        if (kill(pid, sig) == -1) {
            perror(argv[i]);
            ret = 1;
        }
    }

    return ret;
}
