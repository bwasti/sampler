#include "sampler.h"
#include "xbyak/xbyak.h"

#include <chrono>

int main() {
  std::vector<const void*> addresses;

  // What's the relative speed of 20x inc/sub, 20x pipeline hazardous fma, 20x
  // ILP'd fma?

  Xbyak::CodeGenerator code(1024 * 16);
  Xbyak::Label l1;
  addresses.push_back(code.getCurr());
  code.L(l1);
  for (auto i = 0; i < 20; ++i) {
    code.inc(code.rax);
    code.sub(code.rax, 1);
  }
  addresses.push_back(code.getCurr());
  for (auto i = 0; i < 5; ++i) {
    code.vfmadd132pd(code.zmm0, code.zmm1, code.zmm2);
    code.vfmadd132pd(code.zmm0, code.zmm1, code.zmm2);
    code.vfmadd132pd(code.zmm0, code.zmm1, code.zmm2);
    code.vfmadd132pd(code.zmm0, code.zmm1, code.zmm2);
  }
  addresses.push_back(code.getCurr());
  for (auto i = 0; i < 5; ++i) {
    code.vfmadd132pd(code.zmm0, code.zmm1, code.zmm2);
    code.vfmadd132pd(code.zmm3, code.zmm4, code.zmm5);
    code.vfmadd132pd(code.zmm6, code.zmm7, code.zmm8);
    code.vfmadd132pd(code.zmm9, code.zmm10, code.zmm11);
  }
  addresses.push_back(code.getCurr());
  code.jmp(l1);
  code.ret();
  code.setProtectModeRE();
  int (*f)() = code.getCode<int (*)()>();

  // Now we can use the sampler

  Sampler s(addresses, [&]() { f(); });

  // Display sampling data live

  for (int i = 0; i < 1000; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "\033[2J";
    std::cout << "\033[H";
    std::vector<size_t> address_map = s.query();
    size_t max = 1;
    for (auto& addr_count : address_map) {
      if (addr_count > max) {
        max = addr_count;
      }
    }
    for (auto& addr_count : address_map) {
      // Last element is always 0
      if (&addr_count == &address_map.back()) {
        break;
      }
      std::cout << addr_count << "\t";
      for (auto j = 0; j < (addr_count * 100) / max; ++j) {
        std::cout << "#";
      }
      std::cout << "\n";
    }
  }

  // Destroy the sampler

  s.join();

  return 0;
}
