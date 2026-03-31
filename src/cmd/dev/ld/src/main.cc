#include <iostream>
#include <vector>
#include "ld_internal.hh"

void usage(const char* prog) {
    std::cerr << "usage: " << prog << " [-o output] file.o [file2.o ...]\n";
    exit(1);
}

int main(int argc, char* argv[]) {
    if (argc < 2) usage(argv[0]);

    Linker ld;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h") {
            usage(argv[0]);
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                ld.set_output(argv[++i]);
            } else {
                std::cerr << "ld: -o requires argument\n";
                return 1;
            }
        } else {
            ld.add_object(arg);
        }
    }

    ld.resolve();
    ld.layout();
    ld.write();

    return 0;
}
