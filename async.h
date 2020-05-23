#ifndef ASYNC_H_
#define ASYNC_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void *async_handle_t;

void async_init(async_handle_t *handle);

void async_prefetch(async_handle_t handle, void *addr);

#ifdef __cplusplus
}
#endif

#endif // ASYNC_H_
