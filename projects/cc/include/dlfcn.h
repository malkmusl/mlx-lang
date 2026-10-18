#ifndef _MLXCC_DLFCN_H
#define _MLXCC_DLFCN_H
#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_BINDING_MASK 3
#define RTLD_NOLOAD 4
#define RTLD_DEEPBIND 8
#define RTLD_GLOBAL 0x100
#define RTLD_LOCAL 0
#define RTLD_NODELETE 0x1000
#define RTLD_DEFAULT ((void *)0)
#define RTLD_NEXT ((void *)-1)
typedef struct {
    const char *dli_fname;
    void *dli_fbase;
    const char *dli_sname;
    void *dli_saddr;
} Dl_info;
void *dlopen(const char *, int);
void *dlsym(void *, const char *);
int dlclose(void *);
char *dlerror(void);
int dladdr(const void *, Dl_info *);
#endif
