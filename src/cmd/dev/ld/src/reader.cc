#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "ld_internal.hh"

ObjectFile::ObjectFile(const std::string& filename) : path(filename) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("could not open " + filename);

    struct stat st;
    fstat(fd, &st);
    size = st.st_size;

    data = (uint8_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);

    if (data == MAP_FAILED) throw std::runtime_error("mmap failed");

    ehdr = (Elf64_Ehdr*)data;
    if (std::memcmp(ehdr->e_ident, ELFMAG, 4) != 0) {
        throw std::runtime_error("not an ELF file: " + filename);
    }
    shdr = (Elf64_Shdr*)(data + ehdr->e_shoff);
}

ObjectFile::~ObjectFile() {
    if (data) munmap(data, size);
}

void Linker::add_object(const std::string& filename) {
    try {
        objects.push_back(std::make_unique<ObjectFile>(filename));
        std::cout << "ld: loaded " << filename << "\n";
    } catch (const std::exception& e) {
        std::cerr << "ld: error: " << e.what() << "\n";
        exit(1);
    }
}
