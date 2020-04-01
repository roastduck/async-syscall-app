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


const size_t PAGE_SIZE = 4096;
char *ptr;
void *thread_call(void * arg){
	prctl(PR_INIT_ASYNC, ptr, 0, 0, 0);
}
void *thread_work(void * arg){
	for (int i = 0; i < 6; ++ i){
		usleep(1000 * 1000);
		printf("after %d %d\n", i, ptr[0]);
	}
}
int main() {
	posix_memalign((void**)&ptr, PAGE_SIZE, PAGE_SIZE);
	mlock(ptr, PAGE_SIZE);
	printf("%p\n", ptr);
	memset(ptr, 0, PAGE_SIZE);
	printf("before %d\n", ptr[0]);
	pthread_t thr1, thr2;
	pthread_create(&thr1, NULL, thread_call, NULL);
	pthread_create(&thr2, NULL, thread_work, NULL);
	pthread_join(thr1, NULL);
	pthread_join(thr2, NULL);
	//prctl(PR_INIT_ASYNC, 0, 0, 0, 0);
	//prctl(PR_WAIT_ASYNC, 0, 0, 0, 0);

	return 0;
}

