#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

typedef struct {
    const char *label;
    const char *cmd_template;
    int needs_input;
    const char *prompt;
} entry_t;

static const entry_t db[] = {
    /* --- EMERGENCY & STATE --- */
    { "Sync Disks",          "sync; sync", 0, NULL },
    { "Remount Root R/O",    "mount -u -o ro /", 0, NULL },
    { "Remount Root R/W",    "mount -u -o rw /", 0, NULL },
    { "Reboot System",       "reboot", 0, NULL },
    { "Halt System",         "halt -p", 0, NULL },

    /* --- USER & GROUP MANAGEMENT --- */
    { "Add New User",        "useradd -m %s", 1, "Username to add: " },
    { "Modify User",   "usermod %s", 1, "Username and flags (e.g. -G wheel joe): " },
    { "Set User Password",   "passwd %s", 1, "Username to change password: " },
    { "Delete User",         "userdel -r %s", 1, "Username to remove: " },
    { "Add New Group",       "groupadd %s", 1, "Group name to add: " },
    { "Delete Group",        "groupdel %s", 1, "Group name to remove: " },
    { "Lock User Account",   "userconf -L %s", 1, "Username to lock: " },

    /* --- PROCESS CONTROL --- */
    { "List All Processes",  "ps -auwwx", 0, NULL },
    { "Kill PID (-9)",       "kill -9 %s", 1, "PID: " },
    { "Kill All Processes",  "kill -9 -1", 0, NULL },

    /* --- NETWORK --- */
    { "Interface Status",    "ifconfig %s", 1, "IF (e.g. wm0): " },
    { "Show Listening Ports","netstat -an | grep LISTEN", 0, NULL },

    /* --- SYSTEM --- */
    { "Shell Escape",        "!sh", 0, NULL },
    { "Exit SYSMGR",         NULL, 0, NULL }
};

#define COUNT (sizeof(db) / sizeof(entry_t))

void log_action(const char *cmd) {
    const char *log_path = "/var/log/sysmgr.log";
    FILE *fp = fopen(log_path, "a");
    if (fp == NULL) return;

    time_t now = time(NULL);
    char *ts = ctime(&now);
    ts[strlen(ts) - 1] = '\0';

    fprintf(fp, "[%s] EXEC: %s\n", ts, cmd);
    fclose(fp);

    /* * Apply the System Append-Only flag.
     * Note: This will only succeed if sysmgr is running as root
     * and the system securelevel allows it.
     */
    chflags(log_path, SF_APPEND);
}

void sys_cooked() {
    endwin();
    reset_shell_mode();
    printf("\r"); 
}

void sys_visual() {
    reset_prog_mode();
    clear();
    refresh();
}

void get_input(const char *prompt, char *buffer, int size) {
    WINDOW *iw = newwin(5, 60, (LINES-5)/2, (COLS-60)/2);
    box(iw, 0, 0);
    mvwprintw(iw, 1, 2, "%s", prompt);
    wrefresh(iw);
    echo();
    curs_set(1);
    mvwgetnstr(iw, 2, 2, buffer, size - 1);
    noecho();
    curs_set(0);
    delwin(iw);
}

void execute(const entry_t *ent, int preview) {
    char final_cmd[1024];
    char input[128] = {0};

    if (!ent->cmd_template) return;

    if (ent->needs_input) {
        get_input(ent->prompt, input, sizeof(input));
        if (strlen(input) == 0) return;
        snprintf(final_cmd, sizeof(final_cmd), ent->cmd_template, input);
    } else {
        strncpy(final_cmd, ent->cmd_template, sizeof(final_cmd));
    }

    if (preview) {
        WINDOW *pw = newwin(7, 70, (LINES-7)/2, (COLS-70)/2);
        box(pw, 0, 0);
        mvwprintw(pw, 1, 2, "Command:");
        wattron(pw, A_BOLD);
        mvwprintw(pw, 3, 4, "%.60s", final_cmd);
        wattroff(pw, A_BOLD);
        mvwprintw(pw, 5, 2, "Press any key to return...");
        wrefresh(pw);
        wgetch(pw);
        delwin(pw);
        return;
    }
    
    log_action(final_cmd);
    sys_cooked();
    printf("\x1b[2J\x1b[H"); 
    printf("%s\n", final_cmd);
    
    system(final_cmd[0] == '!' ? &final_cmd[1] : final_cmd);
    
    printf("\n[Press Enter]");
    fflush(stdout);
    while(getchar() != '\n');
    
    sys_visual();
}

int main() {
    int sel = 0, ch;
    if (initscr() == NULL) return 1;
    cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0); nl();

    while (1) {
        clear();
        mvprintw(0, 0, "System Administrative Manager");
        mvprintw(0, (COLS-14)/2, "Administration");
        
        for (int i = 0; i < (int)COUNT; i++) {
            if (i == sel) attron(A_REVERSE);
            mvprintw(i + 3, 4, "  %-35s", db[i].label ? db[i].label : "");
            attroff(A_REVERSE);
        }

        mvprintw(LINES-2, 2, "F3=Cancel  F6=Command  F9=Shell  F10=Exit  Enter=Do");
        refresh();

        ch = getch();
        switch (ch) {
            case KEY_UP:   sel = (sel > 0) ? sel - 1 : COUNT - 1; break;
            case KEY_DOWN: sel = (sel < (int)COUNT - 1) ? sel + 1 : 0; break;
            case KEY_F(6): execute(&db[sel], 1); break;
            case KEY_F(9): { entry_t sh = {"Shell", "!sh", 0, NULL}; execute(&sh, 0); break; }
            case 10:
            case '\r':
                if (sel == COUNT - 1) goto quit;
                execute(&db[sel], 0);
                break;
            case KEY_F(10): goto quit;
        }
    }
quit:
    sys_cooked();
    return 0;
}
