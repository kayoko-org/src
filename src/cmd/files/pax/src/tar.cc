#include <iostream>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <cstring>

extern "C" {
    #include <kayoko/files/archive/pmain.h>
}

static void usage_tar() {
    std::cerr << "usage: tar [[-]cxvtf archive] [-C directory] [file ...]" << std::endl;
}

int handle_tar(int argc, char *argv[]) {
    if (argc < 2) {
        usage_tar();
        return 1;
    }

    int create = 0, list = 0, extract = 0;
    std::string arch_file;
    std::string dashed_arg;
    
    // Create a mutable copy of argv for getopt
    std::vector<char*> args(argv, argv + argc);

    // Handle "Old Style" tar commands (e.g., 'tar cvf' -> 'tar -cvf')
    if (args[1][0] != '-') {
        dashed_arg = "-" + std::string(args[1]);
        args[1] = const_cast<char*>(dashed_arg.c_str());
    }

    optind = 1;
    global_fmt = FMT_USTAR;

    int c;
    while ((c = getopt(argc, args.data(), "cvtxf:C:")) != -1) {
        switch (c) {
            case 'c': create = 1; break;
            case 'v': vflag = 1; break;
            case 't': list = 1; break;
            case 'x': extract = 1; break;
            case 'f': arch_file = optarg; break;
            case 'C': 
                if (chdir(optarg) < 0) {
                    perror("tar: chdir");
                    return 1;
                }
                break;
            default:
                usage_tar();
                return 1;
        }
    }

    if (!create && !list && !extract) {
        std::cerr << "tar: must specify one of -c, -t, or -x" << std::endl;
        usage_tar();
        return 1;
    }

    int fd;
    if (!arch_file.empty()) {
        fd = open(arch_file.c_str(), create ? O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY, 0644);
    } else {
        fd = create ? STDOUT_FILENO : STDIN_FILENO;
    }

    if (fd < 0) {
        perror("tar: open");
        return 1;
    }

    if (create) {
        for (int i = optind; i < argc; i++) {
            pax_archive_file(fd, argv[i]);
        }
        // Write two 512-byte zero blocks as trailer
        std::vector<char> trailer(1024, 0);
        write(fd, trailer.data(), trailer.size());
    } else {
        // pax_do_read_list(fd, 1) extracts, (fd, 0) lists
        pax_do_read_list(fd, extract);
    }

    if (!arch_file.empty() && fd > 2) close(fd);
    return 0;
}
