#ifndef SPSC_H
#define SPSC_H

#include <thread>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <atomic>
#include <functional>
#include "timing.h"

unsigned x = 1, y = 4, z = 7, w = 13;
unsigned rando()
{
  unsigned t = x;
  t ^= t << 11;
  t ^= t >> 8;
  x = y;
  y = z;
  z = w;
  w ^= w >> 19;
  w ^= t;
  return w;
}

std::atomic<unsigned> bar = 0;
Timing<true> t;
pthread_t t1;
pthread_attr_t attr1;

#ifndef NOGEM5
#include <gem5/m5ops.h>
#else
inline void m5_reset_stats(unsigned a, unsigned b){}
inline void m5_dump_reset_stats(unsigned a, unsigned b){}
#endif

void* starter(void* f)
{
  auto run = *((std::function<void*()>*) f);
  return run();
}

void pipesq_run(uint64_t* srcbuf, uint64_t* dstbuf, unsigned n, cpu_set_t mask[2])
{
  int pipefd[2];
  int rc = pipe(pipefd);
  if(rc != 0) {std::cerr << "Failed to create pipes" << std::endl; exit(-4);}

  std::function<void*()> recv = [dstbuf,&pipefd,n]()
  {
    for(unsigned i = 0; i < n; i++) dstbuf[i] = rando();
    bar++;
    while(bar != 2);
    for(unsigned i = 0; i < n;) i += (unsigned) read(pipefd[0], &dstbuf[i], sizeof(uint64_t))/sizeof(uint64_t);
    m5_dump_reset_stats(0,0);
    t.e();
    return nullptr;
  };
  auto send = [srcbuf,&pipefd,n]()
  {
    while(bar != 1);
    t.s();
    m5_reset_stats(0,0);
    bar++;
    while(bar != 2);
    for(unsigned i = 0; i < n;) i += (unsigned) write(pipefd[1], &srcbuf[i], sizeof(uint64_t))/sizeof(uint64_t);
  };

  pthread_attr_setaffinity_np(&attr1, sizeof(mask[1]), &mask[1]);
  pthread_setaffinity_np(pthread_self(), sizeof(mask[0]), &mask[0]);
  pthread_create(&t1, &attr1, starter, (void*)&recv);
  send();
  pthread_join(t1, nullptr);
  t.p("PIPESQ");
  close(pipefd[0]);
  close(pipefd[1]);
}

struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) align_u64
{
  volatile uint64_t val;
  align_u64(uint64_t v) : val(v) {}
};

const unsigned QS = PAGE_SIZE/sizeof(uint64_t);
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) SPSC
{
  align_u64 head;
  align_u64 tail;
  align_u64 pads;
  volatile uint64_t buf[QS];

  SPSC() : head(0), tail(0), pads(0) {}

  unsigned push(uint64_t v)
  {
    uint64_t h = head.val + 1;
    uint64_t t = tail.val;
    if(h % QS == t % QS) return 0;
    buf[h % QS] = v;
    __atomic_store_8(&head.val, h, __ATOMIC_RELEASE);
    //head.val = h;
    return 1;
  }

  unsigned pop(uint64_t& v)
  {
    uint64_t h = head.val;
    uint64_t t = __atomic_load_8(&tail.val, __ATOMIC_ACQUIRE) + 1;
    if(h % QS == (t - 1) % QS) return 0;
    v = buf[t % QS];
    tail = t;
    return 1;
  }
};

void spscrb_run(uint64_t* srcbuf, uint64_t* dstbuf, unsigned n, cpu_set_t mask[2])
{
  SPSC* q = new SPSC();
  std::function<void*()> recv = [dstbuf,q,n]()
  {
    for(unsigned i = 0; i < n; i++) dstbuf[i] = rando();
    bar++;
    while(bar != 2);
    for(unsigned i = 0; i < n;) i += q->pop(*(dstbuf+i));
    m5_dump_reset_stats(0,0);
    t.e();
    return nullptr;
  };
  auto send = [srcbuf,&q,n]()
  {
    while(bar != 1);
    t.s();
    m5_reset_stats(0,0);
    bar++;
    while(bar != 2);
    for(unsigned i = 0; i < n;) i += q->push(srcbuf[i]);
  };

  pthread_attr_setaffinity_np(&attr1, sizeof(mask[1]), &mask[1]);
  pthread_setaffinity_np(pthread_self(), sizeof(mask[0]), &mask[0]);
  pthread_create(&t1, &attr1, starter, (void*)&recv);
  send();
  pthread_join(t1, nullptr);
  t.p("SPSCRQ");
  delete q;
}

#include "vl/vl.h"
#include <cassert>
struct VPush
{
  vlendpt_t end;
  VPush(int fd)
  {
    open_twin_vl_as_producer(fd, &end, 1);
  }
  inline void push(uint64_t c)
  {
    twin_vl_push_strong(&end, c);
  }
  ~VPush()
  {
    twin_vl_flush(&end);
    close_twin_vl_as_producer(end);
  }
};

struct VPop
{
  vlendpt_t end;
  VPop(int fd)
  {
      int err = open_twin_vl_as_consumer(fd, &end, 1);
      assert(!err);
    }
  inline uint64_t pop()
  {
      uint64_t blah;
      twin_vl_pop_strong(&end, &blah);
      return blah;
    }
  ~VPop()
  {
      close_twin_vl_as_consumer(end);
    }
};

void vtlink_run(uint64_t* srcbuf, uint64_t* dstbuf, unsigned n, cpu_set_t mask[2])
{
  int fd = mkvl();
  if(fd < 0) {std::cerr << "Unable to initialize virtualLink queue" << std::endl; exit(-5);}
  std::function<void*()> recv = [dstbuf,n,fd]()
  {
    VPop pop(fd);
    for(unsigned i = 0; i < n; i++) dstbuf[i] = rando();
    bar++;
    while(bar != 2);
    for(unsigned i = 0; i < n; i++) dstbuf[i] = pop.pop();
    m5_dump_reset_stats(0,0);
    t.e();
    return nullptr;
  };
  auto send = [srcbuf,n,fd]()
  {
    VPush push(fd);
    while(bar != 1);
    t.s();
    m5_reset_stats(0,0);
    bar++;
    while(bar != 2);
    for(unsigned i = 0; i < n; i++) push.push(srcbuf[i]);
  };
  pthread_attr_setaffinity_np(&attr1, sizeof(mask[1]), &mask[1]);
  pthread_setaffinity_np(pthread_self(), sizeof(mask[0]), &mask[0]);
  pthread_create(&t1, &attr1, starter, (void*)&recv);
  send();
  pthread_join(t1, nullptr);
  t.p("VTLINK");
}

#endif //SPSC_H
