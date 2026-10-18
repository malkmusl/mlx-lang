#ifndef _MLXCC_LINK_H
#define _MLXCC_LINK_H
#include <elf.h>
#include <stddef.h>
#define ElfW(type) Elf64_##type
struct link_map {
    ElfW(Addr) l_addr;
    char *l_name;
    ElfW(Dyn) *l_ld;
    struct link_map *l_next, *l_prev;
};
struct dl_phdr_info {
    ElfW(Addr) dlpi_addr;
    const char *dlpi_name;
    const ElfW(Phdr) *dlpi_phdr;
    ElfW(Half) dlpi_phnum;
    unsigned long long dlpi_adds;
    unsigned long long dlpi_subs;
    size_t dlpi_tls_modid;
    void *dlpi_tls_data;
};
#ifdef __cplusplus
extern "C" {
#endif
int dl_iterate_phdr(int (*)(struct dl_phdr_info *, size_t, void *), void *);
#ifdef __cplusplus
}
#endif
#endif
