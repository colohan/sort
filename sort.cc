#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <sstream>
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

void usage_error(char *prog_name) {
  cerr << "Usage: " << prog_name << ": <data_set_size_in_bytes>" << endl;
  cerr << "Note that data_set_size_in_bytes must be at least 100." << endl;
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
    usage_error(argv[0]);
    return 8;
  }

  uint64_t data_set_size = atoll(argv[1]);

  if (data_set_size < 100) {
    usage_error(argv[0]);
    return 8;
  }

  uint64_t num_recs = data_set_size / 100;
  cout << "data_set_size: " << data_set_size << endl;
  cout << "num_recs:      " << num_recs << endl;
  cout << "ram_required:  " << data_set_size / 1024 / 1024 / 1024 << "GB"
       << endl;

  struct timeval start_generation;
  {
    int rc = gettimeofday(&start_generation, NULL);
    assert(rc == 0);
  }

  // Invoke gensort, and have it put its output into a shared memory segment
  // instead of a file.  (This is done through the preload.so wrapper black
  // magic.)
  cout << "GENERATING DATA ===========================================" << endl;
  {
    ostringstream cmd;
    cmd << "SORT_NUM_RECS=" << num_recs
	<< " LD_PRELOAD=$PWD/preload.so ./gensort-1.5/gensort "
	<< num_recs << " sort_data";
    system(cmd.str().c_str());
  }

  struct timeval start_verify_input;
  {
    int rc = gettimeofday(&start_verify_input, NULL);
    assert(rc == 0);
  }

  // Invoke valsort to verify that our input is not sorted:
  cout << "VERIFYING SORT INPUT ======================================" << endl;
  {
    ostringstream cmd;
    cmd << "VERIFY=1 SORT_NUM_RECS=" << num_recs
	<< " LD_PRELOAD=$PWD/preload.so ./gensort-1.5/valsort sort_data";
    system(cmd.str().c_str());
  }



  int data_fd = shm_open("/sort_data", O_RDWR, 0700);
  if (data_fd == -1) {
    perror("Unable to open /sort_data shared memory segment");
    exit(8);
  }

  // Get the output of gensort:
  Record* data = static_cast<Record *>(mmap(NULL, num_recs * 100ull,
					    PROT_READ | PROT_WRITE,
					    MAP_SHARED | MAP_NORESERVE,
					    data_fd, 0));

  if (data == MAP_FAILED) {
    perror("Unable to map /sort_data shared memory segment");
    exit(8);
  }

  struct timeval start_sort;
  {
    int rc = gettimeofday(&start_sort, NULL);
    assert(rc == 0);
  }

  // Now sort the data!  Simplest possible sort implementation -- just invoke
  // std::sort().
  cout << "SORTING DATA ==============================================" << endl;
  std::sort(data, data + num_recs, compare_records);

  struct timeval start_verify;
  {
    int rc = gettimeofday(&start_verify, NULL);
    assert(rc == 0);
  }

  // Invoke valsort to verify that our output is correct:
  cout << "VERIFYING SORT ============================================" << endl;
  {
    ostringstream cmd;
    cmd << "VERIFY=1 SORT_NUM_RECS=" << num_recs
	<< " LD_PRELOAD=$PWD/preload.so ./gensort-1.5/valsort sort_data";
    system(cmd.str().c_str());
  }

  struct timeval done;
  {
    int rc = gettimeofday(&done, NULL);
    assert(rc == 0);
  }

  cout << "Data generation: " << elapsed_time(start_generation, start_verify_input) << "s" << endl;
  cout << "Data validation: " << elapsed_time(start_verify_input, start_sort) << "s" << endl;
  cout << "Sort:            " << elapsed_time(start_sort, start_verify) << "s" << endl;
  cout << "Verify:          " << elapsed_time(start_verify, done) << "s" << endl;

  return 0;
}
