#ifndef NWM_MEMORY
#define NWM_MEMORY

#ifdef NWM_DEBUG

#define nwm_malloc(X) debug_malloc( X, __FILE__, __LINE__, __func__)
#define nwm_free(X) debug_free( X, __FILE__, __LINE__, __func__)

void *debug_malloc(size_t size, const char *file, int line, const char *func);
void debug_free(void *ptr, const char *file, int line, const char *func);
void checkallocs(void);

#else

#define nwm_malloc(X) malloc(X)
#define nwm_free(X) free(X)

#endif

#endif
