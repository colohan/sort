#include <algorithm>
#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

using namespace std;

struct Record {
  char key[10];
  char data[90];
};

bool compare_records (Record a, Record b) {
  return memcmp(a.key, b.key, 10) < 0;
}

string elapsed_time(timeval start, timeval end) {
  timeval diff;
  diff.tv_sec = end.tv_sec - start.tv_sec;
  if (end.tv_usec < start.tv_usec) {
    diff.tv_sec -= 1;
    end.tv_usec += 1000000;
    assert(end.tv_usec > start.tv_usec);
  }
  diff.tv_usec = end.tv_usec - start.tv_usec;

  char result[30];
  sprintf(result, "%lu.%06lu", diff.tv_sec, diff.tv_usec);
  return string(result);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " <data_file_name>" << endl;
    return 8;
  }

  int data_fd = open(argv[1], O_RDWR);
  if (data_fd == -1) {
    perror("Unable to open file");
    return 8;
  }

  struct stat sb;
  if (fstat(data_fd, &sb) == -1) {
    perror("Unable to stat file");
    return 8;
  }
  
  Record* data = static_cast<Record *>(mmap(NULL, sb.st_size,
					    PROT_READ | PROT_WRITE,
					    MAP_SHARED | MAP_NORESERVE,
					    data_fd, 0));

  if (data == MAP_FAILED) {
    perror("Unable to map /sort_data shared memory segment");
    return 8;
  }

  struct timeval start_sort;
  {
    int rc = gettimeofday(&start_sort, NULL);
    assert(rc == 0);
  }

  // Now sort the data!  Simplest possible sort implementation -- just invoke
  // std::sort().
  std::sort(data, data + (sb.st_size / 100), compare_records);

  struct timeval done;
  {
    int rc = gettimeofday(&done, NULL);
    assert(rc == 0);
  }

  cout << "Sort time: " << elapsed_time(start_sort, done) << "s" << endl;

  return 0;
}
