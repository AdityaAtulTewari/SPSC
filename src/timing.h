//
// Created by Aditya Tewari on 1/20/21.
//

#ifndef MATMUL_TIMING_H
#define MATMUL_TIMING_H
#include <chrono>
using namespace std::chrono;

template <bool on>
struct Timing
{
  time_point<high_resolution_clock> f;
  time_point<high_resolution_clock> l;
  void inline s() { if (on) f = high_resolution_clock::now();}
  void inline e() { if (on) l = high_resolution_clock::now();}
  void inline p(const char* s) {if (on) std::cout << s << " took:\t" << duration_cast<duration<double>>(l - f).count() << std::endl; }
};


#endif //MATMUL_TIMING_H
