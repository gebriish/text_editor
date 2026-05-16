#include "editor.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

funcdef bytes
platform_load_entire_file(string path, Arena *allocator)
{
	Slice<char> cpath = alloc_slice(allocator, char, path.len + 1);
	if (!cpath.raw) return {};

	for (u64 i = 0; i < path.len; i++) {
		cpath[i] = (char)path.raw[i];
	}
	cpath[path.len] = 0;

	int fd = open(cpath.raw, O_RDONLY);
	if (fd < 0) {
		return {};
	}

	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		return {};
	}

	u64 size = (u64) st.st_size;

	bytes data= alloc_slice(allocator, u8, size);
	if (!data.raw) {
		close(fd);
		return {};
	}

	u64 total = 0;
	while (total < size) {
		ssize_t n = read(fd, data.raw + total, size - total);
		if (n <= 0) {
			close(fd);
			return {};
		}
		total += (u64)n;
	}

	close(fd);

	return data;
}
