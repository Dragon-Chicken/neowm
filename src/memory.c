#include "main.h"
#include "memory.h"

// this file is only used when the wm is compiled for debug
// so it doesn't need to be included by the compiler

#ifdef NWM_DEBUG
void *pointers[100] = {NULL};

long allocs = 0;
long total_allocs = 0;

void *debug_malloc(size_t size, const char *file, int line, const char *func) {
  void *p = malloc(size);

  int i;
  for (i = 0; i < 100; i++) {
    if (pointers[i] == NULL) {
      pointers[i] = p;
      break;
    }
    if (i == 99) {
      printerr("ran out of space in array\n");
      exitwm(0);
    }
  }

  allocs++;
  total_allocs++;
  printf("\tmalloc: ");
  printf("i:%d, ", i);
  printf("p:%p, ", p);
  printf("a:%ld, ", allocs);
  printf("t:%ld, ", total_allocs);
  printf("f:%s, ", file);
  printf("f:%s, ", func);
  printf("l:%d\n", line);
  return p;
}

void debug_free(void *ptr, const char *file, int line, const char *func) {
  int i;
  for (i = 0; i < 100; i++) {
    if (pointers[i] == ptr) {
      pointers[i] = NULL;
      break;
    }
    if (i == 99) {
      printerr("what are we freeing here...?\n");
      exitwm(0);
    }
  }

  allocs--;
  printf("\tfree: ");
  printf("i:%d, ", i);
  printf("p:%p, ", ptr);
  printf("a:%ld, ", allocs);
  printf("t:%ld, ", total_allocs);
  printf("f:%s, ", file);
  printf("f:%s, ", func);
  printf("l:%d\n", line);
  free(ptr);
}

void checkallocs(void) {
  if (allocs != 0) {
    printerr("there's a memory leak\n");
    printf("list of remaining pointers:\n");
    for (int i = 0; i < 100; i++) {
      if (pointers[i]) {
        printf("%d %p\n", i, pointers[i]);
      }
    }
  }
}

#endif
