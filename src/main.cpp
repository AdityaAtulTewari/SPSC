#include <iostream>
#include <sstream>

#define ARGS 1
#ifndef L1D_CACHE_LINE_SIZE
#define L1D_CACHE_LINE_SIZE 64
#endif

const unsigned cores = 2;
const unsigned PAGE_SIZE = 4096;

#include "spsc.h"
#include <getopt.h>

void usage(char* arg0)
{
  std::cerr << "Usage:\t" << arg0 << "[-v -d -s -c] data" << std::endl;
}

enum QUEUE_TYPE
{
 PIPESQ,
 SPSCRB,
 VTLINK,
};

void parse_args(int argc, char** argv,
    bool* check, unsigned* n,
    QUEUE_TYPE* at)
{
  int opt;
  char* arg0 = argv[0];
  auto us = [arg0] () {usage(arg0);};
  int helper;
  while((opt = getopt(argc, argv, "sdvc")) != -1)
  {
    std::ostringstream num_hwpar;
    switch(opt)
    {
      case 'v' :
        *at = VTLINK;
        break;
      case 'd' :
        *at = SPSCRB;
        break;
      case 's' :
        *at = PIPESQ;
        break;
      case 'c' :
        *check = true;
    }
  }
  std::istringstream sarr[ARGS];
  unsigned darr[ARGS];
  unsigned i;
  for(i = 0; i < ARGS; i++)
  {
    if(optind + i == argc)
    {
      std::cerr << "You have too few unsigned int arguments: " << i << " out of " << ARGS << std::endl;
      us();
      exit(-3);
    }
    sarr[i] = std::istringstream(argv[optind + i]);
    if(!(sarr[i] >> darr[i]))
    {
      std::cerr << "Your argument at " << optind + i << " was malformed." << std::endl;
      std::cerr << "It should have been an unsigned int" << std::endl;
      us();
      exit(-2);
    }
  }
  if(i + optind != argc)
  {
    std::cerr << "You have too many arguments." << std::endl;
    us();
    exit(-1);
  }

  *n = darr[0] + 1;
}

int main(int argc, char** argv)
{
  QUEUE_TYPE at = PIPESQ;
  unsigned n;
  bool check = false;
  parse_args(argc, argv, &check, &n, &at);

  cpu_set_t mask[cores];
  for(unsigned i = 0; i < cores; i++)
  {
    CPU_ZERO(&mask[i]);
    CPU_SET(i,&mask[i]);
  }

  uint64_t* srcbuf = (uint64_t*) aligned_alloc(PAGE_SIZE, n*sizeof(uint64_t));
  uint64_t* dstbuf = (uint64_t*) aligned_alloc(PAGE_SIZE, n*sizeof(uint64_t));
  //Forces both buffers to have pages accessible reduces probability of faults
  for(unsigned i = 0; i < n; i++) {srcbuf[i] = rando();}

  if(at == PIPESQ) pipesq_run(srcbuf,dstbuf,n,mask);
  else if(at == SPSCRB) spscrb_run(srcbuf,dstbuf,n,mask);
  else
  {
    std::cerr << "Invalid Buffer type" << std::endl;
  }
  if(check)
  {
    for(unsigned i = 0; i < n; i++)
      if(srcbuf[i] != dstbuf[i]) {std::cerr << "THEY ARE NOT THE SAME" << std::endl; exit(-6);}
  }
  free(srcbuf);
  free(dstbuf);
  return 0;
}
