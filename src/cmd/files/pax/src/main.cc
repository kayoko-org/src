#include <string>
#include <libgen.h>
#include <iostream>

extern "C" {
    #include <kayoko/files/archive/pmain.h>
    /* Define storage for the global variables to fix the linker errors */
    int vflag = 0;
    arch_fmt_t global_fmt = FMT_USTAR;
}

// Forward declarations
int handle_cpio(int argc, char *argv[]);
int handle_tar(int argc, char *argv[]);
int handle_pax(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    std::string prog = basename(argv[0]);

    // Dispatch based on symlink name
    if (prog.find("cpio") != std::string::npos) {
        return handle_cpio(argc, argv);
    } else if (prog.find("tar") != std::string::npos) {
        return handle_tar(argc, argv);
    } else {
        return handle_pax(argc, argv);
    }
}
