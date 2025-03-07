/*
$info$
tags: backend|arm64
$end_info$
*/

#include <syscall.h>
#include "Interface/Core/JIT/Arm64/JITClass.h"
#include "FEXCore/Debug/InternalThreadState.h"

namespace FEXCore::CPU {
using namespace vixl;
using namespace vixl::aarch64;
#define DEF_OP(x) void Arm64JITCore::Op_##x(IR::IROp_Header const *IROp, IR::NodeID Node)

DEF_OP(GuestOpcode) {
  auto Op = IROp->C<IR::IROp_GuestOpcode>();
  // metadata
  DebugData->GuestOpcodes.push_back({Op->GuestEntryOffset, GetCursorAddress<uint8_t*>() - GuestEntry});
}

DEF_OP(Fence) {
  auto Op = IROp->C<IR::IROp_Fence>();
  switch (Op->Fence) {
    case IR::Fence_Load.Val:
      dmb(FullSystem, BarrierReads);
      break;
    case IR::Fence_LoadStore.Val:
      dmb(FullSystem, BarrierAll);
      break;
    case IR::Fence_Store.Val:
      dmb(FullSystem, BarrierWrites);
      break;
    default: LOGMAN_MSG_A_FMT("Unknown Fence: {}", Op->Fence); break;
  }
}

DEF_OP(Break) {
  auto Op = IROp->C<IR::IROp_Break>();

  // First we must reset the stack
  ResetStack();

  Core::CpuStateFrame::SynchronousFaultDataStruct State = {
    .FaultToTopAndGeneratedException = 1,
    .Signal = Op->Reason.Signal,
    .TrapNo = Op->Reason.TrapNumber,
    .si_code = Op->Reason.si_code,
    .err_code = Op->Reason.ErrorRegister,
  };

  uint64_t Constant{};
  memcpy(&Constant, &State, sizeof(State));

  LoadConstant(x1, Constant);
  str(x1, MemOperand(STATE, offsetof(FEXCore::Core::CpuStateFrame, SynchronousFaultData)));

  switch (Op->Reason.Signal) {
  case SIGILL:
    ldr(TMP1, MemOperand(STATE, offsetof(FEXCore::Core::CpuStateFrame, Pointers.Common.GuestSignal_SIGILL)));
    br(TMP1);
    break;
  case SIGTRAP:
    ldr(TMP1, MemOperand(STATE, offsetof(FEXCore::Core::CpuStateFrame, Pointers.Common.GuestSignal_SIGTRAP)));
    br(TMP1);
    break;
  case SIGSEGV:
    ldr(TMP1, MemOperand(STATE, offsetof(FEXCore::Core::CpuStateFrame, Pointers.Common.GuestSignal_SIGSEGV)));
    br(TMP1);
    break;
  default:
    ldr(TMP1, MemOperand(STATE, offsetof(FEXCore::Core::CpuStateFrame, Pointers.Common.GuestSignal_SIGTRAP)));
    br(TMP1);
    break;
  }
}

DEF_OP(GetRoundingMode) {
  auto Dst = GetReg<RA_64>(Node);
  mrs(Dst, FPCR);
  lsr(Dst, Dst,  22);

  // FTZ is already in the correct location
  // Rounding mode is different
  and_(TMP1, Dst, 0b11);

  cmp(TMP1, 1);
  LoadConstant(TMP3, IR::ROUND_MODE_POSITIVE_INFINITY);
  csel(TMP2, TMP3, xzr, vixl::aarch64::Condition::eq);

  cmp(TMP1, 2);
  LoadConstant(TMP3, IR::ROUND_MODE_NEGATIVE_INFINITY);
  csel(TMP2, TMP3, TMP2, vixl::aarch64::Condition::eq);

  cmp(TMP1, 3);
  LoadConstant(TMP3, IR::ROUND_MODE_TOWARDS_ZERO);
  csel(TMP2, TMP3, TMP2, vixl::aarch64::Condition::eq);

  orr(Dst, Dst, TMP2);

  bfi(Dst, TMP2, 0, 2);
}

DEF_OP(SetRoundingMode) {
  auto Op = IROp->C<IR::IROp_SetRoundingMode>();
  auto Src = GetReg<RA_64>(Op->RoundMode.ID());

  // Setup the rounding flags correctly
  and_(TMP1, Src, 0b11);

  cmp(TMP1, IR::ROUND_MODE_POSITIVE_INFINITY);
  LoadConstant(TMP3, 1);
  csel(TMP2, TMP3, xzr, vixl::aarch64::Condition::eq);

  cmp(TMP1, IR::ROUND_MODE_NEGATIVE_INFINITY);
  LoadConstant(TMP3, 2);
  csel(TMP2, TMP3, TMP2, vixl::aarch64::Condition::eq);

  cmp(TMP1, IR::ROUND_MODE_TOWARDS_ZERO);
  LoadConstant(TMP3, 3);
  csel(TMP2, TMP3, TMP2, vixl::aarch64::Condition::eq);

  mrs(TMP1, FPCR);

  // vixl simulator doesn't support anything beyond ties-to-even rounding
#ifndef VIXL_SIMULATOR
  // Insert the rounding flags
  bfi(TMP1, TMP2, 22, 2);
#endif

  // Insert the FTZ flag
  lsr(TMP2, Src, 2);
  bfi(TMP1, TMP2, 24, 1);

  // Now save the new FPCR
  msr(FPCR, TMP1);
}

DEF_OP(Print) {
  auto Op = IROp->C<IR::IROp_Print>();

  PushDynamicRegsAndLR(TMP1);
  SpillStaticRegs();

  if (IsGPR(Op->Value.ID())) {
    mov(x0, GetReg<RA_64>(Op->Value.ID()));
    ldr(x3, MemOperand(STATE, offsetof(FEXCore::Core::CpuStateFrame, Pointers.Common.PrintValue)));
  }
  else {
    fmov(x0, GetVReg(Op->Value.ID()).V1D());
    // Bug in vixl that source vector needs to b V1D rather than V2D?
    fmov(x1, GetVReg(Op->Value.ID()).V1D(), 1);
    ldr(x3, MemOperand(STATE, offsetof(FEXCore::Core::CpuStateFrame, Pointers.Common.PrintVectorValue)));
  }

  blr(x3);

  FillStaticRegs();
  PopDynamicRegsAndLR();
}

DEF_OP(ProcessorID) {
  // We always need to spill x8 since we can't know if it is live at this SSA location
  uint32_t SpillMask = 1U << 8;

  // Ordering is incredibly important here
  // We must spill any overlapping registers first THEN claim we are in a syscall without invalidating state at all
  // Only spill the registers that intersect with our usage
  SpillStaticRegs(false, SpillMask);

  // Now that we are spilled, store in the state that we are in a syscall
  // Still without overwriting registers that matter
  // 16bit LoadConstant to be a single instruction
  // We must always spill at least one register (x8) so this value always has a bit set
  // This gives the signal handler a value to check to see if we are in a syscall at all
  LoadConstant(x0, SpillMask & 0xFFFF);
  str(x0, MemOperand(STATE, offsetof(FEXCore::Core::CpuStateFrame, InSyscallInfo)));

  // Allocate some temporary space for storing the uint32_t CPU and Node IDs
  sub(sp, sp, 16);

  // Load the getcpu syscall number
  LoadConstant(x8, SYS_getcpu);

  // CPU pointer in x0
  add(x0, sp, 0);
  // Node in x1
  add(x1, sp, 4);

  svc(0);
  // On updated signal mask we can receive a signal RIGHT HERE

  // Load the values returned by the kernel
  ldp(w0, w1, MemOperand(sp));
  // Deallocate stack space
  sub(sp, sp, 16);

  // Now that we are done in the syscall we need to carefully peel back the state
  // First unspill the registers from before
  FillStaticRegs(false, SpillMask);

  // Now the registers we've spilled are back in their original host registers
  // We can safely claim we are no longer in a syscall
  str(xzr, MemOperand(STATE, offsetof(FEXCore::Core::CpuStateFrame, InSyscallInfo)));


  // Now store the result in the destination in the expected format
  // uint32_t Res = (node << 12) | cpu;
  // CPU is in w0
  // Node is in w1
  orr(GetReg<RA_64>(Node), x0, Operand(x1, LSL, 12));
}

DEF_OP(RDRAND) {
  auto Op = IROp->C<IR::IROp_RDRAND>();

  // Results are in x0, x1
  // Results want to be in a i64v2 vector
  auto Dst = GetRegPair<RA_64>(Node);

  if (Op->GetReseeded) {
    mrs(Dst.first, RNDRRS);
  }
  else {
    mrs(Dst.first, RNDR);
  }

  // If the rng number is valid then NZCV is 0b0000, otherwise NZCV is 0b0100
  cset(Dst.second, Condition::ne);
}

DEF_OP(Yield) {
  hint(SystemHint::YIELD);
}

#undef DEF_OP
void Arm64JITCore::RegisterMiscHandlers() {
#define REGISTER_OP(op, x) OpHandlers[FEXCore::IR::IROps::OP_##op] = &Arm64JITCore::Op_##x
  REGISTER_OP(DUMMY,      NoOp);
  REGISTER_OP(IRHEADER,   NoOp);
  REGISTER_OP(CODEBLOCK,  NoOp);
  REGISTER_OP(BEGINBLOCK, NoOp);
  REGISTER_OP(ENDBLOCK,   NoOp);
  REGISTER_OP(GUESTOPCODE, GuestOpcode);
  REGISTER_OP(FENCE,      Fence);
  REGISTER_OP(BREAK,      Break);
  REGISTER_OP(PHI,        NoOp);
  REGISTER_OP(PHIVALUE,   NoOp);
  REGISTER_OP(PRINT,      Print);
  REGISTER_OP(GETROUNDINGMODE, GetRoundingMode);
  REGISTER_OP(SETROUNDINGMODE, SetRoundingMode);
  REGISTER_OP(INVALIDATEFLAGS,   NoOp);
  REGISTER_OP(PROCESSORID,   ProcessorID);
  REGISTER_OP(RDRAND, RDRAND);
  REGISTER_OP(YIELD, Yield);

#undef REGISTER_OP
}
}

