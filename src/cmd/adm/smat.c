#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

typedef struct entry_t {
    char label[64];
    char cmd_template[256];
    int needs_input;
    char prompt[64];
    struct entry_t *submenu;
    int sub_count;
} entry_t;

entry_t *root_db = NULL;
int root_count = 0;

void log_action(const char *cmd) {
    FILE *fp = fopen("/var/log/smat.log", "a");
    if (!fp) return;
    time_t now = time(NULL);
    char *ts = ctime(&now);
    ts[strlen(ts) - 1] = '\0';
    fprintf(fp, "[%s] EXEC: %s\n", ts, cmd);
    fclose(fp);
}

// Recursive function to parse Lua tables into our C structure
entry_t* parse_lua_table(lua_State *L, int *count) {
    if (!lua_istable(L, -1)) return NULL;

    *count = lua_rawlen(L, -1);
    entry_t *list = malloc(sizeof(entry_t) * (*count));

    for (int i = 1; i <= *count; i++) {
        lua_rawgeti(L, -1, i);
        
        entry_t *e = &list[i-1];
        e->submenu = NULL;
        e->sub_count = 0;

        lua_getfield(L, -1, "label");
        strncpy(e->label, lua_tostring(L, -1) ? lua_tostring(L, -1) : "", 63);
        lua_pop(L, 1);

        lua_getfield(L, -1, "cmd");
        strncpy(e->cmd_template, lua_tostring(L, -1) ? lua_tostring(L, -1) : "", 255);
        lua_pop(L, 1);

        lua_getfield(L, -1, "input");
        e->needs_input = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "prompt");
        strncpy(e->prompt, lua_tostring(L, -1) ? lua_tostring(L, -1) : "Input: ", 63);
        lua_pop(L, 1);

        lua_getfield(L, -1, "submenu");
        if (lua_istable(L, -1)) {
            e->submenu = parse_lua_table(L, &e->sub_count);
        }
        lua_pop(L, 1);

        lua_pop(L, 1); // pop element
    }
    return list;
}

int load_config(const char *filename) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    if (luaL_dofile(L, filename)) {
        lua_close(L);
        return 0;
    }
    lua_getglobal(L, "/etc/smat/tasks");
    root_db = parse_lua_table(L, &root_count);
    lua_close(L);
    return root_db != NULL;
}

void sys_cooked() { endwin(); reset_shell_mode(); printf("\r"); }
void sys_visual() { reset_prog_mode(); clear(); refresh(); }
void draw_custom_box(WINDOW *win) { wborder(win, '|', '|', '-', '-', '+', '+', '+', '+'); }

void get_input(const char *prompt, char *buffer, int size) {
    WINDOW *iw = newwin(5, 60, (LINES-5)/2, (COLS-60)/2);
    draw_custom_box(iw);
    mvwprintw(iw, 1, 2, "%s", prompt);
    wrefresh(iw);
    echo(); curs_set(1);
    mvwgetnstr(iw, 2, 2, buffer, size - 1);
    noecho(); curs_set(0);
    delwin(iw);
}

void execute(entry_t *ent, int preview) {
    if (ent->submenu) return; // Cannot execute a submenu label
    char final_cmd[1024];
    char input[128] = {0};
    if (strlen(ent->cmd_template) == 0) return;

    if (ent->needs_input) {
        get_input(ent->prompt, input, sizeof(input));
        if (strlen(input) == 0) return;
        snprintf(final_cmd, sizeof(final_cmd), ent->cmd_template, input);
    } else {
        strncpy(final_cmd, ent->cmd_template, sizeof(final_cmd));
    }

    if (preview) {
        WINDOW *pw = newwin(7, 70, (LINES-7)/2, (COLS-70)/2);
        draw_custom_box(pw);
        mvwprintw(pw, 1, 2, "Preview: %s", final_cmd);
        mvwprintw(pw, 5, 2, "Any key to return...");
        wrefresh(pw); wgetch(pw); delwin(pw);
        return;
    }

    log_action(final_cmd);
    sys_cooked();
    printf("\x1b[2J\x1b[H-- Executing: %s --\n\n", final_cmd);
    system(final_cmd[0] == '!' ? &final_cmd[1] : final_cmd);
    printf("\n[Press Enter]");
    while(getchar() != '\n');
    sys_visual();
}

// The core menu loop, now capable of recursion
void run_menu(entry_t *menu, int count, const char* title) {
    int sel = 0, ch;
    int height = count + 4;
    int width = 60;

    while (1) {
        clear();
        WINDOW *mw = newwin(height, width, (LINES-height)/2, (COLS-width)/2);
        draw_custom_box(mw);
        mvwprintw(mw, 0, 2, "[ %s ]", title);

        for (int i = 0; i < count; i++) {
            if (i == sel) wattron(mw, A_REVERSE);
            mvwprintw(mw, i + 2, 4, " %-50s ", menu[i].label);
            wattroff(mw, A_REVERSE);
        }
        
        mvprintw(LINES-1, 2, "Arrows: Nav | Enter: Select | F3/ESC: Back | F10: Exit");
        refresh(); wrefresh(mw);

        ch = getch();
        if (ch == KEY_UP) sel = (sel > 0) ? sel - 1 : count - 1;
        else if (ch == KEY_DOWN) sel = (sel < count - 1) ? sel + 1 : 0;
        else if (ch == KEY_F(6)) execute(&menu[sel], 1);
        else if (ch == 10 || ch == '\r') {
            if (menu[sel].submenu) {
                run_menu(menu[sel].submenu, menu[sel].sub_count, menu[sel].label);
            } else {
                if (strlen(menu[sel].cmd_template) == 0) break; // Exit logic
                execute(&menu[sel], 0);
            }
        }
        else if (ch == KEY_F(3) || ch == 27) { delwin(mw); break; }
        else if (ch == KEY_F(10)) { sys_cooked(); exit(0); }

        delwin(mw);
    }
}

int main() {
    // 1. Try the system-wide path first
    // 2. Fall back to the local directory if the system path fails
    if (!load_config("/etc/smat/tasks.lua")) {
        if (!load_config("tasks.lua")) {
            fprintf(stderr, "CRITICAL: Could not find tasks.lua in /etc/smat/ or current directory.\n");
            return 1;
        }
    }

    initscr(); 
    cbreak(); 
    noecho(); 
    keypad(stdscr, TRUE); 
    curs_set(0);
    
    run_menu(root_db, root_count, "SMAT Main Menu");

    sys_cooked();
    return 0;
}
