#include <vector>
#include <cstdint>
#include <cstddef>
#include <zlib.h>
#include <assert.h>
#include <cstdio>
#include <string.h>
#include <fcntl.h>

void compress_memory(void *in_data, size_t in_data_size, std::vector<uint8_t> &out_data)
{
	std::vector<uint8_t> buffer;

	const size_t BUFSIZE = 128 * 1024;
	uint8_t temp_buffer[BUFSIZE];

	z_stream strm;
	strm.zalloc = 0;
	strm.zfree = 0;
	strm.next_in = reinterpret_cast<uint8_t *>(in_data);
	strm.avail_in = in_data_size;
	strm.next_out = temp_buffer;
	strm.avail_out = BUFSIZE;

	deflateInit(&strm, Z_BEST_COMPRESSION);

	while (strm.avail_in != 0)
	{
		int res = deflate(&strm, Z_NO_FLUSH);
		assert(res == Z_OK);
		if (strm.avail_out == 0)
		{
			buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE);
			strm.next_out = temp_buffer;
			strm.avail_out = BUFSIZE;
		}
	}

	int deflate_res = Z_OK;
	while (deflate_res == Z_OK)
	{
		if (strm.avail_out == 0)
		{
			buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE);
			strm.next_out = temp_buffer;
			strm.avail_out = BUFSIZE;
		}
		deflate_res = deflate(&strm, Z_FINISH);
	}

	assert(deflate_res == Z_STREAM_END);
	buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE - strm.avail_out);
	deflateEnd(&strm);

	out_data.swap(buffer);
}

int main()
{
	char in_data[2000];

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

	strcat(executable_path, "/data.txt");
	printf("executable_path = %s\n", executable_path);

	int key_fd = open(executable_path, O_RDONLY);
	assert(key_fd != -1);
	ssize_t res = read(key_fd, in_data, sizeof(in_data));
	assert(res == 1604);

	std::vector<uint8_t> out_data;
	compress_memory(in_data, sizeof(in_data), out_data);
	//printf("Hello world!\n");
}
