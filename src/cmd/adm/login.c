#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <sys/types.h>
#include <pwd.h>

/* * Manually prototype crypt if not in unistd.h to stop the pointer truncation 
 * that causes the memory fault.
 */
extern char *crypt(const char *, const char *);
extern char *getpass(const char *);

void get_aix_time(char *buf) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) {
        strftime(buf, 64, "%a %b %d %H:%M:%S %Z %Y", t);
    } else {
        strcpy(buf, "Unknown Time");
    }
}

void print_splash(const char *user) {
    char time_buf[64];
    get_aix_time(time_buf);
    (void)user; /* Reserve for future 'Last login for %s' logic */
    printf("Last login: %s on vty0\n", time_buf);
    printf("Welcome to openXao. (C) 2026 The openXao Project.\n\n");
}

int main(int argc, char *argv[]) {
    char user[32], *pass, *hashed;
    struct passwd *pw;
    int unauth_mode = 0;

    if (getuid() != 0) {
        fprintf(stderr, "Only root may use this binary.\n");
        exit(1);
    }

    if (argc > 1 && strcmp(argv[1], "-a") == 0) {
        unauth_mode = 1;
    }

    while (1) {
        struct termios t;
	char prompt[64]; /* Buffer for the dynamic password prompt */
        tcgetattr(STDIN_FILENO, &t);
        t.c_lflag |= ECHO; 
        tcsetattr(STDIN_FILENO, TCSANOW, &t);

        printf("Console login: ");
	if (scanf("%31s", user) <= 0) {
            /* If they hit Ctrl-D or there's an error, exit loop */
            if (feof(stdin)) break;
            continue; 
        }
	fflush(stdout);
        pw = getpwnam(user);

        if (unauth_mode) {
            if (pw) goto authenticated;
	} else {
            /* Format the prompt: "root's Password: " */
            snprintf(prompt, sizeof(prompt), "%s's Password: ", user);

            /* getpass uses the custom prompt and hides the typing */
            pass = getpass(prompt);
            
            /*  Print newline because getpass won't move the cursor */
            printf("\n");
            /* * Safety: check pw and pw_passwd existence before passing to crypt
             * to prevent segfaults on accounts with no password field.
             */
            if (pw && pw->pw_passwd && pass) {
                hashed = crypt(pass, pw->pw_passwd);
                if (hashed && strcmp(hashed, pw->pw_passwd) == 0) {
                    goto authenticated;
                }
            }
        }

        sleep(2);
        printf("Login incorrect\n\n");
        continue;

authenticated:
        print_splash(user);

        /* Final safety check: ensure pw is valid before dropping privs */
        if (!pw) {
            fprintf(stderr, "Internal error: User vanished.\n");
            exit(1);
        }

        if (setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
            perror("! ! Failed to drop privileges ! !");
            exit(1);
        }

        /* Standardize environment */
        if (pw->pw_dir) {
            if (chdir(pw->pw_dir) != 0) {
                (void)chdir("/");
            }
            setenv("HOME", pw->pw_dir, 1);
        }
        
        setenv("USER", pw->pw_name, 1);
        setenv("LOGNAME", pw->pw_name, 1);
        setenv("SHELL", "/bin/ksh", 1);
        setenv("PATH", "/bin:/usr/bin:/sbin:/usr/sbin", 1);

        /* Standard Unix practice: argv[0] starts with '-' for login shells */
        execl("/bin/ksh", "-ksh", (char *)NULL);
        
        perror("exec failed");
        exit(1);
    }
    return 0;
}
