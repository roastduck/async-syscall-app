#include <cerrno>
#include <cstdio>
#include <cstddef>
#include <cassert>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/kernel.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <cstring>
#include <pthread.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define pause() __builtin_ia32_pause()

#define PAGE_SIZE 4096
#define BUF_SIZE PAGE_SIZE

typedef struct {
	int x, y;
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

int main() {
	void *ptr;
	posix_memalign(&ptr, PAGE_SIZE, PAGE_SIZE);
	mlock(ptr, PAGE_SIZE);
	memset(ptr, 0, PAGE_SIZE);
	pthread_t th;
	pthread_create(&th, NULL, thread_call, ptr);
	pthread_detach(th);

	AsyncQueue *q = (AsyncQueue *)ptr;
	enqueue(q, queue_elem_t{1, 2});
	enqueue(q, queue_elem_t{2, 5});
	enqueue(q, queue_elem_t{5, 2});
	enqueue(q, queue_elem_t{6, 2});

	sleep(5);

	return 0;
}

