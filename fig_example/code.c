#include <unistd.h>
#include <fcntl.h> 

int main() {
	unsigned char buf[100];
	int fd = open("file.txt", O_RDONLY);
	ssize_t res = read(fd, buf, sizeof(buf));
	if(res == -1) return 1;

	volatile int hist[65536] = {0};
	for(int i = 0 ; i < res ; i ++) {
		unsigned char curr = buf[i];
		hist[curr] ++;
	}
}

