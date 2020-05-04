#include <ctime>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cassert>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/kernel.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define pause() __builtin_ia32_pause()

#define PAGE_SIZE 4096
#define BUF_SIZE PAGE_SIZE

#define ASYNC_CMD_PREFETCH 0

typedef struct {
	int cmd;
	void *addr;
} queue_elem_t;

#define QUEUE_LEN ((BUF_SIZE - 3 * sizeof(int)) / sizeof(queue_elem_t))
typedef struct {
	// Elements in [head, tail_done) are in the queue
	// Elements in [tail_done, tail_begin) are being inserted
	int head;
	queue_elem_t elem[QUEUE_LEN];
	int tail_begin, tail_done; // Make the head and tails in different cache lines
} AsyncQueue;

// Multiple producers are supported
// return true when success
bool enqueue(AsyncQueue *q, queue_elem_t x)
{
	int expect_tail = __atomic_load_n(&q->tail_begin, __ATOMIC_ACQUIRE);
	while (true)
	{
		int head = __atomic_load_n(&q->head, __ATOMIC_ACQUIRE); // read head after reading tail
		// otherwise head may run over tail
		if (unlikely((expect_tail - head + QUEUE_LEN) % QUEUE_LEN >= QUEUE_LEN - 1))
			return false;
		if (likely(__atomic_compare_exchange_n(
						&q->tail_begin, &expect_tail, (expect_tail + 1) % QUEUE_LEN,
						false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)))
			break;
		pause();
	}
	q->elem[expect_tail] = x;
	int expect_tail_volatile = expect_tail;
	while (unlikely(!__atomic_compare_exchange_n(
					&q->tail_done, &expect_tail_volatile, (expect_tail + 1) % QUEUE_LEN,
					false, __ATOMIC_RELEASE, __ATOMIC_RELAXED)))
	{
		expect_tail_volatile = expect_tail;
		pause();
	}
	return true;
}

void *thread_call(void *ptr) {
	while (true)
		prctl(PR_INIT_ASYNC, ptr, 0, 0, 0);
	return NULL;
}

typedef void *async_handle_t;

void async_init(async_handle_t *handle) {
	posix_memalign(handle, PAGE_SIZE, PAGE_SIZE);
	mlock(*handle, PAGE_SIZE);
	memset(*handle, 0, PAGE_SIZE);
	pthread_t th;
	pthread_create(&th, NULL, thread_call, *handle);
	pthread_detach(th);
}

void async_prefetch(async_handle_t handle, void *addr) {
	AsyncQueue *q = (AsyncQueue *)handle;
	enqueue(q, queue_elem_t{ASYNC_CMD_PREFETCH, addr});
}

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

