#include "sampler.h"
#include "xbyak_aarch64/xbyak_aarch64.h"

int main() {
  std::vector<const void*> addresses;
  std::vector<std::string> labels;
  Xbyak::CodeGenerator code;
  Xbyak::Label l1;
  code.L(l1);
  for (auto i = 16; i < 32; ++i) {
    code.fmov(Xbyak::SReg(i), code.wzr);
  }
  addresses.push_back(code.getCurr());
  labels.push_back("fmla v0 v1 v2\t\t");
  for (auto i = 0; i < 100; ++i) {
    code.fmla(code.v0.s4, code.v1.s4, code.v2.s4);
  }
  addresses.push_back(code.getCurr());
  labels.push_back("fmla v0 v0 v2\t\t");
  for (auto i = 0; i < 100; ++i) {
    code.fmla(code.v0.s4, code.v0.s4, code.v2.s4);
  }
  addresses.push_back(code.getCurr());
  labels.push_back("fmla v0 v2 v2\t\t");
  for (auto i = 0; i < 100; ++i) {
    code.fmla(code.v0.s4, code.v2.s4, code.v2.s4);
  }
  addresses.push_back(code.getCurr());
  labels.push_back("fmla pipelined v0 v1 v2\t");
  for (auto i = 0; i < 100; ++i) {
    auto vreg_off = (i * 3) % 13 + 16;
    code.fmla(
        Xbyak::VReg4S(vreg_off),
        Xbyak::VReg4S(vreg_off+1),
        Xbyak::VReg4S(vreg_off+2));
  }
  addresses.push_back(code.getCurr());
  labels.push_back("fmla pipelined v0 v0 v2\t");
  for (auto i = 0; i < 100; ++i) {
    auto vreg_off = (i * 3) % 13 + 16;
    code.fmla(
        Xbyak::VReg4S(vreg_off),
        Xbyak::VReg4S(vreg_off),
        Xbyak::VReg4S(vreg_off+2));
  }
  addresses.push_back(code.getCurr());
  labels.push_back("fmla pipelined v0 v2 v2\t");
  for (auto i = 0; i < 100; ++i) {
    auto vreg_off = (i * 3) % 13 + 16;
    code.fmla(
        Xbyak::VReg4S(vreg_off),
        Xbyak::VReg4S(vreg_off+2),
        Xbyak::VReg4S(vreg_off+2));
  }
  addresses.push_back(code.getCurr());
  code.b(l1);
  code.ret();
  void(*f)() = code.getCode<void(*)()>();
  //f();
  //return 0;
  Sampler s(addresses, [&](){f();});

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
    for (auto i = 0; i < address_map.size(); ++i) {
      // Last element is always 0
      if (i == address_map.size() - 1) {
        break;
      }
      auto label = labels[i];
      std::cout << label << "\t";
      auto addr_count = address_map[i];
      std::cout << addr_count * 99 / max << "\t";
      for (auto j = 0; j < (addr_count * 50) / max; ++j) {
        std::cout << "#";
      }
      std::cout << "\n";
    }
    std::cout << i << "\n";
  }

  // Destroy the sampler

  s.join();
}
