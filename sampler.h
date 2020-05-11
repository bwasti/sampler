#pragma once

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace detail {

class Spinlock {
 public:
  Spinlock() { lock_.clear(); }
  Spinlock(const Spinlock &) = delete;
  ~Spinlock() = default;

  void lock() {
    while (lock_.test_and_set(std::memory_order_acquire))
      ;
  }
  bool try_lock() { return !lock_.test_and_set(std::memory_order_acquire); }
  void unlock() { lock_.clear(std::memory_order_release); }

 private:
  std::atomic_flag lock_;
};

}  // namespace detail

struct Sampler {
  Sampler(std::vector<const void *> addrs, std::function<void()> fn)
      : addrs_(addrs),
        func_(fn),
        addr_counts_(addrs_.size()),
        t_(&Sampler::do_profile, this) {}

  void do_profile();
  void join();
  std::vector<size_t> query();

 private:
  std::vector<const void *> addrs_;
  std::function<void()> func_;
  std::vector<size_t> addr_counts_;
  std::thread t_;
  bool should_quit_ = false;
  detail::Spinlock lock_;
};
