#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/wait.h>
#include <limits.h>

/* POSIX limits */
#ifndef LINE_MAX
#define LINE_MAX 2048
#endif

static int flag_r = 0; // Do not run if empty
static int flag_t = 0; // Trace mode
static long max_args = -1;
static long max_chars = -1;

static void usage(void) {
    fprintf(stderr, "usage: xargs [-rt] [-n number] [-s size] [utility [argument...]]\n");
    exit(1);
}

static int execute(char **argv) {
    if (flag_t) {
        for (int i = 0; argv[i]; i++) fprintf(stderr, "%s%s", argv[i], argv[i+1] ? " " : "");
        fprintf(stderr, "\n");
    }

    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        perror("xargs: exec");
        _exit(127);
    } else if (pid < 0) {
        perror("xargs: fork");
        exit(1);
    }

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == 255) {
            fprintf(stderr, "xargs: utility exited with 255\n");
            exit(124);
        }
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) exit(125);
    return 0;
}

int main(int argc, char *argv[]) {
    int c;
    
    /* Set default max_chars based on POSIX {ARG_MAX}-2048 */
    long arg_max = sysconf(_SC_ARG_MAX);
    if (arg_max < 0) arg_max = 4096; 
    max_chars = arg_max - 2048;

    while ((c = getopt(argc, argv, "rtn:s:")) != -1) {
        switch (c) {
            case 'r': flag_r = 1; break;
            case 't': flag_t = 1; break;
            case 'n':
                max_args = strtol(optarg, NULL, 10);
                if (max_args <= 0) usage();
                break;
            case 's':
                max_chars = strtol(optarg, NULL, 10);
                break;
            default: usage();
        }
    }

    char *utility = (optind < argc) ? argv[optind] : "echo";
    int initial_args_cnt = (optind < argc) ? (argc - optind) : 1;
    
    /* We need to track current command line length (bytes + null terminators) */
    size_t base_len = 0;
    char **cmd_list = malloc(sizeof(char*) * (arg_max / 2)); // Oversized for safety
    int cur_idx = 0;

    if (optind < argc) {
        for (int i = optind; i < argc; i++) {
            cmd_list[cur_idx++] = argv[i];
            base_len += strlen(argv[i]) + 1;
        }
    } else {
        cmd_list[cur_idx++] = "echo";
        base_len += 5;
    }

    int base_idx = cur_idx;
    size_t cur_len = base_len;
    char buf[LINE_MAX];
    int char_in;
    int pos = 0, items_read = 0, total_items = 0;
    int quoted = 0, escaped = 0;

    while ((char_in = getchar()) != EOF) {
        if (escaped) {
            buf[pos++] = char_in;
            escaped = 0;
        } else if (char_in == '\\' && quoted != '\'') {
            escaped = 1;
        } else if (quoted) {
            if (char_in == quoted) quoted = 0;
            else buf[pos++] = char_in;
        } else if (char_in == '\'' || char_in == '"') {
            quoted = char_in;
        } else if (char_in == ' ' || char_in == '\t' || char_in == '\n') {
            if (pos > 0) {
                buf[pos] = '\0';
                size_t arg_sz = pos + 1;

                /* Check constraints: -n or -s */
                if ((max_args != -1 && items_read >= max_args) || 
                    (cur_len + arg_sz > max_chars)) {
                    cmd_list[cur_idx] = NULL;
                    execute(cmd_list);
                    for (int j = base_idx; j < cur_idx; j++) free(cmd_list[j]);
                    cur_idx = base_idx;
                    cur_len = base_len;
                    items_read = 0;
                }

                cmd_list[cur_idx++] = strdup(buf);
                cur_len += arg_sz;
                items_read++;
                total_items++;
                pos = 0;
            }
        } else {
            buf[pos++] = char_in;
        }
    }

    /* Final Flush */
    if (pos > 0) {
        buf[pos] = '\0';
        cmd_list[cur_idx++] = strdup(buf);
        total_items++;
    }

    if (total_items > 0) {
        cmd_list[cur_idx] = NULL;
        execute(cmd_list);
    } else if (!flag_r && optind < argc) {
        /* Run once even if empty unless -r is specified */
        cmd_list[cur_idx] = NULL;
        execute(cmd_list);
    }

    return 0;
}
