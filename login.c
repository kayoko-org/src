#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

void get_aix_time(char *buf) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, 64, "%a %b %d %H:%M:%S %Z %Y", t);
}

int file_exists(const char *path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
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
    struct passwd *pw;

    while (1) {
        printf("Console login: ");
        fflush(stdout);
        if (scanf("%31s", user) == EOF) break;

        printf("%s's Password: ", user);
        fflush(stdout);
        tcgetattr(STDIN_FILENO, &t_old);
        t_new = t_old;
        t_new.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &t_new);
        scanf("%31s", pass);
        tcsetattr(STDIN_FILENO, TCSANOW, &t_old);
        printf("\n");

        // Auth Logic against /etc/shadow
        int auth = 0;
        FILE *fp = fopen("/etc/shadow", "r");
        if (fp) {
            while (fscanf(fp, "%[^:]:%s\n", c_user, c_pass) != EOF) {
                if (strcmp(user, c_user) == 0 && strcmp(pass, c_pass) == 0) {
                    auth = 1;
                    break;
                }
            }
            fclose(fp);
        }

        if (auth && (pw = getpwnam(user))) {
            print_splash(pw->pw_name, "vty0");

            // 1. Determine Home Directory with Fallback
            char *home = (pw->pw_dir && file_exists(pw->pw_dir)) ? pw->pw_dir : "/";
            chdir(home);

            // 2. Determine Shell with Fallback
            char *shell = (pw->pw_shell && file_exists(pw->pw_shell)) ? pw->pw_shell : "/bin/ksh";
            if (!file_exists(shell)) shell = "/bin/sh"; // Last resort

            // 3. Set Environment
            setenv("HOME", home, 1);
            setenv("SHELL", shell, 1);
            setenv("USER", pw->pw_name, 1);
            setenv("LOGNAME", pw->pw_name, 1);
            setenv("PATH", "/bin:/usr/bin", 0);

            // 4. Exec as Login Shell (the '-' prefix)
            execl(shell, "-ksh", (char *)NULL);

            perror("exec failed");
            exit(1);
        } else {
            sleep(2);
            printf("Login incorrect\n\n");
        }
    }
    return 0;
}
