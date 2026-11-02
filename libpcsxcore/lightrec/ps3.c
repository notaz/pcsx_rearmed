#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/process.h>
#include <ppu-lv2.h>
#define system_call_1 lv2syscall1
#define system_call_8 lv2syscall8
#define system_call_5 lv2syscall5
#define sys_process_getpid sysProcessGetPid
#include "ps3.h"

unsigned char *lightrec_ps3_code;
extern unsigned char lightrec_ps3_text_code[8388608];
static uint64_t page_table[2];

static int is_hen(void)
{
	system_call_1(8, 0x1337);
	return_to_user_prog(int);
}

static int allocate_code(sys_pid_t pid)
{
	system_call_8(8, 0x7777, 0x0033, pid, 8388608, 0, 0x2f, 1,
		(uintptr_t)page_table);
	return_to_user_prog(int);
}

static int free_code(sys_pid_t pid)
{
	system_call_5(8, 0x7777, 0x0034, pid, 0x2f,
		(uintptr_t)page_table);
	return_to_user_prog(int);
}

int lightrec_ps3_init_code(FILE *log)
{
	int ret;
	if (lightrec_ps3_code)
		return 0;
	if (is_hen() == 0x1337)
	{
		lightrec_ps3_code = lightrec_ps3_text_code;
		memset(lightrec_ps3_code, 0, 8388608);
#if PS3_LIGHTREC_LOG
		if (log) {
			fprintf(log, "Using 8 MiB of writable executable text on HEN\n");
			fflush(log);
		}
#endif
		return 0;
	}
#if PS3_LIGHTREC_LOG
	if (log) {
		fprintf(log, "Allocating 8 MiB of executable memory via PS3MAPI\n");
		fflush(log);
	}
#else
	(void)log;
#endif
	ret = allocate_code(sys_process_getpid());
#if PS3_LIGHTREC_LOG
	if (log) {
		fprintf(log, "PS3MAPI allocation result=%08x address=%08x\n",
			(unsigned int)ret, (unsigned int)page_table[0]);
		fflush(log);
	}
#endif
	if (ret)
		return ret;
	if (!page_table[0] || !page_table[1])
		return -1;
	if (page_table[0] != (uintptr_t)page_table[0])
	{
		free_code(sys_process_getpid());
		memset(page_table, 0, sizeof(page_table));
		return -1;
	}
	lightrec_ps3_code = (unsigned char *)(uintptr_t)page_table[0];
	memset(lightrec_ps3_code, 0, 8388608);
	return 0;
}

void lightrec_ps3_free_code(void)
{
	if (lightrec_ps3_code == lightrec_ps3_text_code)
		lightrec_ps3_code = NULL;
	else if (lightrec_ps3_code && !free_code(sys_process_getpid()))
	{
		lightrec_ps3_code = NULL;
		memset(page_table, 0, sizeof(page_table));
	}
}

void lightrec_code_inv(void *ptr, uint32_t len)
{
	uintptr_t start = (uintptr_t)ptr & ~(uintptr_t)127;
	uintptr_t end = (uintptr_t)ptr + len;
	uintptr_t addr;

	for (addr = start; addr < end; addr += 128)
		__asm__ volatile("dcbst 0,%0" : : "r"(addr) : "memory");
	__asm__ volatile("sync" : : : "memory");
	for (addr = start; addr < end; addr += 128)
		__asm__ volatile("icbi 0,%0" : : "r"(addr) : "memory");
	__asm__ volatile("sync; isync" : : : "memory");
}

#if PS3_LIGHTREC_LOG && defined(PS3_LIGHTREC_DIAGNOSTICS)
static unsigned int compiled_blocks;
static unsigned int execution_calls;

static void lightrec_ps3_log(const char *event, unsigned int count,
		uint32_t pc, unsigned int size)
{
	FILE *log = lightrec_ps3_open_log("ps3-lightrec-selftest.log", "a");
	if (!log)
		return;
	fprintf(log, "%s count=%u pc=%08x size=%u\n", event, count,
		(unsigned int)pc, size);
	fclose(log);
}

void lightrec_ps3_log_block(uint32_t pc, unsigned int size)
{
	compiled_blocks++;
	if (compiled_blocks <= 16 || !(compiled_blocks & (compiled_blocks - 1)))
		lightrec_ps3_log("compiled", compiled_blocks, pc, size);
}

void lightrec_ps3_log_execution(uint32_t pc, int completed)
{
	if (!completed)
		execution_calls++;
	if (execution_calls <= 4 || !(execution_calls & (execution_calls - 1)))
		lightrec_ps3_log(completed ? "executed" : "executing",
			execution_calls, pc, compiled_blocks);
}
#endif
