#include <iostream>
#include <string>
#include <unistd.h>
#include <getopt.h>

extern "C" {
    #include <kayoko/files/archive/pmain.h>
}
#include <iostream>
#include <string>
#include <unistd.h>
#include <getopt.h>

extern "C" {
    #include <kayoko/files/archive/pmain.h>
}

static void usage_cpio() {
    std::cerr << "usage: cpio -o [-v] > archive" << std::endl;
    std::cerr << "       cpio -i [-v] < archive" << std::endl;
}

int handle_cpio(int argc, char *argv[]) {
    if (argc < 2) {
        usage_cpio();
        return 1;
    }

    int c;
    char mode = 0;

    optind = 1; 
    while ((c = getopt(argc, argv, "oiv")) != -1) {
        switch (c) {
            case 'o': mode = 'o'; break;
            case 'i': mode = 'i'; break;
            case 'v': vflag = 1; break;
            default:
                usage_cpio();
                return 1;
        }
    }

    global_fmt = FMT_CPIO;

    if (mode == 'o') {
        std::string line;
        // cpio -o expects a list of files from stdin
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            pax_archive_file(STDOUT_FILENO, const_cast<char*>(line.c_str()));
        }
        // Write the special CPIO trailer string
        pax_archive_file(STDOUT_FILENO, const_cast<char*>(CPIO_TRAILER));
    } else if (mode == 'i') {
        // cpio -i extracts from stdin
        pax_do_read_list(STDIN_FILENO, 1);
    } else {
        usage_cpio();
        return 1;
    }

    return 0;
}
