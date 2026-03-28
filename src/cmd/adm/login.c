#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <sys/types.h>
#include <pwd.h>
#include <login_cap.h>
#include <ttyent.h>

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
    printf("Last login at: %s\n", time_buf);
}

int main(int argc, char *argv[]) {
    char user[32], *pass, *hashed;
    struct passwd *pw;
    int unauth_mode = 0;

    if (getuid() != 0) {
        fprintf(stderr, "Only root may use this binary.\n");
        exit(1);
    }
	
    if (getenv("KAYAKO_SESS") != NULL) {
    fprintf(stderr, "Nesting prohibited.\n");
    exit(1);
    }
	    char *tty = ttyname(STDIN_FILENO);
	    struct ttyent *ty;
	
	    if (tty) {
	        // Strip the "/dev/" prefix if present for getttynam
	        char *tty_short = (strncmp(tty, "/dev/", 5) == 0) ? tty + 5 : tty;
	        ty = getttynam(tty_short);

	        // Check if the terminal exists in /etc/ttys and has the "secure" flag
	        if (ty && (ty->ty_status & TTY_SECURE)) {
 		printf("");
		} else {
	            fprintf(stderr, "%s is not a secure terminal.\n", tty);
	            exit(1);
	        }
	    } else {
	        fprintf(stderr, "Could not determine TTY.\n");
	        exit(1);
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
	printf("\033[H\033[2J");
	fflush(stdout);
	print_splash(user);

    /* 1. Retrieve the login class for this user */
    login_cap_t *lc = login_getpwclass(pw);
    if (lc == NULL) {
        /* Fallback to 'default' class if user's class isn't found */
        lc = login_getclass("default");
    }

    /* 2. Apply resource limits (datasize, maxproc, etc.) */
    if (setusercontext(lc, pw, pw->pw_uid, LOGIN_SETALL) != 0) {
        perror("setusercontext failed");
        exit(1);
    }

    /* 3. Handle Environment from login.conf */
    /* If path is defined in login.conf, use it. Otherwise, use your fallback. */
    const char *path = login_getcapstr(lc, "path", NULL, NULL);
    if (path) {
        setenv("PATH", path, 1);
    } else {
        setenv("PATH", "/bin:/usr/bin:/sbin:/usr/sbin", 1);
    }

    /* Set other basics */
    setenv("USER", pw->pw_name, 1);
    setenv("LOGNAME", pw->pw_name, 1);
    setenv("HOME", pw->pw_dir, 1);
    setenv("SHELL", pw->pw_shell[0] ? pw->pw_shell : "/bin/ksh", 1);

    /* 4. Cleanup the cap structure */
    login_close(lc);

    /* 1. Determine the shell: use passwd file, or fallback to ksh */
    const char *shell_path = (pw->pw_shell && pw->pw_shell[0]) ? pw->pw_shell : "/bin/ksh";

    /* 2. Update the environment so the child process knows its shell */
    setenv("SHELL", shell_path, 1);

    /* 3. Execute the shell. 
     * Convention: argv[0] starts with '-' to tell the shell it's a login shell.
     * We use strrchr to get just the base name (e.g., "ksh" from "/bin/ksh").
     */
    char login_argv0[64];
    const char *shell_name = strrchr(shell_path, '/');
    shell_name = (shell_name) ? shell_name + 1 : shell_path;
    
    snprintf(login_argv0, sizeof(login_argv0), "-%s", shell_name);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
    } else if (pid == 0) {
        /* Child process: Drop privileges to the user's level */
        if (setgid(pw->pw_gid) != 0) {
            perror("setgid failed");
            exit(1);
        }
        if (setuid(pw->pw_uid) != 0) {
            perror("setuid failed");
            exit(1);
        }

        /* Execute motd as the user */
        execl("/sbin/motd", "motd", (char *)NULL);
        
        /* If execl fails, exit child so we don't end up with two shells */
        exit(1);
    } else {
        /* Parent process: Wait for motd to finish before exec'ing the shell */
        int status;
        waitpid(pid, &status, 0);
    }

    /* Perform the exec */
    execl(shell_path, login_argv0, (char *)NULL);

    /* If execl returns, it failed */
    perror("exec failed");
    exit(1);

    return 0;
	}
}
