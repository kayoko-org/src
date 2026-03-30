#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Implementation guards for our header-only logic */
#define KAYOKO_TEXTPROC_ANSI_VIS_IMPLEMENTATION
#include <kayoko/textproc/ansi/vis/vis.h>

#define KAYOKO_TEXTPROC_ANSI_UNVIS_IMPLEMENTATION
#include <kayoko/textproc/ansi/vis/unvis.h>

/* * Helper to apply the chosen function to either stdin 
 * or a list of files provided in argv.
 */
void process_input(int argc, char *argv[], void (*func)(FILE *)) {
    if (argc == 1) {
        func(stdin);
    } else {
        for (int i = 1; i < argc; i++) {
            FILE *fp = fopen(argv[i], "r");
            if (!fp) {
                perror(argv[i]);
                continue;
            }
            func(fp);
            fclose(fp);
        }
    }
}

int main(int argc, char *argv[]) {
    /* Determine mode based on the program name */
    char *progname = argv[0];
    
    /* Strip path if present (e.g., /usr/bin/unvis -> unvis) */
    char *base = strrchr(progname, '/');
    if (base) progname = base + 1;

    if (strcmp(progname, "unvis") == 0) {
        process_input(argc, argv, kayoko_unvis);
    } else {
        /* Default to vis mode */
        process_input(argc, argv, kayoko_vis);
    }

    return 0;
}
