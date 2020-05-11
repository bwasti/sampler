#include "sampler.h"

#include <asm/unistd.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <iostream>
#include <mutex>

long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu,
                     int group_fd, unsigned long flags) {
  return syscall(SYS_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

struct __attribute__((packed)) sample {
  struct perf_event_header header;
  uint64_t ip;
};

void Sampler::do_profile() {
  pid_t child = 0;
  switch ((child = fork())) {
    case -1:
      std::cerr << "fork failed: %m\n";
      return;
    case 0:
      func_();
      return;
    default:
      break;
  }

  struct perf_event_attr pe;
  memset(&pe, 0, sizeof(struct perf_event_attr));
  // pe.type = PERF_TYPE_HARDWARE;
  // pe.config = PERF_COUNT_HW_INSTRUCTIONS;
  pe.type = PERF_TYPE_SOFTWARE;
  pe.config = PERF_COUNT_SW_TASK_CLOCK;
  pe.size = sizeof(struct perf_event_attr);
  pe.disabled = 1;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;
  pe.sample_type = PERF_SAMPLE_IP;
  pe.sample_period = 1;
  pe.mmap = 1;
  pe.task = 1;
  // pe.precise_ip = 2;

  int fd = -1;
  fd = perf_event_open(&pe, child, -1, -1, 0);
  if (fd == -1) {
    std::cerr << "error opening leader " << std::hex << pe.config << "\n";
    return;
  }

  size_t mmap_pages_count = 2048;
  size_t p_size = sysconf(_SC_PAGESIZE);
  size_t len = p_size * (1 + mmap_pages_count);
  struct perf_event_mmap_page *meta_page = (struct perf_event_mmap_page *)mmap(
      NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (meta_page == MAP_FAILED) {
    fprintf(stderr, "mmap failed: %m\n");
    return;
  }

  char *data_page = ((char *)meta_page) + p_size;
  size_t progress = 0;
  uint64_t last_head = 0;

  ioctl(fd, PERF_EVENT_IOC_REFRESH, 0);
  ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

  bool local_done = false;

  while (!local_done) {
    std::unique_lock<detail::Spinlock> lk(lock_);
    local_done |= should_quit_;
    while (meta_page->data_head == last_head)
      ;

    last_head = meta_page->data_head;
    while (progress < last_head) {
      struct sample *here = (struct sample *)(data_page + progress % p_size);
      switch (here->header.type) {
        case PERF_RECORD_SAMPLE: {
          if (here->header.size < sizeof(*here)) {
            fprintf(stderr, "size too small.\n");
          }
          const void *addr = (const void *)here->ip;
          // TODO binary search
          if (addrs_.size() && addr < addrs_.back() && addr >= addrs_.front()) {
            size_t index = 0;
            for (auto &addr_ : addrs_) {
              if (addr < addr_) {
                index--;
                break;
              }
              index++;
            }
            addr_counts_[index]++;
          }
          break;
        }
        case PERF_RECORD_THROTTLE:
        case PERF_RECORD_UNTHROTTLE:
        case PERF_RECORD_LOST:
          break;
        default:
          fprintf(stderr, "unknown perf event, bailing...\n");
          local_done = true;
          break;
      }
      progress += here->header.size;
    }
  }
  ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
  if (child) {
    kill(child, SIGKILL);
  }
}

void Sampler::join() {
  {
    std::unique_lock<detail::Spinlock> lk(lock_);
    should_quit_ = true;
  }
  t_.join();
}

std::vector<size_t> Sampler::query() {
  std::vector<size_t> out;
  {
    std::unique_lock<detail::Spinlock> lk(lock_);
    out.reserve(addr_counts_.size());
    out.assign(addr_counts_.begin(), addr_counts_.end());
  }
  return out;
}
