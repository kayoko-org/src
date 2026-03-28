#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>

int main() {
    // Get the UID of the user running the command
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);

    if (!pw) {
        fprintf(stderr, "Could not identify user.\n");
        return 1;
    }

    char *user = pw->pw_name;

    // Logic is now hardcoded and compiled, not read from a writable file
    if (strcmp(user, "root") == 0) {
        printf("Welcome to Kayoko. Copyright (C) 2026 The Kayoko Project\n");
        printf("'You are root, be careful!' - a wise man\n");
    } else {
        printf("Welcome to Kayoko, %s. Copyright (C) 2026 The Kayoko Project\n", user);
    }

    return 0;
}
