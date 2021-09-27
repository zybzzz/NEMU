#include <isa.h>

#ifndef __ICS_EXPORT
#ifdef CONFIG_MODE_USER
word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  Assert(NO == 0x8, "Unsupport exception = %ld", NO);
  uintptr_t host_syscall(uintptr_t id, uintptr_t arg1, uintptr_t arg2,
      uintptr_t arg3, uintptr_t arg4, uintptr_t arg5, uintptr_t arg6);
  cpu.gpr[10]._64 = host_syscall(cpu.gpr[17]._64, cpu.gpr[10]._64, cpu.gpr[11]._64,
      cpu.gpr[12]._64, cpu.gpr[13]._64, cpu.gpr[14]._64, cpu.gpr[15]._64);
  return epc + 4;
}
#else
#include "../local-include/intr.h"
#include "../local-include/csr.h"
#include <cpu/difftest.h>

void update_mmu_state();

#define INTR_BIT (1ULL << 63)
enum {
  IRQ_USIP, IRQ_SSIP, IRQ_HSIP, IRQ_MSIP,
  IRQ_UTIP, IRQ_STIP, IRQ_HTIP, IRQ_MTIP,
  IRQ_UEIP, IRQ_SEIP, IRQ_HEIP, IRQ_MEIP
};

word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  switch (NO) {
    case EX_II:
    case EX_IPF:
    case EX_LPF:
    case EX_SPF: difftest_skip_dut(1, 2); break;
  }

  word_t deleg = (NO & INTR_BIT ? mideleg->val : medeleg->val);
  bool delegS = ((deleg & (1 << (NO & 0xf))) != 0) && (cpu.mode < MODE_M);

  if (delegS) {
    scause->val = NO;
    sepc->val = epc;
    mstatus->spp = cpu.mode;
    mstatus->spie = mstatus->sie;
    mstatus->sie = 0;
    switch (NO) {
      case EX_IPF: case EX_LPF: case EX_SPF:
      case EX_LAM: case EX_SAM:
        break;
      default: stval->val = 0;
    }
    cpu.mode = MODE_S;
    update_mmu_state();
    return stvec->val;
  } else {
    mcause->val = NO;
    mepc->val = epc;
    mstatus->mpp = cpu.mode;
    mstatus->mpie = mstatus->mie;
    mstatus->mie = 0;
    switch (NO) {
      case EX_IPF: case EX_LPF: case EX_SPF:
      case EX_LAM: case EX_SAM:
        break;
      default: mtval->val = 0;
    }
    cpu.mode = MODE_M;
    update_mmu_state();
    return mtvec->val;
  }
}

word_t isa_query_intr() {
#ifdef CONFIG_PA
  if (cpu.INTR && mstatus->mie) {
    cpu.INTR = false;
    return 0x8000000000000007;
  }
  return INTR_EMPTY;
#else
  word_t intr_vec = mie->val & mip->val;
  if (!intr_vec) return INTR_EMPTY;

  const int priority [] = {
    IRQ_MEIP, IRQ_MSIP, IRQ_MTIP,
    IRQ_SEIP, IRQ_SSIP, IRQ_STIP,
    IRQ_UEIP, IRQ_USIP, IRQ_UTIP
  };
  int i;
  for (i = 0; i < 9; i ++) {
    int irq = priority[i];
    if (intr_vec & (1 << irq)) {
      bool deleg = (mideleg->val & (1 << irq)) != 0;
      bool global_enable = (deleg ? ((cpu.mode == MODE_S) && mstatus->sie) || (cpu.mode < MODE_S) :
          ((cpu.mode == MODE_M) && mstatus->mie) || (cpu.mode < MODE_M));
      if (global_enable) return irq | INTR_BIT;
    }
  }
  return INTR_EMPTY;
#endif
#endif
}
#else
word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  /* TODO: Trigger an interrupt/exception with ``NO''.
   * Then return the address of the interrupt/exception vector.
   */

  return 0;
}

word_t isa_query_intr() {
  return INTR_EMPTY;
}
#endif
