#define KAYOKO_DEV_AR_IMPLEMENTATION
#include <kayoko/dev/ar.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-trx] archive [file...]\n", prog);
    exit(1);
}

/* Helper to copy data between streams */
void copy_data(FILE *src, FILE *dst, size_t n) {
    char buf[8192];
    while (n > 0) {
        size_t chunk = (n > sizeof(buf)) ? sizeof(buf) : n;
        fread(buf, 1, chunk, src);
        fwrite(buf, 1, chunk, dst);
        n -= chunk;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) usage(argv[0]);

    char *mode = argv[1];
    char *archive_name = argv[2];

    if (strcmp(mode, "-t") == 0) {
        /* LIST CONTENTS */
        FILE *ar = fopen(archive_name, "rb");
        if (!ar || !kayoko_ar_is_valid(ar)) {
            fprintf(stderr, "Invalid archive\n");
            return 1;
        }

        struct kayoko_ar_hdr hdr;
        while (fread(&hdr, sizeof(hdr), 1, ar) == 1) {
            printf("%.16s\n", hdr.ar_name);
            long size = atol(hdr.ar_size);
            fseek(ar, size + (size % 2), SEEK_CUR); // Skip data + padding
        }
        fclose(ar);

    } else if (strcmp(mode, "-r") == 0) {
        /* APPEND/REPLACE (Simple Append for now) */
        FILE *ar = fopen(archive_name, "ab+");
        if (!ar) return 1;

        // If file is empty, write global header
        fseek(ar, 0, SEEK_END);
        if (ftell(ar) == 0) {
            fwrite(ARMAG, 1, SARMAG, ar);
        }

        for (int i = 3; i < argc; i++) {
            struct stat st;
            if (stat(argv[i], &st) != 0) continue;

            FILE *in = fopen(argv[i], "rb");
            if (!in) continue;

            kayoko_ar_write_hdr(ar, argv[i], &st);
            copy_data(in, ar, st.st_size);
            
            if (st.st_size % 2 != 0) fputc('\n', ar); // POSIX Alignment
            fclose(in);
        }
        fclose(ar);

    } else if (strcmp(mode, "-x") == 0) {
        /* EXTRACT */
        FILE *ar = fopen(archive_name, "rb");
        if (!ar || !kayoko_ar_is_valid(ar)) return 1;

        struct kayoko_ar_hdr hdr;
        while (fread(&hdr, sizeof(hdr), 1, ar) == 1) {
            char name[17] = {0};
            strncpy(name, hdr.ar_name, 16);
            // Basic space-trimming for the filename
            for(int i=15; i>=0 && name[i]==' '; i--) name[i]='\0';

            long size = atol(hdr.ar_size);
            FILE *out = fopen(name, "wb");
            if (out) {
                copy_data(ar, out, size);
                fclose(out);
            } else {
                fseek(ar, size, SEEK_CUR);
            }
            if (size % 2 != 0) fseek(ar, 1, SEEK_CUR); // Skip padding
        }
        fclose(ar);
    }

    return 0;
}
