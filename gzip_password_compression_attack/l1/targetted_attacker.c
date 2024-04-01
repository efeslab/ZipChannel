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

// logging extension
//#define NR_BLOCKS (30)
#define NR_BLOCKS (1)
constexpr unsigned long long g_monitor_mask =
					(1ull << (0xac0 / 0x40)) | // $
					(1ull << (0xe80 / 0x40)) | // 3
					0;


constexpr unsigned long long g_log_mask = ~g_monitor_mask;
unsigned char g_log[0x4000 * NR_BLOCKS] __attribute__ ((aligned (0x4000))); // each set of 4 pages goes together

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
	int experiment_i = 0;
	register unsigned long long monitor_mask = g_monitor_mask; // helps tracking the experiment cl
	printf("iteration %d ", experiment_i);

	for(register int log_block = 0 ; log_block < NR_BLOCKS ; log_block ++) {
		for(register int log_offset = 0 ; log_offset < 64 ; log_offset ++) {
			register unsigned long long log_mask = g_log_mask;
			do {
				register unsigned long log_cl = (0x40 *  __builtin_ctzll(log_mask));

				for(register int log_page = 0 ; log_page < 4 ; log_page ++) {
					register unsigned long monitor_cl = ( __builtin_ctzll(monitor_mask));
					int reading = g_log[log_block * 0x4000 + log_page * 0x1000 + log_cl + log_offset];
					if (reading) {
						printf("0x%lx ", monitor_cl * 64);
					}
					monitor_mask &= ~(1ull << __builtin_ctzll(monitor_mask));

					if(monitor_mask == 0ull) {
						monitor_mask = g_monitor_mask;
						experiment_i ++;
						printf("\niteration %d ", experiment_i);
					}
				}

				log_mask &= ~(1ull << __builtin_ctzll(log_mask));

			} while (log_mask != 0ull);
		}
	}
	printf("\n");
}

inline int experiment2byte(int experiment) {
	return (experiment / (63*8) * 0x1000) + (experiment % 63 * 8 / 8) + 1;
}

//__attribute__((optimize("unroll-loops")))
//__attribute__((always_inline)) 
void probe_l1_cache() {
	register unsigned long long monitor_mask = g_monitor_mask; // helps tracking the experiment cl

	for(register int log_block = 0 ; log_block < NR_BLOCKS ; log_block ++) {
		for(register int log_offset = 0 ; log_offset < 64 ; log_offset ++) {
			register unsigned long long log_mask = g_log_mask;
			do {
				register unsigned long log_cl = (0x40 *  __builtin_ctzll(log_mask));

				for(register int log_page = 0 ; log_page < 4 ; log_page ++) {
					register unsigned long monitor_cl = ( __builtin_ctzll(monitor_mask));
					register unsigned long timing = measure_access(monitor_cl * 64);
					g_log[log_block * 0x4000 + log_page * 0x1000 + log_cl + log_offset] = (timing > g_threshold); // TODO: maybe need !! here
					monitor_mask &= ~(1ull << __builtin_ctzll(monitor_mask));

					if(monitor_mask == 0ull) {
						monitor_mask = g_monitor_mask;
					}
				}

				log_mask &= ~(1ull << __builtin_ctzll(log_mask));

			} while (log_mask != 0ull);
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
	printf("g_monitor_mask = 0x%lx\n", g_monitor_mask);

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
	g_threshold = 113; // threshold for ht with prefetcher on
	printf("set g_threshold = %d\n", g_threshold);

	memset(g_log, 5, sizeof(g_log));
	prime_l1_cache();
	//volatile int junk = g_fake_victim[0x280];
	probe_l1_cache();
	print_l1_cache();

	//printf(COLOR_CYAN "Hello! ^_^\n" COLOR_RESET);
}
