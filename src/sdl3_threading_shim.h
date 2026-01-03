// SDL 3 Compatibility Shim for Threading Primitives
// Provides SDL 2-style mutex and condition variable APIs using C11 threading

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <threads.h>

// SDL 2 compatible mutex type
typedef struct {
    mtx_t mutex;
} SDL_mutex;

// SDL 2 compatible condition variable type
typedef struct {
    cnd_t cond;
} SDL_cond;

// Create a mutex (SDL 2 compatible)
SDL_mutex* SDL_CreateMutex(void);

// Destroy a mutex
void SDL_DestroyMutex(SDL_mutex* mutex);

// Lock a mutex
int SDL_LockMutex(SDL_mutex* mutex);

// Unlock a mutex
int SDL_UnlockMutex(SDL_mutex* mutex);

// Create a condition variable
SDL_cond* SDL_CreateCond(void);

// Destroy a condition variable
void SDL_DestroyCond(SDL_cond* cond);

// Wait for a condition variable
int SDL_CondWait(SDL_cond* cond, SDL_mutex* mutex);

// Signal a condition variable
int SDL_CondSignal(SDL_cond* cond);

// Broadcast a condition variable
int SDL_CondBroadcast(SDL_cond* cond);

#ifdef __cplusplus
}
#endif
