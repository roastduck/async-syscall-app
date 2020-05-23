#include <ctime>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cassert>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "async.h"

const int MODE_NAVIE = 0;
const int MODE_ASYNC_PREFETCH = 1;
const int MODE_SYNC_PREFETCH = 2;

const int length = 1024 * 1024 * 1024;
const int n_op = 1000000;
int op_addr[n_op];

// some arbitrary operations
int op(int x) {
	for (int i = 0; i < 100; i++)
		x = (x * 513 + 912747) % 1037593;
	return x;
}

void print_help(const char *name) {
	printf("Usage: %s <naive|sync-prefetch|async-prefetch>\n", name);
}

int main(int argc, char **argv) {
	int mode;
	if (argc != 2) {
		print_help(argv[0]);
		exit(1);
	}
	if (strcmp(argv[1], "naive") == 0)
		mode = MODE_NAVIE;
	else if (strcmp(argv[1], "async-prefetch") == 0)
		mode = MODE_ASYNC_PREFETCH;
	else if (strcmp(argv[1], "sync-prefetch") == 0)
		mode = MODE_SYNC_PREFETCH;
	else {
		print_help(argv[0]);
		exit(1);
	}

	srand(0);
	for (int i = 0; i < n_op; i++)
		op_addr[i] = rand() % length;

	async_handle_t handle;
	async_init(&handle);


	int fd = open("data", O_RDWR);
	uint8_t *buff = (uint8_t*)mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	clock_t t0 = clock();
	switch (mode) {
	case MODE_NAVIE:
		for (int i = 0; i < n_op; i++)
			buff[op_addr[i]] = (uint8_t)op(buff[op_addr[i]]);
		break;
	case MODE_SYNC_PREFETCH:
		for (int i = 0; i < n_op; i++) {
			uint8_t x = buff[op_addr[i]];
			if (i + 1 < n_op)
				__builtin_prefetch(&buff[op_addr[i + 1]]);
			buff[op_addr[i]] = (uint8_t)op(x);
		}
		break;
	case MODE_ASYNC_PREFETCH:
		for (int i = 0; i < n_op; i++) {
			uint8_t x = buff[op_addr[i]];
			if (i + 1 < n_op)
				async_prefetch(handle, &buff[op_addr[i + 1]]);
			buff[op_addr[i]] = (uint8_t)op(x);
		}
		break;
	}
	clock_t t1 = clock();
	printf("Time = %fs\n", (double)(t1 - t0) / CLOCKS_PER_SEC);

	close(fd);
	return 0;
}

