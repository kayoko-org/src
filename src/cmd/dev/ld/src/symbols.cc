#include <iostream>
#include "ld_internal.hh"

void Linker::resolve() {
    for (auto& obj : objects) {
        for (int i = 0; i < obj->ehdr->e_shnum; ++i) {
            if (obj->shdr[i].sh_type == SHT_SYMTAB) {
                auto* sh = &obj->shdr[i];
                auto* syms = (Elf64_Sym*)(obj->data + sh->sh_offset);
                char* strs = (char*)(obj->data + obj->shdr[sh->sh_link].sh_offset);
                int count = sh->sh_size / sizeof(Elf64_Sym);

                for (int j = 0; j < count; ++j) {
                    if ((syms[j].st_info >> 4) == 1 && syms[j].st_shndx != SHN_UNDEF) {
                        global_symbols.push_back({
                            .name = std::string(strs + syms[j].st_name),
                            .value = syms[j].st_value,
                            .source = obj.get()
                        });
                        std::cout << "ld: symbol [" << global_symbols.back().name << "]\n";
                    }
                }
            }
        }
    }
}
