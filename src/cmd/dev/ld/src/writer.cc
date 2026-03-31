#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <iomanip>
#include "ld_internal.hh"

/**
 * Target Configuration
 * This makes it "easy to plug in new headers."
 * To support a new OS or Arch, just create a new instance of this struct.
 */
struct TargetConfig {
    unsigned char abi;
    Elf64_Half machine;
    Elf64_Addr entry_point;
    uint64_t page_size;
};

// Default configuration for NetBSD x86_64
static const TargetConfig NetBSD_x64 = {
    .abi = 0,               // System V ABI
    .machine = EM_X86_64,   // AMD64
    .entry_point = 0x400000, // Standard 4MB start
    .page_size = 4096
};

void Linker::layout() {
    std::cout << "ld: calculating virtual memory layout...\n";
    
    Elf64_Addr current_vaddr = NetBSD_x64.entry_point;

    for (auto& obj : objects) {
        for (int i = 0; i < obj->ehdr->e_shnum; ++i) {
            // We only care about sections that need to be in memory (Allocatable)
            // SHF_ALLOC = 0x2
            if (obj->shdr[i].sh_flags & 0x2) {
                obj->shdr[i].sh_addr = current_vaddr;
                current_vaddr += obj->shdr[i].sh_size;
                
                // Align to 16 bytes to keep CPU happy
                current_vaddr = (current_vaddr + 15) & ~15;
            }
        }
    }
}

void Linker::write() {
    std::ofstream out(output_name, std::ios::binary);
    if (!out) {
        std::cerr << "ld: error: could not open " << output_name << " for writing\n";
        return;
    }

    // 1. Prepare the ELF Header using our pluggable TargetConfig
    Elf64_Ehdr out_ehdr;
    std::memset(&out_ehdr, 0, sizeof(Elf64_Ehdr));

    // Identify as ELF64, Little Endian
    std::memcpy(out_ehdr.e_ident, ELFMAG, 4);
    out_ehdr.e_ident[4] = 2; // 64-bit
    out_ehdr.e_ident[5] = 1; // Little Endian
    out_ehdr.e_ident[6] = 1; // Version
    out_ehdr.e_ident[7] = NetBSD_x64.abi;

    out_ehdr.e_type = ET_EXEC;
    out_ehdr.e_machine = NetBSD_x64.machine;
    out_ehdr.e_version = 1;
    out_ehdr.e_entry = NetBSD_x64.entry_point;
    out_ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    out_ehdr.e_shentsize = sizeof(Elf64_Shdr);

    // 2. Write the Header
    out.write(reinterpret_cast<char*>(&out_ehdr), sizeof(Elf64_Ehdr));

    // 3. Merge and Write Sections
    // In a simple linker, we iterate through our objects and 
    // dump the .text (code) and .data sections into the file.
    for (auto& obj : objects) {
        for (int i = 0; i < obj->ehdr->e_shnum; ++i) {
            auto& sh = obj->shdr[i];
            
            // Only write PROGBITS (actual code/data), ignore SYMTAB/STRTAB for the final blob
            if (sh.sh_type == 1 && (sh.sh_flags & 0x2)) {
                // Seek to the section's intended file offset (simplified)
                // In a real linker, you'd calculate file offsets based on VAs
                out.write(reinterpret_cast<char*>(obj->data + sh.sh_offset), sh.sh_size);
            }
        }
    }

    std::cout << "ld: final binary " << output_name << " generated successfully.\n";
    out.close();
}
