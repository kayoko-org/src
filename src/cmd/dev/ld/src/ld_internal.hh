#ifndef LD_INTERNAL_HH
#define LD_INTERNAL_HH

#include <vector>
#include <string>
#include <memory>
#include <kayoko/dev/ld_elf64.h>

class ObjectFile {
public:
    std::string path;
    uint8_t* data;
    size_t size;
    Elf64_Ehdr* ehdr;
    Elf64_Shdr* shdr;

    ObjectFile(const std::string& filename);
    ~ObjectFile();
};

struct Symbol {
    std::string name;
    Elf64_Addr value;
    ObjectFile* source;
};

class Linker {
private:
    std::vector<std::unique_ptr<ObjectFile>> objects;
    std::vector<Symbol> global_symbols;
    std::string output_name = "a.out";

public:
    void add_object(const std::string& filename);
    void set_output(const std::string& name) { output_name = name; }
    void resolve();
    void layout();
    void write();
};

#endif
