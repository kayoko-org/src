#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <cstring>

extern "C" {
    #include <kayoko/files/archive/pmain.h>
}

static void usage_pax() {
    std::cerr << "usage: pax [-rwv] [-f archive] [-x format] [pattern ...]" << std::endl;
}

int handle_pax(int argc, char *argv[]) {
    if (argc < 2) {
        usage_pax();
        return 1;
    }

    int c;
    int rflag = 0, wflag = 0;
    std::string arch_file;

    optind = 1;
    while ((c = getopt(argc, argv, "rwvf:x:")) != -1) {
        switch (c) {
            case 'r': rflag = 1; break;
            case 'w': wflag = 1; break;
            case 'v': vflag = 1; break;
            case 'f': arch_file = optarg; break;
            case 'x':
                if (std::string(optarg) == "cpio") global_fmt = FMT_CPIO;
                else global_fmt = FMT_USTAR;
                break;
            default:
                usage_pax();
                return 1;
        }
    }

    // Read and Write mode (Copy)
    if (rflag && wflag) {
        if (optind >= argc - 1) {
            usage_pax();
            return 1;
        }
        const char *dest = argv[argc - 1];
        for (int i = optind; i < argc - 1; i++) {
            pax_copy_node(argv[i], const_cast<char*>(dest));
        }
    }
    // Write mode (Archive creation)
    else if (wflag) {
        int fd = arch_file.empty() ? STDOUT_FILENO : open(arch_file.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if (fd < 0) {
            perror("pax: open");
            return 1;
        }
        for (int i = optind; i < argc; i++) {
            pax_archive_file(fd, argv[i]);
        }

        if (global_fmt == FMT_CPIO) {
            pax_archive_file(fd, const_cast<char*>(CPIO_TRAILER));
        } else {
            std::vector<char> trailer(1024, 0);
            write(fd, trailer.data(), trailer.size());
        }
        if (!arch_file.empty()) close(fd);
    }
    // Read mode (List or Extract)
    else {
        int fd = arch_file.empty() ? STDIN_FILENO : open(arch_file.c_str(), O_RDONLY);
        if (fd < 0) {
            perror("pax: open");
            return 1;
        }
        pax_do_read_list(fd, rflag);
        if (!arch_file.empty()) close(fd);
    }
    return 0;
}
