// test_jem.c
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

int main() {
  void* h = dlopen("libjemalloc.so.2", RTLD_NOW | RTLD_LOCAL);
  if (!h) { puts(dlerror()); return 1; }
  void* m = dlsym(h, "malloc");
  void* f = dlsym(h, "free");
  printf("malloc=%p free=%p err=%s\n", m, f, dlerror());
  return 0;
}

