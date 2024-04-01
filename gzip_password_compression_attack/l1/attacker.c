#include <stdio.h>
#include <assert.h>
#include <x86intrin.h>
#include <string.h>

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

// getconf -a | grep CACHE
#define L1_ASSOC (8)
/* structure of g_l1_cache:
 * 1st byte of CL has the index of the next CL
 * Rest are hit/miss per experiment
 */
unsigned char g_l1_cache_buf[L1_ASSOC * 0x1000] __attribute__ ((aligned (0x1000)));
register unsigned char* g_l1_cache asm ("r13");
unsigned char g_fake_victim[0x1000] __attribute__ ((aligned (0x1000)));
// putting this in a register eliminates a cl accesed
register unsigned long g_threshold asm ("r12");

__attribute__((always_inline)) 
static inline unsigned long rdtscp( register unsigned int & aux )
{ // need this function so I don't touch registers
	unsigned long rax,rdx;
	asm volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
	return (rdx << 32) + rax;
}

__attribute__((always_inline)) inline
unsigned long measure_access (register int offset) {
	offset /= 0x40;
	offset *= 0x40;

	register unsigned int junk;
	register unsigned long time1, time2;
	register char tmp = 0;

	//_mm_mfence();
	time1 = rdtscp(junk);
	//_mm_mfence();
	__asm__ __volatile__("":::"memory");
	for(register int i = 0 ; i < L1_ASSOC ; i ++) {
		tmp = g_l1_cache[tmp * 0x1000 + offset];
	}

	__asm__ __volatile__("":::"memory");
	//_mm_mfence();
	time2 = rdtscp(junk);
	//_mm_mfence();

	g_l1_cache[7 * 0x1000 + offset] = tmp;
	return time2 - time1;
}

__attribute__((always_inline)) 
inline void prime_l1_cache () {
	memset(g_l1_cache, 0, sizeof(g_l1_cache_buf));

	for(register int page = 0 ; page < sizeof(g_l1_cache_buf)/0x1000; page ++) {
		for(register int offset = 0 ; offset < 0x1000 ; offset += 64) {
			g_l1_cache[page * 0x1000 + offset] = page + 1;
		}
	}
}

void print_l1_cache() {
	for(int i = 0 ; i < sizeof(g_l1_cache_buf) ; i ++) {
		if(i % 64 == 0) {
			printf("\n");
		}
		if(i % 0x1000 == 0) {
			printf("========================\n");
		}
		if(i % 64 == 0) {
			printf("%4x: ", i);
		}
		//if(g_l1_cache[i] != 0) printf(COLOR_RED);
		printf("%x ", g_l1_cache[i] & 0xff);
		//if(g_l1_cache[i] != 0) printf(COLOR_RESET);
	}
	printf("\n");
	printf("========================\n");
}

inline int experiment2byte(int experiment) {
	return (experiment / (63*8) * 0x1000) + (experiment % 63 * 8 / 8) + 1;
}

//__attribute__((optimize("unroll-loops")))
//__attribute__((always_inline)) 
void probe_l1_cache() {
	// nr experiments = nr_pages * nr_butes to fill in cl * bits in byte
	for(register int experiment = 0 ; experiment < 8*63*8 ; experiment ++) {
		register int cl = 0;
		for(; cl < 64 ;) {
			register unsigned long timing = measure_access(cl * 64);
#if 0
			printf("64*cl + experiment2byte(%d) = 0x%x\n",
					experiment,
					64*cl + experiment2byte(experiment));
#endif
			g_l1_cache[64*cl + experiment2byte(experiment)] <<= 1;
			g_l1_cache[64*cl + experiment2byte(experiment)] += !!(timing > g_threshold);
			cl++;
		}
	}
}

unsigned long detect_cache_threshold () {
	volatile int junk;
	unsigned long times[100] = {0};

	for(int i = 0 ; i < 100 ; ) {
		// even is hit
		times[i] = measure_access(400);
		i ++;

		// odd if miss
		junk = g_fake_victim[400];
		times[i] = measure_access(400);
		i ++;

	}

	unsigned long even_sum = 0, even_cnt = 0;
	unsigned long odd_sum = 0, odd_cnt = 0;
	for(int i = 2 ; i < 100 ; ) {
		even_sum += times[i];
		even_cnt ++;
		i ++;

		odd_sum += times[i];
		odd_cnt ++;
		i ++;
	}

#if 1
	printf("hit_avg = %d\n", even_sum/even_cnt);
	printf("miss_avg = %d\n", odd_sum/odd_cnt);
#endif

	return (even_sum/even_cnt + odd_sum/odd_cnt) / 2;
}

int main()
{
	g_l1_cache = g_l1_cache_buf;


	printf("g_l1_cache = %p\n", g_l1_cache);
	memset(g_l1_cache, 5, sizeof(g_l1_cache_buf));
	assert(((unsigned long)g_l1_cache & 0xfff) == 0);

	printf("g_fake_victim = %p\n", g_fake_victim);
	memset(g_fake_victim, 5, sizeof(g_fake_victim));
	assert(((unsigned long)g_fake_victim & 0xfff) == 0);

	for(int page = 0 ; page < sizeof(g_l1_cache_buf)/0x1000; page ++) {
		for(int offset = 0 ; offset < 0x1000 ; offset ++) {
			g_l1_cache[page * 0x1000 + offset] = page + 1;
		}
	}

	g_threshold = detect_cache_threshold();
	//g_threshold = 113;
	g_threshold = 78;
	printf("g_threshold = %d\n", g_threshold);

	prime_l1_cache();
	//volatile int junk = g_fake_victim[0x240];
	probe_l1_cache();
	print_l1_cache();

	//printf(COLOR_CYAN "Hello! ^_^\n" COLOR_RESET);
}
