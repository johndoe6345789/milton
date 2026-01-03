// SDL 3 Compatibility Shim Implementation
// Implements SDL 2-style threading using C11 threads

#include "sdl3_threading_shim.h"
#include <stdlib.h>

SDL_mutex* SDL_CreateMutex(void)
{
    SDL_mutex* mutex = (SDL_mutex*)malloc(sizeof(SDL_mutex));
    if (mutex == NULL) {
        return NULL;
    }
    
    if (mtx_init(&mutex->mutex, mtx_plain) != thrd_success) {
        free(mutex);
        return NULL;
    }
    
    return mutex;
}

void SDL_DestroyMutex(SDL_mutex* mutex)
{
    if (mutex != NULL) {
        mtx_destroy(&mutex->mutex);
        free(mutex);
    }
}

int SDL_LockMutex(SDL_mutex* mutex)
{
    if (mutex == NULL) {
        return -1;
    }
    
    if (mtx_lock(&mutex->mutex) != thrd_success) {
        return -1;
    }
    
    return 0;
}

int SDL_UnlockMutex(SDL_mutex* mutex)
{
    if (mutex == NULL) {
        return -1;
    }
    
    if (mtx_unlock(&mutex->mutex) != thrd_success) {
        return -1;
    }
    
    return 0;
}

SDL_cond* SDL_CreateCond(void)
{
    SDL_cond* cond = (SDL_cond*)malloc(sizeof(SDL_cond));
    if (cond == NULL) {
        return NULL;
    }
    
    if (cnd_init(&cond->cond) != thrd_success) {
        free(cond);
        return NULL;
    }
    
    return cond;
}

void SDL_DestroyCond(SDL_cond* cond)
{
    if (cond != NULL) {
        cnd_destroy(&cond->cond);
        free(cond);
    }
}

int SDL_CondWait(SDL_cond* cond, SDL_mutex* mutex)
{
    if (cond == NULL || mutex == NULL) {
        return -1;
    }
    
    if (cnd_wait(&cond->cond, &mutex->mutex) != thrd_success) {
        return -1;
    }
    
    return 0;
}

int SDL_CondSignal(SDL_cond* cond)
{
    if (cond == NULL) {
        return -1;
    }
    
    if (cnd_signal(&cond->cond) != thrd_success) {
        return -1;
    }
    
    return 0;
}

int SDL_CondBroadcast(SDL_cond* cond)
{
    if (cond == NULL) {
        return -1;
    }
    
    if (cnd_broadcast(&cond->cond) != thrd_success) {
        return -1;
    }
    
    return 0;
}
