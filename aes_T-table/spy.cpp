#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <openssl/aes.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
//#include "../../cacheutils.h"
#include <map>
#include <vector>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <time.h>
#include <x86intrin.h>

unsigned char key[] =
{
	0xdc, 0x5, 0xca, 0x5e,
	0x86, 0x3b, 0xf9, 0x7,
	0x9d, 0x9d, 0x1a, 0xeb,
	0x4a, 0x64, 0x58, 0x87,
};

size_t sum;
size_t scount;
unsigned int junk;

std::map<char*, std::map<size_t, size_t> > timings;

int main()
{
	char executable_path[1000];
	ssize_t ret = readlink("/proc/self/exe", executable_path, 1000);
	assert(ret != -1);

	int len = strlen(executable_path);
	int i;
	for (i = len - 1 ; i >=0 ; i --) {
		if(executable_path[i] == '/') {
			executable_path[i] = '\0';
			break;
		}
	}
	executable_path[i] = '\0';

	strcat(executable_path, "/key.txt");
	printf("executable_path = %s\n", executable_path);

	int key_fd = open(executable_path, O_RDONLY);
	assert(key_fd != -1);
	ssize_t res = read(key_fd, key, sizeof(key));
	assert(res == 16);

	unsigned char plaintext[] =
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	unsigned char ciphertext[128] = 
	{
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
	};

	AES_KEY key_struct;
	memset(&key_struct, 0, sizeof(key_struct));

	AES_set_encrypt_key(key, 128, &key_struct);


	AES_encrypt(plaintext, ciphertext, &key_struct);

	return 0;
}

