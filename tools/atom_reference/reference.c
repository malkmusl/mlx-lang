/*
 * Linux's own interpreter for a Radeon's firmware tables (atom.c of Linux
 * 6.6, which is not in this repository: tools/check_atom_reference.sh is
 * told where it lies) run as an ordinary program on a card that is a
 * table, writing down every read, write and wait: the reference the
 * kernel's interpreter (projects/brix/kernel/atom.mlx) is held against.
 * A development comparison; nothing of brixOS is built from this.
 *
 *   reference ROM RUNS [-v]
 *
 * ROM: a video BIOS image. RUNS: a text file, a run a line, numbers in
 * decimal:
 *   TABLE SEED FRESH STEPS COUNT WORD...
 * the command table's place in the ROM's list, the seed of what the
 * card's unwritten registers read as, whether the interpreter starts
 * anew (1) or keeps what the run before left (0), a limit on the
 * instructions that only the kernel's interpreter has (not used here),
 * and the parameter space's first COUNT words. -v: Linux's own account
 * of every instruction, on the error output.
 *
 * The card is tests/support/atom_card.mlx's, number for number: a
 * register reads what was written to it last, and never written
 * seeded(space, number). What is written out is what tools/
 * atom_reference/ours.mlx writes for the same run:
 *   run N table T seed S
 *   R/W number value        a register read, written
 *   PR/PW, MR/MW            the PLL's, the memory controller's
 *   D microseconds          a wait
 *   end done|absent|stuck   how amdgpu_atom_execute_table answered
 *   ps ...                  the parameter space's first 16 words and all
 *                           1024 folded into one number
 *   state ...               the quotient, the remainder, the shift, the IO
 *                           attributes, the two flags, the data block,
 *                           the register block, the scratch's window, the
 *                           IO mode
 *   fb ...                  the scratch folded into one number
 */
#include "kernel.h"
#include "atom.h"
#include "atom-bits.h"

#define REGISTERS 0x20000
#define ROM_ROOM 0x100000
#define PARAMETERS 1024
#define SCRATCH_WORDS 5120

int reference_verbose;

static uint32_t words[REGISTERS], stamps[REGISTERS];
static uint32_t pll_words[256], pll_stamps[256];
static uint32_t mc_words[256], mc_stamps[256];
static uint32_t round_now;
static uint32_t seed;
static unsigned long ticks;

static uint32_t seeded(uint32_t space, uint32_t number)
{
	uint32_t x = number * 2654435761u + seed + space * 0x9E3779B1u;

	x ^= x >> 15;
	x *= 2246822519u;
	x ^= x >> 13;
	return x;
}

static uint32_t reg_read(struct card_info *info, uint32_t reg)
{
	uint32_t value = reg < REGISTERS && stamps[reg] == round_now ? words[reg] : (reg < REGISTERS ? seeded(0, reg) : 0);

	printf("R %05x %08x\n", reg, value);
	return value;
}

static void reg_write(struct card_info *info, u32 reg, uint32_t value)
{
	printf("W %05x %08x\n", reg, value);
	if (reg < REGISTERS) {
		words[reg] = value;
		stamps[reg] = round_now;
	}
}

static uint32_t pll_read(struct card_info *info, uint32_t reg)
{
	uint32_t at = reg & 0xFF;
	uint32_t value = pll_stamps[at] == round_now ? pll_words[at] : seeded(1, at);

	printf("PR %02x %08x\n", at, value);
	return value;
}

static void pll_write(struct card_info *info, u32 reg, uint32_t value)
{
	printf("PW %02x %08x\n", reg & 0xFF, value);
	pll_words[reg & 0xFF] = value;
	pll_stamps[reg & 0xFF] = round_now;
}

static uint32_t mc_read(struct card_info *info, uint32_t reg)
{
	uint32_t at = reg & 0xFF;
	uint32_t value = mc_stamps[at] == round_now ? mc_words[at] : seeded(2, at);

	printf("MR %02x %08x\n", at, value);
	return value;
}

static void mc_write(struct card_info *info, u32 reg, uint32_t value)
{
	printf("MW %02x %08x\n", reg & 0xFF, value);
	mc_words[reg & 0xFF] = value;
	mc_stamps[reg & 0xFF] = round_now;
}

void reference_wait(unsigned long microseconds)
{
	printf("D %lu\n", microseconds);
}

unsigned long reference_clock(void)
{
	return ++ticks;
}

static uint64_t fold(const uint32_t *from, size_t count)
{
	uint64_t digest = 0xCBF29CE484222325ull;
	size_t k;

	for (k = 0; k < count; k++)
		digest = (digest ^ from[k]) * 1099511628211ull;
	return digest;
}

static uint8_t rom[ROM_ROOM];
static uint32_t parameters[PARAMETERS + 1024];
static struct card_info card = {
	.reg_read = reg_read, .reg_write = reg_write,
	.pll_read = pll_read, .pll_write = pll_write,
	.mc_read = mc_read, .mc_write = mc_write,
};

int main(int argc, char **argv)
{
	struct atom_context *ctx = NULL;
	uint32_t *scratch = calloc(SCRATCH_WORDS + 4096, 4);
	FILE *file;
	char line[8192];
	size_t size;
	int run = 0;

	if (argc < 3) {
		fprintf(stderr, "usage: reference ROM RUNS [-v]\n");
		return 2;
	}
	reference_verbose = argc > 3;
	amdgpu_atom_debug = reference_verbose;
	file = fopen(argv[1], "rb");
	if (!file) {
		perror(argv[1]);
		return 2;
	}
	size = fread(rom, 1, ROM_ROOM - 0x40000, file);
	fclose(file);
	(void)size;
	file = fopen(argv[2], "r");
	if (!file) {
		perror(argv[2]);
		return 2;
	}
	while (fgets(line, sizeof(line), file)) {
		unsigned long table, with_seed, fresh, steps, count, k;
		char *at = line;
		int answer;

		if (line[0] < '0' || line[0] > '9')
			continue;
		table = strtoul(at, &at, 10);
		with_seed = strtoul(at, &at, 10);
		fresh = strtoul(at, &at, 10);
		steps = strtoul(at, &at, 10);
		count = strtoul(at, &at, 10);
		(void)steps;
		memset(parameters, 0, sizeof(parameters));
		for (k = 0; k < count && k < PARAMETERS; k++)
			parameters[k] = (uint32_t)strtoul(at, &at, 10);
		if (fresh || !ctx) {
			if (ctx)
				amdgpu_atom_destroy(ctx);
			ctx = amdgpu_atom_parse(&card, rom);
			if (!ctx) {
				printf("open failed\n");
				return 0;
			}
			memset(scratch, 0, (SCRATCH_WORDS + 4096) * 4);
			ctx->scratch = scratch;
			ctx->scratch_size_bytes = SCRATCH_WORDS * 4;
		}
		round_now++;
		seed = (uint32_t)with_seed;
		ticks = 0;
		printf("run %d table %lu seed %lu\n", run++, table, with_seed);
		/* (Linux does not hold the table's number against the list's
		 * length: a number beyond it is not asked of it. A table the
		 * list has no place for answers as one that is stuck does.) */
		if (table >= (get_u16(rom, ctx->cmd_table) - 4u) / 2) {
			printf("end absent\n");
		} else {
			bool there = get_u16(rom, ctx->cmd_table + 4 + 2 * table) != 0;

			answer = amdgpu_atom_execute_table(ctx, table, parameters);
			printf("end %s\n", answer == 0 ? "done" : there ? "stuck" : "absent");
		}
		printf("ps");
		for (k = 0; k < 16; k++)
			printf(" %08x", parameters[k]);
		printf(" %016llx\n", (unsigned long long)fold(parameters, PARAMETERS));
		printf("state %08x %08x %02x %04x %d %d %04x %04x %08x %x\n", ctx->divmul[0], ctx->divmul[1], ctx->shift,
		       ctx->io_attr, ctx->cs_equal, ctx->cs_above, ctx->data_block, ctx->reg_block, ctx->fb_base, ctx->io_mode);
		printf("fb %016llx\n", (unsigned long long)fold(scratch, SCRATCH_WORDS));
	}
	fclose(file);
	return 0;
}
