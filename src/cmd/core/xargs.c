#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 255
#define BUF_SIZE 4096

static int run(char **argv) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        perror("xargs");
        _exit(127);
    } else if (pid < 0) {
        perror("xargs: fork");
        return 1;
    }
    
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 255) {
        fprintf(stderr, "xargs: %s exited with status 255; aborting\n", argv[0]);
        exit(124);
    }
    return WEXITSTATUS(status);
}

int main(int argc, char *argv[]) {
    char *cmd_args[MAX_ARGS + 1];
    char arg_buf[BUF_SIZE];
    int cmd_argc = 0, i;
    int c, quoted = 0, escaped = 0;
    size_t pos = 0;

    /* Handle options - POSIX requires at least basic getopt support */
    while ((c = getopt(argc, argv, "")) != -1) {
        switch (c) {
            default:
                fprintf(stderr, "usage: xargs [utility [argument...]]\n");
                return 1;
        }
    }

    /* Set up initial command */
    if (optind < argc) {
        for (i = optind; i < argc && cmd_argc < MAX_ARGS; i++) {
            cmd_args[cmd_argc++] = argv[i];
        }
    } else {
        cmd_args[cmd_argc++] = "echo";
    }

    int base_argc = cmd_argc;

    while ((c = getchar()) != EOF) {
        if (escaped) {
            arg_buf[pos++] = c;
            escaped = 0;
        } else if (c == '\\' && quoted != '\'') {
            escaped = 1;
        } else if (quoted) {
            if (c == quoted) quoted = 0;
            else arg_buf[pos++] = c;
        } else if (c == '\'' || c == '"') {
            quoted = c;
        } else if (c == ' ' || c == '\t' || c == '\n') {
            if (pos > 0) {
                arg_buf[pos++] = '\0';
                cmd_args[cmd_argc++] = strdup(arg_buf);
                pos = 0;
                if (cmd_argc >= MAX_ARGS) {
                    cmd_args[cmd_argc] = NULL;
                    run(cmd_args);
                    for (i = base_argc; i < cmd_argc; i++) free(cmd_args[i]);
                    cmd_argc = base_argc;
                }
            }
        } else {
            arg_buf[pos++] = c;
        }
        
        if (pos >= BUF_SIZE - 1) {
            fprintf(stderr, "xargs: argument too long\n");
            return 1;
        }
    }

    /* Final flush of remaining arguments */
    if (pos > 0) {
        arg_buf[pos++] = '\0';
        cmd_args[cmd_argc++] = strdup(arg_buf);
    }

    if (cmd_argc > base_argc || (optind >= argc && cmd_argc == base_argc)) {
        cmd_args[cmd_argc] = NULL;
        run(cmd_args);
        for (i = base_argc; i < cmd_argc; i++) free(cmd_args[i]);
    }

    return 0;
}
