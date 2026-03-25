#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>

void get_aix_time(char *buf) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    // Format: Mon Mar 23 15:25:31 CDT 2026
    strftime(buf, 64, "%a %b %d %H:%M:%S %Z %Y", t);
}

void print_splash(const char *version, const char *pty) {
    char time_buf[64];
    get_aix_time(time_buf);

    printf("Last login: %s on /dev/%s\n", time_buf, pty);
    printf("*******************************************************************************\n");
    printf("*                                                                             *\n");
    printf("*                                                                             *\n");
    printf("* Welcome to Xai Version %-5s                                                *\n", version);
    printf("*                                                                             *\n");
    printf("*                                                                             *\n");
    printf("* Please see the README file in /usr/lpp/bos for information pertinent to     *\n");
    printf("* this release of the Xai Operating System.                                   *\n");
    printf("*                                                                             *\n");
    printf("*                                                                             *\n");
    printf("*******************************************************************************\n");
}

int main() {
    char user[32], pass[32], c_user[32], c_pass[32];
    struct termios t_old, t_new;
    char *xroot = getenv("XAIROOT") ? getenv("XAIROOT") : "/";

    while (1) {
        printf("Console login: ");
        fflush(stdout);
        if (scanf("%31s", user) == EOF) break;

        printf("%s's Password: ", user);
        fflush(stdout);

        // Mask Password
        tcgetattr(STDIN_FILENO, &t_old);
        t_new = t_old;
        t_new.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &t_new);
        scanf("%31s", pass);
        tcsetattr(STDIN_FILENO, TCSANOW, &t_old);
        printf("\n");

        // Auth Logic
        char shadow_path[256];
        snprintf(shadow_path, sizeof(shadow_path), "%s/etc/shadow", xroot);
        FILE *fp = fopen(shadow_path, "r");
        int authenticated = 0;

        if (fp) {
            while (fscanf(fp, "%[^:]:%s\n", c_user, c_pass) != EOF) {
                if (strcmp(user, c_user) == 0 && strcmp(pass, c_pass) == 0) {
                    authenticated = 1;
                    break;
                }
            }
            fclose(fp);
        }

        if (authenticated) {
            print_splash("7.3", "vty0");
            
            // Set up environment for ksh
            setenv("SHELL", "/bin/ksh", 1);
            setenv("HOME", "/home/root", 1);
            setenv("PS2", "> ", 1);
            setenv("PS1", "$(whoami)@$(hostname):$(pwd)> ", 1);
            
	    // Exec into ksh via env
	    execl("/bin/ksh", "-ksh", (char *)NULL);
	    perror("exec failed");
            exit(1);
        } else {
            sleep(2);
            printf("Login incorrect\n\n");
        }
    }
    return 0;
}
