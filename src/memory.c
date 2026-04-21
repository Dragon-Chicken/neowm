#include "main.h"
#include "memory.h"

long allocs = 0;
long total_allocs = 0;

void *nwm_malloc(size_t size, const char *file, int line, const char *func) {
  allocs++;
  total_allocs++;
  void *p = malloc(size);
  printf("\tmalloc: ");
  printf("p:%p, ", p);
  printf("a:%ld, ", allocs);
  printf("t:%ld, ", total_allocs);
  printf("f:%s, ", file);
  printf("f:%s, ", func);
  printf("l:%d\n", line);
  return p;
}

void nwm_free(void *ptr, const char *file, int line, const char *func) {
  allocs--;
  printf("\tfree: ");
  printf("p:%p, ", ptr);
  printf("a:%ld, ", allocs);
  printf("t:%ld, ", total_allocs);
  printf("f:%s, ", file);
  printf("f:%s, ", func);
  printf("l:%d\n", line);
  free(ptr);
}
