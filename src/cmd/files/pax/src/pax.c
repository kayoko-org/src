#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <kayoko/files/archive/pmain.h>

/* Global state from pmain.h */
int vflag = 0;
arch_fmt_t global_fmt = FMT_USTAR;

static void usage_pax(void) {
    fprintf(stderr, "usage: pax [-rwv] [-f archive] [-x format] [pattern ...]\n");
    exit(1);
}

static void usage_cpio(void) {
    fprintf(stderr, "usage: cpio -o [-v] > archive\n");
    fprintf(stderr, "       cpio -i [-v] < archive\n");
    exit(1);
}

static void usage_tar(void) {
    fprintf(stderr, "usage: tar [[-]cxvtf archive] [-C directory] [file ...]\n");
    exit(1);
}

/**
 * handle_cpio: Implementation of cpio-style interface
 * cpio typically reads a list of filenames from stdin.
 */
int handle_cpio(int argc, char *argv[]) {
    int c;
    int mode = 0; /* 'o' for output, 'i' for input */

    global_fmt = FMT_CPIO;

    while ((c = getopt(argc, argv, "oiv")) != -1) {
        switch (c) {
            case 'o': mode = 'o'; break;
            case 'i': mode = 'i'; break;
            case 'v': vflag = 1; break;
            default: usage_cpio();
        }
    }

    if (mode == 'o') {
        char line[4096];
        while (fgets(line, sizeof(line), stdin)) {
            line[strcspn(line, "\n")] = 0;
            pax_archive_file(STDOUT_FILENO, line);
        }
        pax_archive_file(STDOUT_FILENO, CPIO_TRAILER);
    } else if (mode == 'i') {
        pax_do_read_list(STDIN_FILENO, 1);
    } else {
        usage_cpio();
    }
    return 0;
}

/**
 * handle_tar: Implementation of tar-style interface
 */

int handle_tar(int argc, char *argv[]) {
    int c;
    int create = 0;
    int list = 0;
    int extract = 0;
    char *arch_file = NULL;
    char *dashed_arg = NULL;

    if (argc < 2) {
        usage_tar();
        return 1;
    }

    /* * Handle "Old Style" tar commands (e.g., 'tar cvf' instead of 'tar -cvf').
     * We check if the first argument doesn't start with a dash.
     */
if (argv[1][0] != '-') {
        size_t len = strlen(argv[1]);
        dashed_arg = malloc(len + 2);
        if (dashed_arg) {
            dashed_arg[0] = '-';
            memcpy(dashed_arg + 1, argv[1], len + 1);
            argv[1] = dashed_arg;
        }
    }

    /* Reset getopt state for multi-call binary consistency */
    optind = 1;
    global_fmt = FMT_USTAR;

    while ((c = getopt(argc, argv, "cvtxf:C:")) != -1) {
        switch (c) {
            case 'c': create = 1;  break;
            case 'v': vflag = 1;   break;
            case 't': list = 1;    break;
            case 'x': extract = 1; break;
            case 'f': arch_file = optarg; break;
            case 'C':
                if (chdir(optarg) < 0) {
                    perror("tar: chdir");
                    free(dashed_arg);
                    return 1;
                }
                break;
            default:
                usage_tar();
                free(dashed_arg);
                return 1;
        }
    }

    /* Safety check: ensure one action was requested */
    if (!create && !list && !extract) {
        fprintf(stderr, "tar: must specify one of -c, -t, or -x\n");
        usage_tar();
        free(dashed_arg);
        return 1;
    }

    /* Open the archive file or use stdio */
    int fd;
    if (arch_file) {
        if (create) {
            fd = open(arch_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        } else {
            fd = open(arch_file, O_RDONLY);
        }
    } else {
        fd = create ? STDOUT_FILENO : STDIN_FILENO;
    }

    if (fd < 0) {
        perror("tar: open");
        free(dashed_arg);
        return 1;
    }

    if (create) {
        /* Process all remaining arguments as files to archive */
        for (int i = optind; i < argc; i++) {
            pax_archive_file(fd, argv[i]);
        }
        
        /* Write the POSIX tar end-of-archive marker (two 512-byte blocks of zeros) */
        char trailer[1024];
        memset(trailer, 0, sizeof(trailer));
        if (write(fd, trailer, sizeof(trailer)) != sizeof(trailer)) {
            perror("tar: write trailer");
        }
    } else {
        /* * pax_do_read_list(fd, 0) -> List (-t)
         * pax_do_read_list(fd, 1) -> Extract (-x)
         */
        pax_do_read_list(fd, extract);
    }

    /* Cleanup */
    if (arch_file && fd > 2) {
        close(fd);
    }
    free(dashed_arg);

    return 0;
}


/**
 * handle_pax: The standard POSIX pax interface
 */
int handle_pax(int argc, char *argv[]) {
    int c;
    int rflag = 0, wflag = 0;
    char *arch_file = NULL;

    while ((c = getopt(argc, argv, "rwvf:x:")) != -1) {
        switch (c) {
            case 'r': rflag = 1; break;
            case 'w': wflag = 1; break;
            case 'v': vflag = 1; break;
            case 'f': arch_file = optarg; break;
            case 'x':
                if (!strcmp(optarg, "ustar")) global_fmt = FMT_USTAR;
                else if (!strcmp(optarg, "cpio")) global_fmt = FMT_CPIO;
                else usage_pax();
                break;
            default: usage_pax();
        }
    }

    if (rflag && wflag) {
        if (optind >= argc - 1) usage_pax();
        const char *dest = argv[argc - 1];
        for (int i = optind; i < argc - 1; i++) pax_copy_node(argv[i], dest);
    } else if (wflag) {
        int fd = arch_file ? open(arch_file, O_WRONLY|O_CREAT|O_TRUNC, 0644) : STDOUT_FILENO;
        if (fd < 0) return 1;
        for (int i = optind; i < argc; i++) pax_archive_file(fd, argv[i]);
        if (global_fmt == FMT_CPIO) pax_archive_file(fd, CPIO_TRAILER);
        else {
            char trailer[BLKSIZE * 2];
            memset(trailer, 0, sizeof(trailer));
            write(fd, trailer, sizeof(trailer));
        }
        if (arch_file) close(fd);
    } else {
        int fd = arch_file ? open(arch_file, O_RDONLY) : STDIN_FILENO;
        if (fd < 0) return 1;
        pax_do_read_list(fd, rflag);
        if (arch_file) close(fd);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    char *prog = basename(argv[0]);

    if (strstr(prog, "cpio")) {
        return handle_cpio(argc, argv);
    } else if (strstr(prog, "tar")) {
        return handle_tar(argc, argv);
    } else {
        return handle_pax(argc, argv);
    }
}
