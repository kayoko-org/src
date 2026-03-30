#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define KAYOKO_TEXTPROC_PAGER_MORE_IMPLEMENTATION
#include <kayoko/textproc/pager/more.h>

/* Global TTY for user input */
static FILE *kyk_tty = NULL;

void do_search(FILE *fp) {
    char pattern[256];
    kyk_clear_footer();
    fprintf(stderr, "/");
    
    /* Must read pattern from TTY */
    if (!kyk_tty || fgets(pattern, sizeof(pattern), kyk_tty) == NULL) return;
    
    pattern[strcspn(pattern, "\n")] = 0;
    if (pattern[0] == '\0') return; 

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, pattern)) {
            fputs(line, stdout);
            return;
        }
    }
    fprintf(stderr, "...Not found...");
    fflush(stderr);
}

void page_stream(FILE *fp, const char *filename) {
    struct stat st;
    long total_size = 0;
    int is_stdin = (filename && strcmp(filename, "-") == 0);

    if (!is_stdin && stat(filename, &st) == 0) total_size = st.st_size;

    char line[1024];
    int lines_printed = 0;
    int height = kyk_get_term_height() - 1;
    int ch;

    while (fgets(line, sizeof(line), fp) != NULL) {
        fputs(line, stdout);
        lines_printed++;

        if (lines_printed >= height) {
            int perc = 0;
            if (total_size > 0) perc = (int)((ftell(fp) * 100) / total_size);
            
            kyk_draw_footer(perc);
            ch = fgetc(kyk_tty);
            kyk_clear_footer();

            if (ch == 'q' || ch == 'Q' || ch == EOF) return;
            if (ch == '/') { do_search(fp); lines_printed = 1; }
            if (ch == 'v' || ch == 'V') {
                if (!is_stdin) {
                    kyk_pager_restore();
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd), "vi %s", filename);
                    system(cmd);
                    kyk_pager_init();
                }
            }
            if (ch == ' ') lines_printed = 0; 
            if (ch == '\n' || ch == '\r') lines_printed = height - 1; 
        }
    }

    /* REACHED END: Handle files smaller than screen or final page */
    fprintf(stderr, "%s-- More -- (END)%s", KYK_ATTR_REVERSE, KYK_ATTR_RESET);
    fflush(stderr);
    
    /* Wait for one last keypress before quitting */
    fgetc(kyk_tty);
    kyk_clear_footer();
}

int main(int argc, char *argv[]) {
    if (argc == 1 && isatty(STDIN_FILENO)) {
        fprintf(stderr, "Missing filename\n");
        return 2;
    }

    kyk_tty = fopen("/dev/tty", "r+");
    if (!kyk_tty) kyk_tty = stdin;

    kyk_pager_init();

    if (argc == 1) {
        page_stream(stdin, "-");
    } else {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-") == 0) {
                page_stream(stdin, "-");
            } else {
                FILE *fp = fopen(argv[i], "r");
                if (!fp) { perror(argv[i]); continue; }
                page_stream(fp, argv[i]);
                fclose(fp);
            }
        }
    }

    kyk_pager_restore();
    if (kyk_tty && kyk_tty != stdin) fclose(kyk_tty);
    return 0;
}
