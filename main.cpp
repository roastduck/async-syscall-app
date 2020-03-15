#include <cerrno>
#include <cstdio>
#include <cstddef>
#include <cassert>
#include <sys/prctl.h>

int main() {
	prctl(PR_INIT_ASYNC, 0, 0, 0, 0);
	prctl(PR_WAIT_ASYNC, 0, 0, 0, 0);
	return 0;
}

