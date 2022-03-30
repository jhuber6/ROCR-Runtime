////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
// 
// Copyright (c) 2014-2020, Advanced Micro Devices, Inc. All rights reserved.
// 
// Developed by:
// 
//                 AMD Research and AMD HSA Software Development
// 
//                 Advanced Micro Devices, Inc.
// 
//                 www.amd.com
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// 
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include "stdio.h"

#include "core/util/timer.h"

namespace rocr {
namespace timer {

accurate_clock::init::init() {
  freq = os::AccurateClockFrequency();
  accurate_clock::period_ns = 1e9 / double(freq);
}

struct sample {
  fast_clock::raw_rep r1, r2;
  accurate_clock::time_point t0, t1, t2, t3;
  fast_clock::raw_rep min;
  accurate_clock::duration elapsed;
  bool shortest, first, second;
};
static sample samples[100] = {0};
static int iteration = 0;
static FILE* file = nullptr;

void PrintClockSamples() {
  for (auto sample : samples) {
    fprintf(file,
            "type=%d r1=%ld r2=%ld dt1=%ld dt2=%ld dt3=%ld dr=%ld elapsed=%ld min=%ld short=%d "
            "first=%d second=%d\n",
            fast_clock::type(), sample.r1, sample.r2,
            std::chrono::duration_cast<std::chrono::nanoseconds>(sample.t1 - sample.t0).count(),
            std::chrono::duration_cast<std::chrono::nanoseconds>(sample.t2 - sample.t0).count(),
            std::chrono::duration_cast<std::chrono::nanoseconds>(sample.t3 - sample.t0).count(),
            sample.r2 - sample.r1,
            std::chrono::duration_cast<std::chrono::nanoseconds>(sample.elapsed).count(),
            sample.min, sample.shortest, sample.first, sample.second);
  }
}

// Calibrates the fast clock using the accurate clock.
fast_clock::init::init() {
  typedef accurate_clock clock;
  clock::duration delay(std::chrono::milliseconds(1));

  uint64_t pid = getpid();
  char buff[17];
  snprintf(buff, 17, "%ld", pid);
  buff[16] = '\0';
  std::string name = std::string("rocr_log_") + buff + ".txt";
  if (file != nullptr) fclose(file);
  file = fopen(name.c_str(), "w");
  assert(file != nullptr);

  // calibrate clock
  fast_clock::raw_rep min = 0;
  clock::duration elapsed = clock::duration::max();

  do {
    for (int t = 0; t < 10; t++) {
      fast_clock::raw_rep r1, r2;
      clock::time_point t0, t1, t2, t3;

      t0 = clock::now();
      std::atomic_signal_fence(std::memory_order_acq_rel);
      r1 = fast_clock::raw_now();
      std::atomic_signal_fence(std::memory_order_acq_rel);
      t1 = clock::now();
      std::atomic_signal_fence(std::memory_order_acq_rel);

      do {
        t2 = clock::now();
      } while (t2 - t1 < delay);

      std::atomic_signal_fence(std::memory_order_acq_rel);
      r2 = fast_clock::raw_now();
      std::atomic_signal_fence(std::memory_order_acq_rel);
      t3 = clock::now();

      // If elapsed time is shorter than last recorded time and both the start
      // and end times are confirmed correlated then record the clock readings.
      // This protects against inaccuracy due to thread switching
      if ((t3 - t1 < elapsed) && ((t1 - t0) * 10 < (t2 - t1)) &&
          ((t3 - t2) * 10 < (t2 - t1))) {
        elapsed = t3 - t1;
        min = r2 - r1;
      }

      samples[iteration].r1 = r1;
      samples[iteration].r2 = r2;
      samples[iteration].t0 = t0;
      samples[iteration].t1 = t1;
      samples[iteration].t2 = t2;
      samples[iteration].t3 = t3;
      samples[iteration].elapsed = elapsed;
      samples[iteration].min = min;
      samples[iteration].shortest = ((t3 - t1) == elapsed);
      samples[iteration].first = ((t1 - t0) * 10 < (t2 - t1));
      samples[iteration].second = ((t3 - t2) * 10 < (t2 - t1));
      iteration++;
    }
    delay += delay;

    if (iteration == 100) {
      PrintClockSamples();
      fprintf(file, "Hang detected\n");
      fclose(file);
      abort();
      iteration = 0;
    }
  } while (min < 1000);

  PrintClockSamples();
  fclose(file);
  file = nullptr;

  fast_clock::freq = double(min) / duration_in_seconds(elapsed);
  fast_clock::period_ps = 1e12 / fast_clock::freq;
  // printf("Timer setup took %f ms\n", duration_in_seconds(elapsed)*1000.0f);
  // printf("Fast clock frequency: %f MHz\n", double(fast_clock::freq)/1e6);
}

double accurate_clock::period_ns;
accurate_clock::raw_frequency accurate_clock::freq;
accurate_clock::init accurate_clock::accurate_clock_init;

double fast_clock::period_ps;
fast_clock::raw_frequency fast_clock::freq;
fast_clock::init fast_clock::fast_clock_init;
}   //  namespace timer
}   //  namespace rocr
