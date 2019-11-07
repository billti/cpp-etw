// Copyright 2019 Bill Ticehurst. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>

#include "./example-etw-provider.h"

namespace {

example::ExampleEtwProvider* example_etw_provider = nullptr;
int total_elements = 0;

void PrintArray(int* p_start, int count) {
  for (int i = 0; i < count; ++i) {
    printf("%d ", *(p_start + i));
  }
  printf("\n");
}

int64_t SortArray() {
  LARGE_INTEGER starting_time, ending_time, elapsed_microseconds;
  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&starting_time);

  example_etw_provider->Initialized();

  for (int i = 0; i < 10000; ++i) {
    // Returns a value between 0 and RAND_MAX (0x7fff i.e. 32767)
    // The below gets a random number between 1000 & 2000
    int r = (rand() * 1000 / 32767) + 1000;  // NOLINT
    total_elements += r;

    // Allocate an array of ints and fill it with random numbers
    int* p_elems = new int[r];
    for (int j = 0; j < r; ++j) {
      *(p_elems + j) = rand(); // NOLINT
    }

    example_etw_provider->StartSort(r);

    qsort(p_elems, r, sizeof(int), [](void const* a, void const* b) -> int {
      int _a = *(static_cast<const int*>(a));
      int _b = *(static_cast<const int*>(b));
      if (_a == _b) return 0;
      return _a > _b ? 1 : -1;
    });

    example_etw_provider->StopSort();

    delete[] p_elems;
  }

  example_etw_provider->Finished(total_elements);

  QueryPerformanceCounter(&ending_time);
  elapsed_microseconds.QuadPart = ending_time.QuadPart - starting_time.QuadPart;
  // We now have the elapsed number of ticks, along with the
  // number of ticks-per-second. Convert to the number of elapsed microseconds.
  elapsed_microseconds.QuadPart *= 1000000;
  elapsed_microseconds.QuadPart /= frequency.QuadPart;
  return elapsed_microseconds.QuadPart;
}

}  // namespace

int main() {
  example_etw_provider = &example::ExampleEtwProvider::GetProvider();
  if (example_etw_provider->enabled() == false) {
    printf("Enable the provider before running the tests");
    return -1;
  }

  printf("enabled   disabled\n");
  for (int i = 0; i < 20; ++i) {
    int64_t duration;

    // Constant seed to ensure the same work on each run
    srand(51);
    total_elements = 0;
    example_etw_provider->set_enabled(true);
    duration = SortArray();
    printf("%8d  ", static_cast<int>(duration));

    srand(51);
    total_elements = 0;
    example_etw_provider->set_enabled(false);
    duration = SortArray();
    printf("%8d\n", static_cast<int>(duration));
  }
  return 0;
}
