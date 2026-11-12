/*
 * What Linux's atom.c asks of the kernel around it, for building that one
 * file as an ordinary program (tools/check_atom_reference.sh; a
 * development comparison, nothing of brixOS is built from this): the
 * integer types, memory, the log, waits, the clock, the lock. None of it
 * is Linux's code. The waits and the clock are the comparison's own: a
 * wait is written down (reference.c) instead of waited, and the clock
 * goes on by one tick each time it is looked at, so that a table that
 * loops for ever meets Linux's 20 seconds after 20000 looks.
 */
#ifndef ATOM_REFERENCE_KERNEL_H
#define ATOM_REFERENCE_KERNEL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/* Memory: zeroed, and never less than 4 KiB, so that what Linux reads
 * past a work space's end (it does not look) is the program's own. */
#define GFP_KERNEL 0
static inline void *reference_alloc(size_t bytes) { return calloc(1, bytes < 4096 ? 4096 : bytes); }
#define kzalloc(size, flags) reference_alloc(size)
#define kmalloc(size, flags) reference_alloc(size)
#define kcalloc(count, size, flags) reference_alloc((size_t)(count) * (size_t)(size))
#define kfree(pointer) free(pointer)

/* The log: Linux's own trace of what it runs, when asked for. */
extern int reference_verbose;
#define KERN_DEBUG ""
#define printk(...) do { if (reference_verbose) fprintf(stderr, __VA_ARGS__); } while (0)
#define pr_info(...) printk(__VA_ARGS__)
#define DRM_ERROR(...) printk(__VA_ARGS__)

/* Waits (written down) and the clock (a tick a look, a tick a
 * millisecond). */
void reference_wait(unsigned long microseconds);
unsigned long reference_clock(void);
#define udelay(count) reference_wait(count)
#define mdelay(count) reference_wait((unsigned long)(count) * 1000)
#define msleep(count) reference_wait((unsigned long)(count) * 1000)
#define drm_can_sleep() 1
#define jiffies reference_clock()
#define time_after(a, b) ((long)((b) - (a)) < 0)
#define jiffies_to_msecs(ticks) (ticks)

struct mutex { int held; };
#define mutex_lock(lock) ((void)(lock))
#define mutex_unlock(lock) ((void)(lock))

#define cpu_to_le32(value) (value)
#define le32_to_cpu(value) (value)
#define cpu_to_le16(value) (value)
#define le16_to_cpu(value) (value)
static inline u32 get_unaligned_le32(const void *pointer)
{
	u32 value;
	memcpy(&value, pointer, 4);
	return value;
}
#define lower_32_bits(value) ((u32)(value))
#define upper_32_bits(value) ((u32)((value) >> 32))
/* A 64-bit number divided in place by a 32-bit one; the remainder. */
#define do_div(number, base) ({ u32 reference_rest = (number) % (base); (number) /= (base); reference_rest; })
static inline const char *str_yes_no(bool value) { return value ? "yes" : "no"; }

#endif
