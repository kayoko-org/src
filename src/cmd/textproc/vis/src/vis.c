#include <stdio.h>
#include <stdlib.h>
#define KAYOKO_TEXTPROC_ANSI_VIS_IMPLEMENTATION
#include <kayoko/textproc/ansi/vis/vis.h>

int main(int argc, char *argv[]) {
    if (argc == 1) {
        kayoko_vis(stdin);
    } else {
        for (int i = 1; i < argc; i++) {
            FILE *fp = fopen(argv[i], "r");
            if (!fp) {
                perror(argv[i]);
                continue;
            }
            kayoko_vis(fp);
            fclose(fp);
        }
    }
    return 0;
}
