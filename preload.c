#define _GNU_SOURCE
#include <dlfcn.h>

#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef size_t (*orig_fwrite_f_type)(const void *ptr, size_t size, size_t nmemb,
				     FILE *stream);
typedef size_t (*orig_fread_f_type)(const void *ptr, size_t size, size_t nmemb,
				    FILE *stream);

static unsigned long long num_recs = 0;
static unsigned char *data = NULL;

static FILE* const FILE_POINTER = (FILE*)0xDEADBEEF;

FILE *fopen(const char *path, const char *mode) {
#ifdef DEBUG
  printf("fopen(%s, %s);\n", path, mode);
#endif /* DEBUG */

  /* Assumption: fopen will only be called once, and it will be to open the
   * output file for writing. */
  assert(num_recs == 0);
  assert(data == NULL);

  char* num_recs_char = getenv("SORT_NUM_RECS");
  if (!num_recs_char) {
    fprintf(stderr, "Unable to read SORT_NUM_RECS environment variable.\n");
    exit(8);
  }
  num_recs = atoll(num_recs_char);

  int verifying = (getenv("VERIFY") != 0);
  
  int data_fd = shm_open("/sort_data", O_RDWR |
			 (verifying ? 0 : (O_CREAT | O_TRUNC)), 0700);
  if (data_fd == -1) {
    perror("Unable to open /sort_data shared memory segment");
    exit(8);
  }

  if (!verifying) {
    if (ftruncate64(data_fd, num_recs * 100ull) == -1) {
      perror("Failed to set size of /sort_data shared memory segment");
      exit(8);
    }
  }

  data = (unsigned char*)mmap(NULL, num_recs * 100ull, PROT_READ |
			      (verifying ? 0 : PROT_WRITE),
			      MAP_SHARED | MAP_NORESERVE, data_fd, 0);

  if (data == MAP_FAILED) {
    perror("Unable to map /sort_data shared memory segment");
    exit(8);
  }

  return FILE_POINTER;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb,
	      FILE *stream) {
#ifdef DEBUG
  printf("fwrite(%p, %ld, %ld, %p);\n", ptr, size, nmemb, stream);
  printf("key = ");
  for (int i = 0; i < 10; i++) {
    printf("%02x ", ((const unsigned char *)ptr)[i]);
  }
  printf("\n");
#endif /* DEBUG */

  if (stream != FILE_POINTER) {
    orig_fwrite_f_type orig_fwrite;
    orig_fwrite = (orig_fwrite_f_type)dlsym(RTLD_NEXT, "fwrite");
    return orig_fwrite(ptr, size, nmemb, stream);
  }

  assert(data != NULL);
  static unsigned char* cur = NULL;

  if (cur == NULL) {
    cur = data;
  }

  memcpy(cur, ptr, size * nmemb);
  cur += size * nmemb;

  assert(cur - data <= num_recs * 100ull);
  
  return nmemb;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
#ifdef DEBUG
  printf("fread(%p, %ld, %ld, %p);\n", ptr, size, nmemb, stream);
#endif /* DEBUG */

  if (stream != FILE_POINTER) {
    orig_fread_f_type orig_fread;
    orig_fread = (orig_fread_f_type)dlsym(RTLD_NEXT, "fread");
    return orig_fread(ptr, size, nmemb, stream);
  }

  assert(data != NULL);
  static unsigned char* cur = NULL;

  if (cur == NULL) {
    cur = data;
  }

  if (cur - data < num_recs * 100ull) {  
    memcpy(ptr, cur, size * nmemb);
    cur += size * nmemb;

#ifdef DEBUG
    printf("key = ");
    for (int i = 0; i < 10; i++) {
      printf("%02x ", ((const unsigned char *)ptr)[i]);
    }
    printf("\n");
#endif /* DEBUG */

    return nmemb;
  } else {
    return 0;
  }
}
