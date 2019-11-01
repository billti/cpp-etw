#include <stdio.h>
#include <cstdint>

#include "v8-etw.h"

v8::etw::V8EtwProvider* v8_provider = nullptr;
INT32 total_elements = 0;

void print_array(int* p_start, int count) {
	for (int i = 0; i < count; ++i) {
		printf("%d ", *(p_start + i));
	}
	printf("\n");
}

LONGLONG sort_array() {
	LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);

	v8_provider->Initialized();

	for (int i = 0; i < 10000; ++i) {
		// Returns a value between 0 and RAND_MAX (0x7fff i.e. 32767)
		int r = (rand() * 1000 / 32767) + 1000; // Between 1000 & 2000
		total_elements += r;

		// Allocate an array of ints and fill it with random numbers
		int* p_elems = new int[r];
		for (int j = 0; j < r; ++j) {
			*(p_elems + j) = rand();
		}

		v8_provider->StartSort(r);

		qsort(p_elems, r, sizeof(int), [](void const* a, void const* b) -> int {
			int _a = *(static_cast<const int*>(a));
			int _b = *(static_cast<const int*>(b));
			if (_a == _b) return 0;
			return _a > _b ? 1 : -1;
			});

		v8_provider->StopSort();

		delete[] p_elems;
	}

	v8_provider->Finished(total_elements);

	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	// We now have the elapsed number of ticks, along with the
	// number of ticks-per-second. Convert to the number of elapsed microseconds.
	ElapsedMicroseconds.QuadPart *= 1000000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
	return ElapsedMicroseconds.QuadPart;
}

int main(int argc, char** argv) {
	v8_provider = v8::etw::V8EtwProvider::GetProvider();
	if (v8_provider->is_enabled == false) {
		printf("Enable the provider before running the tests");
		return -1;
	}

	printf("enabled   disabled\n");
	for (int i = 0; i < 40; ++i) {
		LONGLONG count;

		// Constant seed to ensure the same work on each run
		srand(51);
		total_elements = 0;
		v8_provider->is_enabled = true;
		count = sort_array();
		printf("%8d  ", static_cast<int>(count));

		srand(51);
		total_elements = 0;
		v8_provider->is_enabled = false;
		count = sort_array();
		printf("%8d\n", static_cast<int>(count));
	}
	return 0;
}
