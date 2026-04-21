#ifndef NWM_MEMORY
#define NWM_MEMORY

#define debug_malloc(X) nwm_malloc( X, __FILE__, __LINE__, __FUNCTION__)
#define debug_free(X) nwm_free( X, __FILE__, __LINE__, __FUNCTION__)

void *nwm_malloc(size_t size, const char *file, int line, const char *func);
void nwm_free(void *ptr, const char *file, int line, const char *func);

#endif
