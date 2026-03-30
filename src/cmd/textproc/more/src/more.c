#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

#define KAYOKO_TEXTPROC_PAGER_MORE_IMPLEMENTATION
#include <kayoko/textproc/pager/more.h>

static char **line_buffer = NULL;
static int lines_count = 0;
static int buffer_cap = 0;
static FILE *kyk_tty = NULL;
static int is_moar_mode = 0;

/* --- Buffer Management --- */

void add_to_buffer(const char *line) {
    if (lines_count >= buffer_cap) {
        buffer_cap = (buffer_cap == 0) ? 1024 : buffer_cap * 2;
        line_buffer = realloc(line_buffer, buffer_cap * sizeof(char *));
    }
    line_buffer[lines_count++] = strdup(line);
}

/* --- Terminal UI Helpers --- */

void enter_moar_screen() {
    if (is_moar_mode) {
        /* Switch to Alt Buffer and Home cursor */
        fprintf(stdout, "\033[?1049h\033[H");
    }
    fflush(stdout);
}

void leave_moar_screen() {
    if (is_moar_mode) {
        /* Switch back to Main Buffer */
        fprintf(stdout, "\033[?1049l");
    }
    fflush(stdout);
}

void redraw_screen(int current_top, int term_height) {
    if (is_moar_mode) {
        /* Move cursor to top-left to overwrite current view */
        fprintf(stdout, "\033[H");
    }

    int end_line = current_top + term_height;
    if (end_line > lines_count) end_line = lines_count;

    for (int i = current_top; i < end_line; i++) {
        /* \033[K clears the line to the right to prevent "ghost" characters 
           from longer lines previously on screen */
        if (is_moar_mode) fprintf(stdout, "\033[K");
        fputs(line_buffer[i], stdout);
    }
    fflush(stdout);
}

/* --- Main Pager Logic --- */

void page_stream(FILE *fp, const char *filename) {
    char raw_line[1024];
    int current_top = 0;
    int term_height = kyk_get_term_height() - 1;
    int quit = 0;
    int file_eof = 0;

    enter_moar_screen();

    while (!quit) {
        /* 1. Load enough lines to fill the viewport */
        while (!file_eof && (lines_count < current_top + term_height)) {
            if (fgets(raw_line, sizeof(raw_line), fp) != NULL) {
                add_to_buffer(raw_line);
            } else {
                file_eof = 1;
            }
        }

        /* 2. Render the viewport */
        redraw_screen(current_top, term_height);

        /* 3. Draw Status / Footer */
        int perc = (file_eof && lines_count > 0) ? 100 : (int)((current_top * 100) / (lines_count + 1));
        if (is_moar_mode) {
            fprintf(stderr, "%s(moar) %s %d%%%s", KYK_ATTR_REVERSE, filename, perc, KYK_ATTR_RESET);
        } else {
            kyk_draw_footer(perc);
        }

        /* 4. Handle Input */
        int ch = fgetc(kyk_tty);
        kyk_clear_footer();

        switch (ch) {
            case 'q': case 'Q': case EOF:
                quit = 1;
                break;
            case ' ': /* Page Down */
                if (!file_eof || current_top + term_height < lines_count) {
                    current_top += term_height;
                } else if (file_eof && current_top + term_height >= lines_count) {
                    if (!is_moar_mode) quit = 1; /* Classic 'more' exits at end */
                }
                break;
            case '\n': case '\r': /* Line Down */
                if (!file_eof || current_top < lines_count - 1) {
                    current_top++;
                }
                break;
            case 'b': case 'B': /* Page Up */
                if (is_moar_mode) {
                    current_top -= term_height;
                    if (current_top < 0) current_top = 0;
                    /* In moar mode, we must clear the screen for a full backward jump */
                    fprintf(stdout, "\033[2J");
                }
                break;
        }

        /* If we hit EOF and we're at the bottom in 'more' mode, exit naturally */
        if (!is_moar_mode && file_eof && (current_top + term_height >= lines_count)) {
            quit = 1;
        }
    }

    leave_moar_screen();

    /* Memory Cleanup */
    for (int i = 0; i < lines_count; i++) free(line_buffer[i]);
    free(line_buffer);
    line_buffer = NULL; lines_count = 0; buffer_cap = 0;
}

int main(int argc, char *argv[]) {
    if (argc < 1) return 1;
    char *argv0_copy = strdup(argv[0]);
    char *progname = basename(argv0_copy);
    if (progname && strcmp(progname, "moar") == 0) is_moar_mode = 1;

    kyk_tty = fopen("/dev/tty", "r+");
    if (!kyk_tty) kyk_tty = stdin;

    if (argc == 1 && isatty(STDIN_FILENO)) {
        fprintf(stderr, "Usage: %s [file...]\n", progname);
        free(argv0_copy);
        return 2;
    }

    kyk_pager_init();

    if (argc == 1) {
        page_stream(stdin, "stdin");
    } else {
        for (int i = 1; i < argc; i++) {
            FILE *fp = (strcmp(argv[i], "-") == 0) ? stdin : fopen(argv[i], "r");
            if (!fp) { perror(argv[i]); continue; }
            page_stream(fp, argv[i]);
            if (fp != stdin) fclose(fp);
        }
    }

    kyk_pager_restore();
    if (kyk_tty && kyk_tty != stdin) fclose(kyk_tty);
    free(argv0_copy);
    return 0;
}
