/* <linux/memfd.h>: an anonymous shared memory file as WebCore's
   SharedMemoryUnix makes one (syscall(__NR_memfd_create, name,
   MFD_CLOEXEC)), sized, written, mapped and read back. */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/memfd.h>

int main(void) {
    int fd = (int)syscall(__NR_memfd_create, "MlxSharedMemory", MFD_CLOEXEC);
    if (fd < 0) { printf("memfd_create failed\n"); return 1; }
    printf("cloexec %d\n", (fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0);
    if (ftruncate(fd, 4096) != 0) { printf("ftruncate failed\n"); return 1; }
    char *map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { printf("mmap failed\n"); return 1; }
    strcpy(map, "shared");
    char back[8] = { 0 };
    printf("pread %d %s\n", (int)pread(fd, back, 7, 0), back);
    munmap(map, 4096);
    close(fd);
    printf("flags %u %u %u\n", MFD_CLOEXEC, MFD_ALLOW_SEALING, MFD_HUGE_2MB >> MFD_HUGE_SHIFT);
    return 0;
}
