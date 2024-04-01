#include <vector>
#include <cstdint>
#include <cstddef>
#include <zlib.h>
#include <assert.h>
#include <cstdio>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

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

int main(int argc, char *argv[])
{
	// check that command line arguments are valid 
	if (argc != 2) { 
		printf("Usage: %s <file to compress>\n", argv[0]); 
		return 1; 
	} 

	const char* filename = argv[1];

	int fd = open(filename, O_RDONLY);
	assert(fd != -1);

	struct stat st;
	stat(filename, &st);
	off_t file_size = st.st_size;

	char in_data[file_size];

	ssize_t res = read(fd, in_data, sizeof(in_data));
	assert(res == file_size);

	std::vector<uint8_t> out_data;
	compress_memory(in_data, sizeof(in_data), out_data);
	//printf("Hello world!\n");
}
