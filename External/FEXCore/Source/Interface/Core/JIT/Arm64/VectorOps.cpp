/*
$info$
tags: backend|arm64
$end_info$
*/

#include "Interface/Core/JIT/Arm64/JITClass.h"

namespace FEXCore::CPU {

using namespace vixl;
using namespace vixl::aarch64;
#define DEF_OP(x) void Arm64JITCore::Op_##x(IR::IROp_Header const *IROp, IR::NodeID Node)
DEF_OP(VectorZero) {
  if (HostSupportsSVE) {
    const auto Dst = GetVReg(Node).Z().VnD();
    eor(Dst, Dst, Dst);
  } else {
    const uint8_t OpSize = IROp->Size;

    switch (OpSize) {
      case 8: {
        const auto Dst = GetVReg(Node).V8B();
        eor(Dst, Dst, Dst);
        break;
      }
      case 16: {
        const auto Dst = GetVReg(Node).V16B();
        eor(Dst, Dst, Dst);
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Op Size: {}", OpSize);
        break;
    }
  }
}

DEF_OP(VectorImm) {
  auto Op = IROp->C<IR::IROp_VectorImm>();

  const uint8_t OpSize = IROp->Size;
  const uint8_t ElementSize = Op->Header.ElementSize;
  const uint8_t Elements = OpSize / ElementSize;

  if (HostSupportsSVE) {
    const auto Dst = [&] {
      const auto Tmp = GetVReg(Node).Z();
      switch (ElementSize) {
      case 1:
        return Tmp.VnB();
      case 2:
        return Tmp.VnH();
      case 4:
        return Tmp.VnS();
      case 8:
        return Tmp.VnD();
      default:
        LOGMAN_MSG_A_FMT("Unhandled element size: {}", ElementSize);
        return Tmp;
      }
    }();

    if (ElementSize > 1 && (Op->Immediate & 0x80)) {
      // SVE dup uses sign extension where VectorImm wants zext
      LoadConstant(TMP1.X(), Op->Immediate);
      dup(Dst, TMP1.X());
    }
    else {
      dup(Dst, static_cast<int8_t>(Op->Immediate));
    }
  } else {
    if (ElementSize == 8) {
      // movi with 64bit element size doesn't do what we want here
      LoadConstant(TMP1.X(), Op->Immediate);
      dup(GetVReg(Node).V2D(), TMP1.X());
    }
    else {
      movi(GetVReg(Node).VCast(OpSize * 8, Elements), Op->Immediate);
    }
  }
}

DEF_OP(VMov) {
  const auto Op = IROp->C<IR::IROp_VMov>();
  const auto OpSize = IROp->Size;

  const auto Dst = GetVReg(Node);
  const auto Source = GetVReg(Op->Source.ID());

  switch (OpSize) {
    case 1: {
      eor(VTMP1.V16B(), VTMP1.V16B(), VTMP1.V16B());
      mov(VTMP1.V16B(), 0, Source.V16B(), 0);
      mov(Dst, VTMP1);
      break;
    }
    case 2: {
      eor(VTMP1.V16B(), VTMP1.V16B(), VTMP1.V16B());
      mov(VTMP1.V8H(), 0, Source.V8H(), 0);
      mov(Dst, VTMP1);
      break;
    }
    case 4: {
      eor(VTMP1.V16B(), VTMP1.V16B(), VTMP1.V16B());
      mov(VTMP1.V4S(), 0, Source.V4S(), 0);
      mov(Dst, VTMP1);
      break;
    }
    case 8: {
      mov(Dst.V8B(), Source.V8B());
      break;
    }
    case 16: {
      if (HostSupportsSVE) {
        mov(Dst.V16B(), Source.V16B());
      } else {
        if (Dst.GetCode() != Source.GetCode()) {
          mov(Dst.V16B(), Source.V16B());
        }
      }
      break;
    }
    case 32: {
      // NOTE: If, in the distant future we support larger moves, or registers
      //       (*cough* AVX-512 *cough*) make sure to change this to treat
      //       256-bit moves with zero extending behavior instead of doing only
      //       a regular SVE move into a 512-bit register.
      if (Dst.GetCode() != Source.GetCode()) {
        mov(Dst.Z().VnD(), Source.Z().VnD());
      }
      break;
    }
    default:
      LOGMAN_MSG_A_FMT("Unknown Op Size: {}", OpSize);
      break;
  }
}

DEF_OP(VAnd) {
  auto Op = IROp->C<IR::IROp_VAnd>();

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE) {
    and_(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
  } else {
    and_(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
  }
}

DEF_OP(VBic) {
  auto Op = IROp->C<IR::IROp_VBic>();

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE) {
    bic(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
  } else {
    bic(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
  }
}

DEF_OP(VOr) {
  auto Op = IROp->C<IR::IROp_VOr>();

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE) {
    orr(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
  } else {
    orr(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
  }
}

DEF_OP(VXor) {
  auto Op = IROp->C<IR::IROp_VXor>();

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE) {
    eor(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
  } else {
    eor(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
  }
}

DEF_OP(VAdd) {
  auto Op = IROp->C<IR::IROp_VAdd>();

  const auto ElementSize = Op->Header.ElementSize;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  switch (ElementSize) {
    case 1: {
      if (HostSupportsSVE) {
        add(Dst.Z().VnB(), Vector1.Z().VnB(), Vector2.Z().VnB());
      } else {
        add(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
      }
      break;
    }
    case 2: {
      if (HostSupportsSVE) {
        add(Dst.Z().VnH(), Vector1.Z().VnH(), Vector2.Z().VnH());
      } else {
        add(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
      }
      break;
    }
    case 4: {
      if (HostSupportsSVE) {
        add(Dst.Z().VnS(), Vector1.Z().VnS(), Vector2.Z().VnS());
      } else {
        add(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
      }
      break;
    }
    case 8: {
      if (HostSupportsSVE) {
        add(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
      } else {
        add(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
      }
      break;
    }
    default:
      LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
      break;
  }
}

DEF_OP(VSub) {
  auto Op = IROp->C<IR::IROp_VSub>();

  const auto ElementSize = Op->Header.ElementSize;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  switch (ElementSize) {
    case 1: {
      if (HostSupportsSVE) {
        sub(Dst.Z().VnB(), Vector1.Z().VnB(), Vector2.Z().VnB());
      } else {
        sub(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
      }
      break;
    }
    case 2: {
      if (HostSupportsSVE) {
        sub(Dst.Z().VnH(), Vector1.Z().VnH(), Vector2.Z().VnH());
      } else {
        sub(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
      }
      break;
    }
    case 4: {
      if (HostSupportsSVE) {
        sub(Dst.Z().VnS(), Vector1.Z().VnS(), Vector2.Z().VnS());
      } else {
        sub(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
      }
      break;
    }
    case 8: {
      if (HostSupportsSVE) {
        sub(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
      } else {
        sub(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
      }
      break;
    }
    default:
      LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
      break;
  }
}

DEF_OP(VUQAdd) {
  auto Op = IROp->C<IR::IROp_VUQAdd>();

  const auto ElementSize = Op->Header.ElementSize;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  switch (ElementSize) {
    case 1: {
      if (HostSupportsSVE) {
        uqadd(Dst.Z().VnB(), Vector1.Z().VnB(), Vector2.Z().VnB());
      } else {
        uqadd(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
      }
      break;
    }
    case 2: {
      if (HostSupportsSVE) {
        uqadd(Dst.Z().VnH(), Vector1.Z().VnH(), Vector2.Z().VnH());
      } else {
        uqadd(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
      }
      break;
    }
    case 4: {
      if (HostSupportsSVE) {
        uqadd(Dst.Z().VnS(), Vector1.Z().VnS(), Vector2.Z().VnS());
      } else {
        uqadd(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
      }
      break;
    }
    case 8: {
      if (HostSupportsSVE) {
        uqadd(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
      } else {
        uqadd(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
      }
      break;
    }
    default:
      LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
      break;
  }
}

DEF_OP(VUQSub) {
  auto Op = IROp->C<IR::IROp_VUQSub>();

  const auto ElementSize = Op->Header.ElementSize;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  switch (ElementSize) {
    case 1: {
      if (HostSupportsSVE) {
        uqsub(Dst.Z().VnB(), Vector1.Z().VnB(), Vector2.Z().VnB());
      } else {
        uqsub(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
      }
      break;
    }
    case 2: {
      if (HostSupportsSVE) {
        uqsub(Dst.Z().VnH(), Vector1.Z().VnH(), Vector2.Z().VnH());
      } else {
        uqsub(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
      }
      break;
    }
    case 4: {
      if (HostSupportsSVE) {
        uqsub(Dst.Z().VnS(), Vector1.Z().VnS(), Vector2.Z().VnS());
      } else {
        uqsub(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
      }
      break;
    }
    case 8: {
      if (HostSupportsSVE) {
        uqsub(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
      } else {
        uqsub(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
      }
      break;
    }
    default:
      LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
      break;
  }
}

DEF_OP(VSQAdd) {
  auto Op = IROp->C<IR::IROp_VSQAdd>();

  const auto ElementSize = Op->Header.ElementSize;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  switch (ElementSize) {
    case 1: {
      if (HostSupportsSVE) {
        sqadd(Dst.Z().VnB(), Vector1.Z().VnB(), Vector2.Z().VnB());
      } else {
        sqadd(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
      }
      break;
    }
    case 2: {
      if (HostSupportsSVE) {
        sqadd(Dst.Z().VnH(), Vector1.Z().VnH(), Vector2.Z().VnH());
      } else {
        sqadd(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
      }
      break;
    }
    case 4: {
      if (HostSupportsSVE) {
        sqadd(Dst.Z().VnS(), Vector1.Z().VnS(), Vector2.Z().VnS());
      } else {
        sqadd(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
      }
      break;
    }
    case 8: {
      if (HostSupportsSVE) {
        sqadd(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
      } else {
        sqadd(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
      }
      break;
    }
    default:
      LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
      break;
  }
}

DEF_OP(VSQSub) {
  auto Op = IROp->C<IR::IROp_VSQSub>();

  const auto ElementSize = Op->Header.ElementSize;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  switch (ElementSize) {
    case 1: {
      if (HostSupportsSVE) {
        sqsub(Dst.Z().VnB(), Vector1.Z().VnB(), Vector2.Z().VnB());
      } else {
        sqsub(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
      }
      break;
    }
    case 2: {
      if (HostSupportsSVE) {
        sqsub(Dst.Z().VnH(), Vector1.Z().VnH(), Vector2.Z().VnH());
      } else {
        sqsub(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
      }
      break;
    }
    case 4: {
      if (HostSupportsSVE) {
        sqsub(Dst.Z().VnS(), Vector1.Z().VnS(), Vector2.Z().VnS());
      } else {
        sqsub(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
      }
      break;
    }
    case 8: {
      if (HostSupportsSVE) {
        sqsub(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
      } else {
        sqsub(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
      }
      break;
    }
    default:
      LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
      break;
  }
}

DEF_OP(VAddP) {
  const auto Op = IROp->C<IR::IROp_VAddP>();
  const auto OpSize = IROp->Size;
  const auto IsScalar = OpSize == 8;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto ElementSize = Op->Header.ElementSize;

  const auto Dst = GetVReg(Node);
  const auto VectorLower = GetVReg(Op->VectorLower.ID());
  const auto VectorUpper = GetVReg(Op->VectorUpper.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Pred = PRED_TMP_32B.Merging();

    // SVE ADDP is a destructive operation, so we need a temporary
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());
    movprfx(VTMP1.Z().VnD(), VectorLower.Z().VnD());

    // Unlike Adv. SIMD's version of ADDP, which acts like it concats the
    // upper vector onto the end of the lower vector and then performs
    // pairwise addition, the SVE version actually interleaves the
    // results of the pairwise addition (gross!), so we need to undo that.
    switch (ElementSize) {
      case 1: {
        addp(VTMP1.Z().VnB(), Pred, VTMP1.Z().VnB(), VectorUpper.Z().VnB());
        uzp1(Dst.Z().VnB(), VTMP1.Z().VnB(), VTMP1.Z().VnB());
        uzp2(VTMP2.Z().VnB(), VTMP1.Z().VnB(), VTMP1.Z().VnB());
        break;
      }
      case 2: {
        addp(VTMP1.Z().VnH(), Pred, VTMP1.Z().VnH(), VectorUpper.Z().VnH());
        uzp1(Dst.Z().VnH(), VTMP1.Z().VnH(), VTMP1.Z().VnH());
        uzp2(VTMP2.Z().VnH(), VTMP1.Z().VnH(), VTMP1.Z().VnH());
        break;
      }
      case 4: {
        addp(VTMP1.Z().VnS(), Pred, VTMP1.Z().VnS(), VectorUpper.Z().VnS());
        uzp1(Dst.Z().VnS(), VTMP1.Z().VnS(), VTMP1.Z().VnS());
        uzp2(VTMP2.Z().VnS(), VTMP1.Z().VnS(), VTMP1.Z().VnS());
        break;
      }
      case 8: {
        addp(VTMP1.Z().VnD(), Pred, VTMP1.Z().VnD(), VectorUpper.Z().VnD());
        uzp1(Dst.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());
        uzp2(VTMP2.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    // Merge upper half with lower half.
    splice(Dst.Z().VnD(), PRED_TMP_16B, Dst.Z().VnD(), VTMP2.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 1: {
          addp(Dst.V8B(), VectorLower.V8B(), VectorUpper.V8B());
          break;
        }
        case 2: {
          addp(Dst.V4H(), VectorLower.V4H(), VectorUpper.V4H());
          break;
        }
        case 4: {
          addp(Dst.V2S(), VectorLower.V2S(), VectorUpper.V2S());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 1: {
          addp(Dst.V16B(), VectorLower.V16B(), VectorUpper.V16B());
          break;
        }
        case 2: {
          addp(Dst.V8H(), VectorLower.V8H(), VectorUpper.V8H());
          break;
        }
        case 4: {
          addp(Dst.V4S(), VectorLower.V4S(), VectorUpper.V4S());
          break;
        }
        case 8: {
          addp(Dst.V2D(), VectorLower.V2D(), VectorUpper.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VAddV) {
  const auto Op = IROp->C<IR::IROp_VAddV>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Elements = OpSize / ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    // SVE doesn't have an equivalent ADDV instruction, so we make do
    // by performing two Adv. SIMD ADDV operations on the high and low
    // 128-bit lanes and then sum them up.

    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto CompactPred = p0;

    // Select all our upper elements to run ADDV over them.
    not_(CompactPred.VnB(), Mask, PRED_TMP_16B.VnB());
    compact(VTMP1.Z().VnD(), CompactPred, Vector.Z().VnD());

    switch (ElementSize) {
      case 1:
        addv(VTMP2.B(), Vector.V16B());
        addv(VTMP1.B(), VTMP1.V16B());
        add(Dst.V16B(), VTMP1.V16B(), VTMP2.V16B());
        break;
      case 2:
        addv(VTMP2.H(), Vector.V8H());
        addv(VTMP1.H(), VTMP1.V8H());
        add(Dst.V8H(), VTMP1.V8H(), VTMP2.V8H());
        break;
      case 4:
        addv(VTMP2.S(), Vector.V4S());
        addv(VTMP1.S(), VTMP1.V4S());
        add(Dst.V4S(), VTMP1.V4S(), VTMP2.V4S());
        break;
      case 8:
        addp(VTMP2.D(), Vector.V2D());
        addp(VTMP1.D(), VTMP1.V2D());
        add(Dst.V2D(), VTMP1.V2D(), VTMP2.V2D());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    const auto OpSizeBits = OpSize * 8;
    const auto ElementSizeBits = ElementSize * 8;

    switch (ElementSize) {
      case 1:
      case 2:
      case 4:
        addv(Dst.VCast(ElementSizeBits, 1), Vector.VCast(OpSizeBits, Elements));
        break;
      case 8:
        addp(Dst.VCast(OpSizeBits, 1), Vector.VCast(OpSizeBits, Elements));
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VUMinV) {
  auto Op = IROp->C<IR::IROp_VUMinV>();

  const auto OpSize = IROp->Size;
  const auto ElementSize = Op->Header.ElementSize;
  const auto Elements = OpSize / ElementSize;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE) {
    LOGMAN_THROW_AA_FMT(OpSize == 16 || OpSize == 32,
                        "Unsupported vector length: {}", OpSize);

    const auto Pred = OpSize == 16 ? PRED_TMP_16B
                                   : PRED_TMP_32B;

    switch (ElementSize) {
      case 1:
        uminv(Dst.B(), Pred, Vector.Z().VnB());
        break;
      case 2:
        uminv(Dst.H(), Pred, Vector.Z().VnH());
        break;
      case 4:
        uminv(Dst.S(), Pred, Vector.Z().VnS());
        break;
      case 8:
        uminv(Dst.D(), Pred, Vector.Z().VnD());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }
  } else {
    // Vector
    switch (ElementSize) {
      case 1:
      case 2:
      case 4:
        uminv(Dst.VCast(ElementSize * 8, 1), Vector.VCast(OpSize * 8, Elements));
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VURAvg) {
  const auto Op = IROp->C<IR::IROp_VURAvg>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit) {
    const auto Mask = PRED_TMP_32B.Merging();

    // SVE URHADD is a destructive operation, so we need
    // a temporary for performing operations.
    movprfx(VTMP1.Z().VnD(), Vector1.Z().VnD());

    switch (ElementSize) {
      case 1: {
        urhadd(VTMP1.Z().VnB(), Mask,
               VTMP1.Z().VnB(), Vector2.Z().VnB());
        break;
      }
      case 2: {
        urhadd(VTMP1.Z().VnH(), Mask,
               VTMP1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        urhadd(VTMP1.Z().VnS(), Mask,
               VTMP1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        urhadd(VTMP1.Z().VnD(), Mask,
               VTMP1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    switch (ElementSize) {
      case 1: {
        urhadd(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
        break;
      }
      case 2: {
        urhadd(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
        break;
      }
      case 4: {
        urhadd(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VAbs) {
  const auto Op = IROp->C<IR::IROp_VAbs>();
  const auto OpSize = IROp->Size;

  const uint8_t ElementSize = Op->Header.ElementSize;
  const uint8_t Elements = OpSize / Op->Header.ElementSize;

  const auto Dst = GetVReg(Node);
  const auto Src = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && OpSize == 32) {
    switch (ElementSize) {
      case 1: {
        abs(Dst.Z().VnB(), PRED_TMP_32B.Merging(), Src.Z().VnB());
        break;
      }
      case 2: {
        abs(Dst.Z().VnH(), PRED_TMP_32B.Merging(), Src.Z().VnH());
        break;
      }
      case 4: {
        abs(Dst.Z().VnS(), PRED_TMP_32B.Merging(), Src.Z().VnS());
        break;
      }
      case 8: {
        abs(Dst.Z().VnD(), PRED_TMP_32B.Merging(), Src.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    if (ElementSize == OpSize) {
      // Scalar
      switch (ElementSize) {
        case 8: {
          abs(Dst.D(), Src.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      // Vector
      switch (ElementSize) {
        case 1:
        case 2:
        case 4:
        case 8:
          abs(Dst.VCast(OpSize * 8, Elements), Src.VCast(OpSize * 8, Elements));
          break;
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VPopcount) {
  const auto Op = IROp->C<IR::IROp_VPopcount>();
  const auto OpSize = IROp->Size;
  const bool IsScalar = OpSize == 8;

  const auto ElementSize = Op->Header.ElementSize;

  const auto Dst = GetVReg(Node);
  const auto Src = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && !IsScalar) {
    const auto Pred = OpSize == 16 ? PRED_TMP_16B.Merging()
                                   : PRED_TMP_32B.Merging();

    switch (ElementSize) {
      case 1:
        cnt(Dst.Z().VnB(), Pred, Src.Z().VnB());
        break;
      case 2:
        cnt(Dst.Z().VnH(), Pred, Src.Z().VnH());
        break;
      case 4:
        cnt(Dst.Z().VnS(), Pred, Src.Z().VnS());
        break;
      case 8:
        cnt(Dst.Z().VnD(), Pred, Src.Z().VnD());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    if (IsScalar) {
      // Scalar
      switch (ElementSize) {
        case 1: {
          cnt(Dst.V8B(), Src.V8B());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      // Vector
      switch (ElementSize) {
        case 1:
          cnt(Dst.V16B(), Src.V16B());
          break;
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFAdd) {
  const auto Op = IROp->C<IR::IROp_VFAdd>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && !IsScalar) {
    switch (ElementSize) {
      case 2: {
        fadd(Dst.Z().VnH(), Vector1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        fadd(Dst.Z().VnS(), Vector1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        fadd(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fadd(Dst.H(), Vector1.H(), Vector2.H());
          break;
        }
        case 4: {
          fadd(Dst.S(), Vector1.S(), Vector2.S());
          break;
        }
        case 8: {
          fadd(Dst.D(), Vector1.D(), Vector2.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fadd(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
          break;
        }
        case 4: {
          fadd(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
          break;
        }
        case 8: {
          fadd(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFAddP) {
  const auto Op = IROp->C<IR::IROp_VFAddP>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;

  const auto Dst = GetVReg(Node);
  const auto VectorLower = GetVReg(Op->VectorLower.ID());
  const auto VectorUpper = GetVReg(Op->VectorUpper.ID());

  const bool Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  if (HostSupportsSVE && Is256Bit) {
    const auto Pred = PRED_TMP_32B.Merging();

    // SVE FADDP is a destructive operation, so we need a temporary
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());
    movprfx(VTMP1.Z().VnD(), VectorLower.Z().VnD());

    // Unlike Adv. SIMD's version of FADDP, which acts like it concats the
    // upper vector onto the end of the lower vector and then performs
    // pairwise addition, the SVE version actually interleaves the
    // results of the pairwise addition (gross!), so we need to undo that.
    switch (ElementSize) {
      case 2: {
        faddp(VTMP1.Z().VnH(), Pred, VTMP1.Z().VnH(), VectorUpper.Z().VnH());
        uzp1(Dst.Z().VnH(), VTMP1.Z().VnH(), VTMP1.Z().VnH());
        uzp2(VTMP2.Z().VnH(), VTMP1.Z().VnH(), VTMP1.Z().VnH());
        break;
      }
      case 4: {
        faddp(VTMP1.Z().VnS(), Pred, VTMP1.Z().VnS(), VectorUpper.Z().VnS());
        uzp1(Dst.Z().VnS(), VTMP1.Z().VnS(), VTMP1.Z().VnS());
        uzp2(VTMP2.Z().VnS(), VTMP1.Z().VnS(), VTMP1.Z().VnS());
        break;
      }
      case 8: {
        faddp(VTMP1.Z().VnD(), Pred, VTMP1.Z().VnD(), VectorUpper.Z().VnD());
        uzp1(Dst.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());
        uzp2(VTMP2.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    // Merge upper half with lower half.
    splice(Dst.Z().VnD(), PRED_TMP_16B, Dst.Z().VnD(), VTMP2.Z().VnD());
  } else {
    switch (ElementSize) {
      case 2: {
        faddp(Dst.V8H(), VectorLower.V8H(), VectorUpper.V8H());
        break;
      }
      case 4: {
        faddp(Dst.V4S(), VectorLower.V4S(), VectorUpper.V4S());
        break;
      }
      case 8: {
        faddp(Dst.V2D(), VectorLower.V2D(), VectorUpper.V2D());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VFSub) {
  const auto Op = IROp->C<IR::IROp_VFSub>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && !IsScalar) {
    switch (ElementSize) {
      case 2: {
        fsub(Dst.Z().VnH(), Vector1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        fsub(Dst.Z().VnS(), Vector1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        fsub(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fsub(Dst.H(), Vector1.H(), Vector2.H());
          break;
        }
        case 4: {
          fsub(Dst.S(), Vector1.S(), Vector2.S());
          break;
        }
        case 8: {
          fsub(Dst.D(), Vector1.D(), Vector2.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fsub(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
          break;
        }
        case 4: {
          fsub(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
          break;
        }
        case 8: {
          fsub(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFMul) {
  const auto Op = IROp->C<IR::IROp_VFMul>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && !IsScalar) {
    switch (ElementSize) {
      case 2: {
        fmul(Dst.Z().VnH(), Vector1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        fmul(Dst.Z().VnS(), Vector1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        fmul(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fmul(Dst.H(), Vector1.H(), Vector2.H());
          break;
        }
        case 4: {
          fmul(Dst.S(), Vector1.S(), Vector2.S());
          break;
        }
        case 8: {
          fmul(Dst.D(), Vector1.D(), Vector2.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fmul(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
          break;
        }
        case 4: {
          fmul(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
          break;
        }
        case 8: {
          fmul(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFDiv) {
  const auto Op = IROp->C<IR::IROp_VFDiv>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Merging();

    // SVE VDIV is a destructive operation, so we need a temporary.
    movprfx(VTMP1.Z().VnD(), Vector1.Z().VnD());

    switch (ElementSize) {
      case 2: {
        fdiv(VTMP1.Z().VnH(), Mask,
             VTMP1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        fdiv(VTMP1.Z().VnS(), Mask,
             VTMP1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        fdiv(VTMP1.Z().VnD(), Mask,
             VTMP1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fdiv(Dst.H(), Vector1.H(), Vector2.H());
          break;
        }
        case 4: {
          fdiv(Dst.S(), Vector1.S(), Vector2.S());
          break;
        }
        case 8: {
          fdiv(Dst.D(), Vector1.D(), Vector2.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fdiv(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
          break;
        }
        case 4: {
          fdiv(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
          break;
        }
        case 8: {
          fdiv(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFMin) {
  const auto Op = IROp->C<IR::IROp_VFMin>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  // NOTE: We don't directly use FMIN here for any of the implementations,
  //       because it has undesirable NaN handling behavior (it sets
  //       entries either to the incoming NaN value*, or the default NaN
  //       depending on FPCR flags set). We want behavior that sets NaN
  //       entries to zero for the comparison result.
  //
  // * - Not exactly (differs slightly with SNaNs), but close enough for the explanation

  if (HostSupportsSVE && !IsScalar) {
    const auto Mask = Is256Bit ? PRED_TMP_32B
                               : PRED_TMP_16B;
    const auto ComparePred = p0;

    // General idea:
    // 1. Compare greater than against the two vectors
    // 2. Invert the resulting values in the predicate register.
    // 3. Move the first vector into a temporary
    // 4. Merge all the elements that correspond to the inverted
    //    predicate bits from the second vector into the
    //    same temporary.
    // 5. Move temporary into the destination register and we're done.

    switch (ElementSize) {
      case 2: {
        fcmgt(ComparePred.VnH(), Mask.Zeroing(),
              Vector2.Z().VnH(), Vector1.Z().VnH());
        not_(ComparePred.VnB(), Mask.Zeroing(), ComparePred.VnB());
        mov(VTMP1.Z().VnD(), Vector1.Z().VnD());
        mov(VTMP1.Z().VnH(), ComparePred.Merging(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        fcmgt(ComparePred.VnS(), Mask.Zeroing(),
              Vector2.Z().VnS(), Vector1.Z().VnS());
        not_(ComparePred.VnB(), Mask.Zeroing(), ComparePred.VnB());
        mov(VTMP1.Z().VnD(), Vector1.Z().VnD());
        mov(VTMP1.Z().VnS(), ComparePred.Merging(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        fcmgt(ComparePred.VnD(), Mask.Zeroing(),
              Vector2.Z().VnD(), Vector1.Z().VnD());
        not_(ComparePred.VnB(), Mask.Zeroing(), ComparePred.VnB());
        mov(VTMP1.Z().VnD(), Vector1.Z().VnD());
        mov(VTMP1.Z().VnD(), ComparePred.Merging(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fcmp(Vector1.H(), Vector2.H());
          fcsel(Dst.H(), Vector1.H(), Vector2.H(), Condition::mi);
          break;
        }
        case 4: {
          fcmp(Vector1.S(), Vector2.S());
          fcsel(Dst.S(), Vector1.S(), Vector2.S(), Condition::mi);
          break;
        }
        case 8: {
          fcmp(Vector1.D(), Vector2.D());
          fcsel(Dst.D(), Vector1.D(), Vector2.D(), Condition::mi);
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fcmgt(VTMP1.V8H(), Vector2.V8H(), Vector1.V8H());
          mov(VTMP2.V8H(), Vector1.V8H());
          bif(VTMP2.V16B(), Vector2.V16B(), VTMP1.V16B());
          mov(Dst.V8H(), VTMP2.V8H());
          break;
        }
        case 4: {
          fcmgt(VTMP1.V4S(), Vector2.V4S(), Vector1.V4S());
          mov(VTMP2.V4S(), Vector1.V4S());
          bif(VTMP2.V16B(), Vector2.V16B(), VTMP1.V16B());
          mov(Dst.V4S(), VTMP2.V4S());
          break;
        }
        case 8: {
          fcmgt(VTMP1.V2D(), Vector2.V2D(), Vector1.V2D());
          mov(VTMP2.V2D(), Vector1.V2D());
          bif(VTMP2.V16B(), Vector2.V16B(), VTMP1.V16B());
          mov(Dst.V2D(), VTMP2.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFMax) {
  const auto Op = IROp->C<IR::IROp_VFMax>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  // NOTE: See VFMin implementation for reasons why we
  //       don't just use FMAX/FMIN for these implementations.

  if (HostSupportsSVE && !IsScalar) {
    const auto Mask = Is256Bit ? PRED_TMP_32B
                               : PRED_TMP_16B;
    const auto ComparePred = p0;

    switch (ElementSize) {
      case 2: {
        fcmgt(ComparePred.VnH(), Mask.Zeroing(),
              Vector2.Z().VnH(), Vector1.Z().VnH());
        mov(VTMP1.Z().VnD(), Vector1.Z().VnD());
        mov(VTMP1.Z().VnH(), ComparePred.Merging(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        fcmgt(ComparePred.VnS(), Mask.Zeroing(),
              Vector2.Z().VnS(), Vector1.Z().VnS());
        mov(VTMP1.Z().VnD(), Vector1.Z().VnD());
        mov(VTMP1.Z().VnS(), ComparePred.Merging(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        fcmgt(ComparePred.VnD(), Mask.Zeroing(),
              Vector2.Z().VnD(), Vector1.Z().VnD());
        mov(VTMP1.Z().VnD(), Vector1.Z().VnD());
        mov(VTMP1.Z().VnD(), ComparePred.Merging(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fcmp(Vector1.H(), Vector2.H());
          fcsel(Dst.H(), Vector2.H(), Vector1.H(), Condition::mi);
          break;
        }
        case 4: {
          fcmp(Vector1.S(), Vector2.S());
          fcsel(Dst.S(), Vector2.S(), Vector1.S(), Condition::mi);
          break;
        }
        case 8: {
          fcmp(Vector1.D(), Vector2.D());
          fcsel(Dst.D(), Vector2.D(), Vector1.D(), Condition::mi);
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fcmgt(VTMP1.V8H(), Vector2.V8H(), Vector1.V8H());
          mov(VTMP2.V8H(), Vector1.V8H());
          bit(VTMP2.V16B(), Vector2.V16B(), VTMP1.V16B());
          mov(Dst.V8H(), VTMP2.V8H());
          break;
        }
        case 4: {
          fcmgt(VTMP1.V4S(), Vector2.V4S(), Vector1.V4S());
          mov(VTMP2.V4S(), Vector1.V4S());
          bit(VTMP2.V16B(), Vector2.V16B(), VTMP1.V16B());
          mov(Dst.V4S(), VTMP2.V4S());
          break;
        }
        case 8: {
          fcmgt(VTMP1.V2D(), Vector2.V2D(), Vector1.V2D());
          mov(VTMP2.V2D(), Vector1.V2D());
          bit(VTMP2.V16B(), Vector2.V16B(), VTMP1.V16B());
          mov(Dst.V2D(), VTMP2.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFRecp) {
  const auto Op = IROp->C<IR::IROp_VFRecp>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = Op->Header.ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && !IsScalar) {
    const auto Pred = Is256Bit ? PRED_TMP_32B.Merging()
                               : PRED_TMP_16B.Merging();

    switch (ElementSize) {
      case 2: {
        fmov(VTMP1.Z().VnH(), 1.0);
        fdiv(VTMP1.Z().VnH(), Pred, VTMP1.Z().VnH(), Vector.Z().VnH());
        break;
      }
      case 4: {
        fmov(VTMP1.Z().VnS(), 1.0);
        fdiv(VTMP1.Z().VnS(), Pred, VTMP1.Z().VnS(), Vector.Z().VnS());
        break;
      }
      case 8: {
        fmov(VTMP1.Z().VnD(), 1.0);
        fdiv(VTMP1.Z().VnD(), Pred, VTMP1.Z().VnD(), Vector.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fmov(VTMP1.H(), Float16{1.0});
          fdiv(Dst.H(), VTMP1.H(), Vector.H());
          break;
        }
        case 4: {
          fmov(VTMP1.S(), 1.0f);
          fdiv(Dst.S(), VTMP1.S(), Vector.S());
          break;
        }
        case 8: {
          fmov(VTMP1.D(), 1.0);
          fdiv(Dst.D(), VTMP1.D(), Vector.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fmov(VTMP1.V8H(), Float16{1.0});
          fdiv(Dst.V8H(), VTMP1.V8H(), Vector.V8H());
          break;
        }
        case 4: {
          fmov(VTMP1.V4S(), 1.0f);
          fdiv(Dst.V4S(), VTMP1.V4S(), Vector.V4S());
          break;
        }
        case 8: {
          fmov(VTMP1.V2D(), 1.0);
          fdiv(Dst.V2D(), VTMP1.V2D(), Vector.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFSqrt) {
  const auto Op = IROp->C<IR::IROp_VFRSqrt>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && !IsScalar) {
    const auto Pred = Is256Bit ? PRED_TMP_32B.Merging()
                               : PRED_TMP_16B.Merging();

    switch (ElementSize) {
      case 2: {
        fsqrt(Dst.Z().VnH(), Pred, Vector.Z().VnH());
        break;
      }
      case 4: {
        fsqrt(Dst.Z().VnS(), Pred, Vector.Z().VnS());
        break;
      }
      case 8: {
        fsqrt(Dst.Z().VnD(), Pred, Vector.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fsqrt(Dst.H(), Vector.H());
          break;
        }
        case 4: {
          fsqrt(Dst.S(), Vector.S());
          break;
        }
        case 8: {
          fsqrt(Dst.D(), Vector.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fsqrt(Dst.V8H(), Vector.V8H());
          break;
        }
        case 4: {
          fsqrt(Dst.V4S(), Vector.V4S());
          break;
        }
        case 8: {
          fsqrt(Dst.V2D(), Vector.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFRSqrt) {
  const auto Op = IROp->C<IR::IROp_VFRSqrt>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Pred = PRED_TMP_32B.Merging();

    switch (ElementSize) {
      case 2: {
        fmov(VTMP1.Z().VnH(), 1.0);
        fsqrt(VTMP2.Z().VnH(), Pred, Vector.Z().VnH());
        fdiv(VTMP1.Z().VnH(), Pred, VTMP1.Z().VnH(), VTMP2.Z().VnH());
        break;
      }
      case 4: {
        fmov(VTMP1.Z().VnS(), 1.0);
        fsqrt(VTMP2.Z().VnS(), Pred, Vector.Z().VnS());
        fdiv(VTMP1.Z().VnS(), Pred, VTMP1.Z().VnS(), VTMP2.Z().VnS());
        break;
      }
      case 8: {
        fmov(VTMP1.Z().VnD(), 1.0);
        fsqrt(VTMP2.Z().VnD(), Pred, Vector.Z().VnD());
        fdiv(VTMP1.Z().VnD(), Pred, VTMP1.Z().VnD(), VTMP2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fmov(VTMP1.H(), Float16{1.0});
          fsqrt(VTMP2.H(), Vector.H());
          fdiv(Dst.H(), VTMP1.H(), VTMP2.H());
          break;
        }
        case 4: {
          fmov(VTMP1.S(), 1.0f);
          fsqrt(VTMP2.S(), Vector.S());
          fdiv(Dst.S(), VTMP1.S(), VTMP2.S());
          break;
        }
        case 8: {
          fmov(VTMP1.D(), 1.0);
          fsqrt(VTMP2.D(), Vector.D());
          fdiv(Dst.D(), VTMP1.D(), VTMP2.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fmov(VTMP1.V8H(), Float16{1.0});
          fsqrt(VTMP2.V8H(), Vector.V8H());
          fdiv(Dst.V8H(), VTMP1.V8H(), VTMP2.V8H());
          break;
        }
        case 4: {
          fmov(VTMP1.V4S(), 1.0f);
          fsqrt(VTMP2.V4S(), Vector.V4S());
          fdiv(Dst.V4S(), VTMP1.V4S(), VTMP2.V4S());
          break;
        }
        case 8: {
          fmov(VTMP1.V2D(), 1.0);
          fsqrt(VTMP2.V2D(), Vector.V2D());
          fdiv(Dst.V2D(), VTMP1.V2D(), VTMP2.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VNeg) {
  const auto Op = IROp->C<IR::IROp_VNeg>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE) {
    const auto Pred = Is256Bit ? PRED_TMP_32B.Merging()
                               : PRED_TMP_16B.Merging();

    switch (ElementSize) {
      case 1:
        neg(Dst.Z().VnB(), Pred, Vector.Z().VnB());
        break;
      case 2:
        neg(Dst.Z().VnH(), Pred, Vector.Z().VnH());
        break;
      case 4:
        neg(Dst.Z().VnS(), Pred, Vector.Z().VnS());
        break;
      case 8:
        neg(Dst.Z().VnD(), Pred, Vector.Z().VnD());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unsupported VNeg size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
    case 1:
      neg(Dst.V16B(), Vector.V16B());
      break;
    case 2:
      neg(Dst.V8H(), Vector.V8H());
      break;
    case 4:
      neg(Dst.V4S(), Vector.V4S());
      break;
    case 8:
      neg(Dst.V2D(), Vector.V2D());
      break;
    default:
      LOGMAN_MSG_A_FMT("Unsupported VNeg size: {}", ElementSize);
      break;
    }
  }
}

DEF_OP(VFNeg) {
  const auto Op = IROp->C<IR::IROp_VFNeg>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE) {
    const auto Pred = Is256Bit ? PRED_TMP_32B.Merging()
                               : PRED_TMP_16B.Merging();

    switch (ElementSize) {
      case 2:
        fneg(Dst.Z().VnH(), Pred, Vector.Z().VnH());
        break;
      case 4:
        fneg(Dst.Z().VnS(), Pred, Vector.Z().VnS());
        break;
      case 8:
        fneg(Dst.Z().VnD(), Pred, Vector.Z().VnD());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unsupported VFNeg element size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 2:
        fneg(Dst.V8H(), Vector.V8H());
        break;
      case 4:
        fneg(Dst.V4S(), Vector.V4S());
        break;
      case 8:
        fneg(Dst.V2D(), Vector.V2D());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unsupported VFNeg element size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VNot) {
  const auto Op = IROp->C<IR::IROp_VNot>();
  const auto OpSize = IROp->Size;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    not_(Dst.Z().VnB(), PRED_TMP_32B.Merging(), Vector.Z().VnB());
  } else {
    mvn(Dst.V16B(), Vector.V16B());
  }
}

DEF_OP(VUMin) {
  const auto Op = IROp->C<IR::IROp_VUMin>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Pred = PRED_TMP_32B.Merging();

    // SVE UMIN is a destructive operation so we need a temporary.
    movprfx(VTMP1.Z().VnD(), Vector1.Z().VnD());

    switch (ElementSize) {
      case 1: {
        umin(VTMP1.Z().VnB(), Pred, VTMP1.Z().VnB(), Vector2.Z().VnB());
        break;
      }
      case 2: {
        umin(VTMP1.Z().VnH(), Pred, VTMP1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        umin(VTMP1.Z().VnS(), Pred, VTMP1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        umin(VTMP1.Z().VnD(), Pred, VTMP1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    switch (ElementSize) {
      case 1: {
        umin(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
        break;
      }
      case 2: {
        umin(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
        break;
      }
      case 4: {
        umin(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
        break;
      }
      case 8: {
        cmhi(VTMP1.V2D(), Vector2.V2D(), Vector1.V2D());
        mov(VTMP2.V2D(), Vector1.V2D());
        bif(VTMP2.V16B(), Vector2.V16B(), VTMP1.V16B());
        mov(Dst.V2D(), VTMP2.V2D());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VSMin) {
  const auto Op = IROp->C<IR::IROp_VSMin>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Pred = PRED_TMP_32B.Merging();

    // SVE SMIN is a destructive operation, so we need a temporary.
    movprfx(VTMP1.Z().VnD(), Vector1.Z().VnD());

    switch (ElementSize) {
      case 1: {
        smin(VTMP1.Z().VnB(), Pred, VTMP1.Z().VnB(), Vector2.Z().VnB());
        break;
      }
      case 2: {
        smin(VTMP1.Z().VnH(), Pred, VTMP1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        smin(VTMP1.Z().VnS(), Pred, VTMP1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        smin(VTMP1.Z().VnD(), Pred, VTMP1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    switch (ElementSize) {
      case 1: {
        smin(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
        break;
      }
      case 2: {
        smin(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
        break;
      }
      case 4: {
        smin(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
        break;
      }
      case 8: {
        cmgt(VTMP1.V2D(), Vector2.V2D(), Vector1.V2D());
        mov(VTMP2.V2D(), Vector1.V2D());
        bif(VTMP2.V16B(), Vector2.V16B(), VTMP1.V16B());
        mov(Dst.V2D(), VTMP2.V2D());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VUMax) {
  const auto Op = IROp->C<IR::IROp_VUMax>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Pred = PRED_TMP_32B.Merging();

    // SVE UMAX is a destructive operation, so we need a temporary.
    movprfx(VTMP1.Z().VnD(), Vector1.Z().VnD());

    switch (ElementSize) {
      case 1: {
        umax(VTMP1.Z().VnB(), Pred, VTMP1.Z().VnB(), Vector2.Z().VnB());
        break;
      }
      case 2: {
        umax(VTMP1.Z().VnH(), Pred, VTMP1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        umax(VTMP1.Z().VnS(), Pred, VTMP1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        umax(VTMP1.Z().VnD(), Pred, VTMP1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    switch (ElementSize) {
      case 1: {
        umax(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
        break;
      }
      case 2: {
        umax(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
        break;
      }
      case 4: {
        umax(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
        break;
      }
      case 8: {
        cmhi(VTMP1.V2D(), Vector2.V2D(), Vector1.V2D());
        mov(VTMP2.V2D(), Vector1.V2D());
        bit(VTMP2.V16B(), Vector2.V16B(), VTMP1.V16B());
        mov(Dst.V2D(), VTMP2.V2D());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VSMax) {
  const auto Op = IROp->C<IR::IROp_VSMax>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Pred = PRED_TMP_32B.Merging();

    // SVE SMAX is a destructive operation, so we need a temporary.
    movprfx(VTMP1.Z().VnD(), Vector1.Z().VnD());

    switch (ElementSize) {
      case 1: {
        smax(VTMP1.Z().VnB(), Pred, VTMP1.Z().VnB(), Vector2.Z().VnB());
        break;
      }
      case 2: {
        smax(VTMP1.Z().VnH(), Pred, VTMP1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        smax(VTMP1.Z().VnS(), Pred, VTMP1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        smax(VTMP1.Z().VnD(), Pred, VTMP1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    switch (ElementSize) {
      case 1: {
        smax(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
        break;
      }
      case 2: {
        smax(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
        break;
      }
      case 4: {
        smax(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
        break;
      }
      case 8: {
        cmgt(VTMP1.V2D(), Vector2.V2D(), Vector1.V2D());
        mov(VTMP2.V2D(), Vector1.V2D());
        bit(VTMP2.V16B(), Vector2.V16B(), VTMP1.V16B());
        mov(Dst.V2D(), VTMP2.V2D());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VZip) {
  const auto Op = IROp->C<IR::IROp_VZip>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto VectorLower = GetVReg(Op->VectorLower.ID());
  const auto VectorUpper = GetVReg(Op->VectorUpper.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 1: {
        zip1(Dst.Z().VnB(), VectorLower.Z().VnB(), VectorUpper.Z().VnB());
        break;
      }
      case 2: {
        zip1(Dst.Z().VnH(), VectorLower.Z().VnH(), VectorUpper.Z().VnH());
        break;
      }
      case 4: {
        zip1(Dst.Z().VnS(), VectorLower.Z().VnS(), VectorUpper.Z().VnS());
        break;
      }
      case 8: {
        zip1(Dst.Z().VnD(), VectorLower.Z().VnD(), VectorUpper.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    if (OpSize == 8) {
      switch (ElementSize) {
        case 1: {
          zip1(Dst.V8B(), VectorLower.V8B(), VectorUpper.V8B());
          break;
        }
        case 2: {
          zip1(Dst.V4H(), VectorLower.V4H(), VectorUpper.V4H());
          break;
        }
        case 4: {
          zip1(Dst.V2S(), VectorLower.V2S(), VectorUpper.V2S());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 1: {
          zip1(Dst.V16B(), VectorLower.V16B(), VectorUpper.V16B());
          break;
        }
        case 2: {
          zip1(Dst.V8H(), VectorLower.V8H(), VectorUpper.V8H());
          break;
        }
        case 4: {
          zip1(Dst.V4S(), VectorLower.V4S(), VectorUpper.V4S());
          break;
        }
        case 8: {
          zip1(Dst.V2D(), VectorLower.V2D(), VectorUpper.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VZip2) {
  const auto Op = IROp->C<IR::IROp_VZip2>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto VectorLower = GetVReg(Op->VectorLower.ID());
  const auto VectorUpper = GetVReg(Op->VectorUpper.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 1: {
        zip2(Dst.Z().VnB(), VectorLower.Z().VnB(), VectorUpper.Z().VnB());
        break;
      }
      case 2: {
        zip2(Dst.Z().VnH(), VectorLower.Z().VnH(), VectorUpper.Z().VnH());
        break;
      }
      case 4: {
        zip2(Dst.Z().VnS(), VectorLower.Z().VnS(), VectorUpper.Z().VnS());
        break;
      }
      case 8: {
        zip2(Dst.Z().VnD(), VectorLower.Z().VnD(), VectorUpper.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    if (OpSize == 8) {
      switch (ElementSize) {
      case 1: {
        zip2(Dst.V8B(), VectorLower.V8B(), VectorUpper.V8B());
        break;
      }
      case 2: {
        zip2(Dst.V4H(), VectorLower.V4H(), VectorUpper.V4H());
        break;
      }
      case 4: {
        zip2(Dst.V2S(), VectorLower.V2S(), VectorUpper.V2S());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
      }
    } else {
      switch (ElementSize) {
      case 1: {
        zip2(Dst.V16B(), VectorLower.V16B(), VectorUpper.V16B());
        break;
      }
      case 2: {
        zip2(Dst.V8H(), VectorLower.V8H(), VectorUpper.V8H());
        break;
      }
      case 4: {
        zip2(Dst.V4S(), VectorLower.V4S(), VectorUpper.V4S());
        break;
      }
      case 8: {
        zip2(Dst.V2D(), VectorLower.V2D(), VectorUpper.V2D());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
      }
    }
  }
}

DEF_OP(VUnZip) {
  const auto Op = IROp->C<IR::IROp_VUnZip>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto VectorLower = GetVReg(Op->VectorLower.ID());
  const auto VectorUpper = GetVReg(Op->VectorUpper.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 1: {
        uzp1(Dst.Z().VnB(), VectorLower.Z().VnB(), VectorUpper.Z().VnB());
        break;
      }
      case 2: {
        uzp1(Dst.Z().VnH(), VectorLower.Z().VnH(), VectorUpper.Z().VnH());
        break;
      }
      case 4: {
        uzp1(Dst.Z().VnS(), VectorLower.Z().VnS(), VectorUpper.Z().VnS());
        break;
      }
      case 8: {
        uzp1(Dst.Z().VnD(), VectorLower.Z().VnD(), VectorUpper.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    if (OpSize == 8) {
      switch (ElementSize) {
        case 1: {
          uzp1(Dst.V8B(), VectorLower.V8B(), VectorUpper.V8B());
          break;
        }
        case 2: {
          uzp1(Dst.V4H(), VectorLower.V4H(), VectorUpper.V4H());
          break;
        }
        case 4: {
          uzp1(Dst.V2S(), VectorLower.V2S(), VectorUpper.V2S());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 1: {
          uzp1(Dst.V16B(), VectorLower.V16B(), VectorUpper.V16B());
          break;
        }
        case 2: {
          uzp1(Dst.V8H(), VectorLower.V8H(), VectorUpper.V8H());
          break;
        }
        case 4: {
          uzp1(Dst.V4S(), VectorLower.V4S(), VectorUpper.V4S());
          break;
        }
        case 8: {
          uzp1(Dst.V2D(), VectorLower.V2D(), VectorUpper.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VUnZip2) {
  const auto Op = IROp->C<IR::IROp_VUnZip2>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto VectorLower = GetVReg(Op->VectorLower.ID());
  const auto VectorUpper = GetVReg(Op->VectorUpper.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 1: {
        uzp2(Dst.Z().VnB(), VectorLower.Z().VnB(), VectorUpper.Z().VnB());
        break;
      }
      case 2: {
        uzp2(Dst.Z().VnH(), VectorLower.Z().VnH(), VectorUpper.Z().VnH());
        break;
      }
      case 4: {
        uzp2(Dst.Z().VnS(), VectorLower.Z().VnS(), VectorUpper.Z().VnS());
        break;
      }
      case 8: {
        uzp2(Dst.Z().VnD(), VectorLower.Z().VnD(), VectorUpper.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    if (OpSize == 8) {
      switch (ElementSize) {
      case 1: {
        uzp2(Dst.V8B(), VectorLower.V8B(), VectorUpper.V8B());
        break;
      }
      case 2: {
        uzp2(Dst.V4H(), VectorLower.V4H(), VectorUpper.V4H());
        break;
      }
      case 4: {
        uzp2(Dst.V2S(), VectorLower.V2S(), VectorUpper.V2S());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
      }
    } else {
      switch (ElementSize) {
      case 1: {
        uzp2(Dst.V16B(), VectorLower.V16B(), VectorUpper.V16B());
        break;
      }
      case 2: {
        uzp2(Dst.V8H(), VectorLower.V8H(), VectorUpper.V8H());
        break;
      }
      case 4: {
        uzp2(Dst.V4S(), VectorLower.V4S(), VectorUpper.V4S());
        break;
      }
      case 8: {
        uzp2(Dst.V2D(), VectorLower.V2D(), VectorUpper.V2D());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
      }
    }
  }
}

DEF_OP(VBSL) {
  const auto Op = IROp->C<IR::IROp_VBSL>();
  const auto OpSize = IROp->Size;

  const auto Dst = GetVReg(Node);
  const auto VectorFalse = GetVReg(Op->VectorFalse.ID());
  const auto VectorTrue = GetVReg(Op->VectorTrue.ID());
  const auto VectorMask = GetVReg(Op->VectorMask.ID());

  if (HostSupportsSVE) {
    // NOTE: Slight parameter difference from ASIMD
    //       ASIMD -> BSL Mask, True, False
    //       SVE   -> BSL True, True, False, Mask
    mov(VTMP1.Z().VnD(), VectorTrue.Z().VnD());
    bsl(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VectorFalse.Z().VnD(), VectorMask.Z().VnD());
    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (OpSize == 8) {
      mov(VTMP1.V8B(), VectorMask.V8B());
      bsl(VTMP1.V8B(), VectorTrue.V8B(), VectorFalse.V8B());
      mov(Dst.V8B(), VTMP1.V8B());
    } else {
      mov(VTMP1.V16B(), VectorMask.V16B());
      bsl(VTMP1.V16B(), VectorTrue.V16B(), VectorFalse.V16B());
      mov(Dst.V16B(), VTMP1.V16B());
    }
  }
}

DEF_OP(VCMPEQ) {
  const auto Op = IROp->C<IR::IROp_VCMPEQ>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure no junk is in the temp (important for ensuring
    // non-equal entries remain as zero during the final bitwise OR). 
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    // General idea is to compare for equality, not the equal vals
    // from one of the registers, then or both together to make the
    // relevant equal entries all 1s.

    switch (ElementSize) {
      case 1: {
        cmpeq(ComparePred.VnB(), Mask, Vector1.Z().VnB(), Vector2.Z().VnB());
        not_(VTMP1.Z().VnB(), ComparePred.Merging(), Vector1.Z().VnB());
        orr(VTMP1.Z().VnB(), ComparePred.Merging(), VTMP1.Z().VnB(), Vector1.Z().VnB());
        break;
      }
      case 2: {
        cmpeq(ComparePred.VnH(), Mask, Vector1.Z().VnH(), Vector2.Z().VnH());
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector1.Z().VnH());
        orr(VTMP1.Z().VnH(), ComparePred.Merging(), VTMP1.Z().VnH(), Vector1.Z().VnH());
        break;
      }
      case 4: {
        cmpeq(ComparePred.VnS(), Mask, Vector1.Z().VnS(), Vector2.Z().VnS());
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector1.Z().VnS());
        orr(VTMP1.Z().VnS(), ComparePred.Merging(), VTMP1.Z().VnS(), Vector1.Z().VnS());
        break;
      }
      case 8: {
        cmpeq(ComparePred.VnD(), Mask, Vector1.Z().VnD(), Vector2.Z().VnD());
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector1.Z().VnD());
        orr(VTMP1.Z().VnD(), ComparePred.Merging(), VTMP1.Z().VnD(), Vector1.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 4: {
          cmeq(Dst.S(), Vector1.S(), Vector2.S());
          break;
        }
        case 8: {
          cmeq(Dst.D(), Vector1.D(), Vector2.D());
          break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
      }
    } else {
      switch (ElementSize) {
        case 1: {
          cmeq(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
          break;
        }
        case 2: {
          cmeq(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
          break;
        }
        case 4: {
          cmeq(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
          break;
        }
        case 8: {
          cmeq(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
          break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
      }
    }
  }
}

DEF_OP(VCMPEQZ) {
  const auto Op = IROp->C<IR::IROp_VCMPEQZ>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure no junk is in the temp (important for ensuring
    // non-equal entries remain as zero).
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    // Unlike with VCMPEQ, we can skip needing to bitwise OR the
    // final results, since if our elements are equal to zero,
    // we just need to bitwise NOT them and they're already set
    // to all 1s.
    switch (ElementSize) {
      case 1: {
        cmpeq(ComparePred.VnB(), Mask, Vector.Z().VnB(), 0);
        not_(VTMP1.Z().VnB(), ComparePred.Merging(), Vector.Z().VnB());
        break;
      }
      case 2: {
        cmpeq(ComparePred.VnH(), Mask, Vector.Z().VnH(), 0);
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector.Z().VnH());
        break;
      }
      case 4: {
        cmpeq(ComparePred.VnS(), Mask, Vector.Z().VnS(), 0);
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector.Z().VnS());
        break;
      }
      case 8: {
        cmpeq(ComparePred.VnD(), Mask, Vector.Z().VnD(), 0);
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 4: {
          cmeq(Dst.S(), Vector.S(), 0);
          break;
        }
        case 8: {
          cmeq(Dst.D(), Vector.D(), 0);
          break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
      }
    } else {
      switch (ElementSize) {
        case 1: {
          cmeq(Dst.V16B(), Vector.V16B(), 0);
          break;
        }
        case 2: {
          cmeq(Dst.V8H(), Vector.V8H(), 0);
          break;
        }
        case 4: {
          cmeq(Dst.V4S(), Vector.V4S(), 0);
          break;
        }
        case 8: {
          cmeq(Dst.V2D(), Vector.V2D(), 0);
          break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
      }
    }
  }
}

DEF_OP(VCMPGT) {
  const auto Op = IROp->C<IR::IROp_VCMPGT>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure no junk is in the temp (important for ensuring
    // non greater-than values remain as zero).
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    // General idea is to compare for greater-than, bitwise NOT
    // the valid values, then ORR the NOTed values with the original
    // values to form entries that are all 1s.

    switch (ElementSize) {
      case 1: {
        cmpgt(ComparePred.VnB(), Mask, Vector1.Z().VnB(), Vector2.Z().VnB());
        not_(VTMP1.Z().VnB(), ComparePred.Merging(), Vector1.Z().VnB());
        orr(VTMP1.Z().VnB(), ComparePred.Merging(), VTMP1.Z().VnB(), Vector1.Z().VnB());
        break;
      }
      case 2: {
        cmpgt(ComparePred.VnH(), Mask, Vector1.Z().VnH(), Vector2.Z().VnH());
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector1.Z().VnH());
        orr(VTMP1.Z().VnH(), ComparePred.Merging(), VTMP1.Z().VnH(), Vector1.Z().VnH());
        break;
      }
      case 4: {
        cmpgt(ComparePred.VnS(), Mask, Vector1.Z().VnS(), Vector2.Z().VnS());
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector1.Z().VnS());
        orr(VTMP1.Z().VnS(), ComparePred.Merging(), VTMP1.Z().VnS(), Vector1.Z().VnS());
        break;
      }
      case 8: {
        cmpgt(ComparePred.VnD(), Mask, Vector1.Z().VnD(), Vector2.Z().VnD());
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector1.Z().VnD());
        orr(VTMP1.Z().VnD(), ComparePred.Merging(), VTMP1.Z().VnD(), Vector1.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 4: {
          cmgt(Dst.S(), Vector1.S(), Vector2.S());
          break;
        }
        case 8: {
          cmgt(Dst.D(), Vector1.D(), Vector2.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 1: {
          cmgt(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
          break;
        }
        case 2: {
          cmgt(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
          break;
        }
        case 4: {
          cmgt(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
          break;
        }
        case 8: {
          cmgt(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VCMPGTZ) {
  const auto Op = IROp->C<IR::IROp_VCMPGTZ>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure no junk is in the temp (important for ensuring
    // non greater-than values remain as zero).
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    switch (ElementSize) {
      case 1: {
        cmpgt(ComparePred.VnB(), Mask, Vector.Z().VnB(), 0);
        not_(VTMP1.Z().VnB(), ComparePred.Merging(), Vector.Z().VnB());
        orr(VTMP1.Z().VnB(), ComparePred.Merging(), VTMP1.Z().VnB(), Vector.Z().VnB());
        break;
      }
      case 2: {
        cmpgt(ComparePred.VnH(), Mask, Vector.Z().VnH(), 0);
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector.Z().VnH());
        orr(VTMP1.Z().VnH(), ComparePred.Merging(), VTMP1.Z().VnH(), Vector.Z().VnH());
        break;
      }
      case 4: {
        cmpgt(ComparePred.VnS(), Mask, Vector.Z().VnS(), 0);
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector.Z().VnS());
        orr(VTMP1.Z().VnS(), ComparePred.Merging(), VTMP1.Z().VnS(), Vector.Z().VnS());
        break;
      }
      case 8: {
        cmpgt(ComparePred.VnD(), Mask, Vector.Z().VnD(), 0);
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector.Z().VnD());
        orr(VTMP1.Z().VnD(), ComparePred.Merging(), VTMP1.Z().VnD(), Vector.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 4: {
          cmgt(Dst.S(), Vector.S(), 0);
          break;
        }
        case 8: {
          cmgt(Dst.D(), Vector.D(), 0);
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 1: {
          cmgt(Dst.V16B(), Vector.V16B(), 0);
          break;
        }
        case 2: {
          cmgt(Dst.V8H(), Vector.V8H(), 0);
          break;
        }
        case 4: {
          cmgt(Dst.V4S(), Vector.V4S(), 0);
          break;
        }
        case 8: {
          cmgt(Dst.V2D(), Vector.V2D(), 0);
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VCMPLTZ) {
  const auto Op = IROp->C<IR::IROp_VCMPLTZ>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure no junk is in the temp (important for ensuring
    // non less-than values remain as zero).
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    switch (ElementSize) {
      case 1: {
        cmplt(ComparePred.VnB(), Mask, Vector.Z().VnB(), 0);
        not_(VTMP1.Z().VnB(), ComparePred.Merging(), Vector.Z().VnB());
        orr(VTMP1.Z().VnB(), ComparePred.Merging(), VTMP1.Z().VnB(), Vector.Z().VnB());
        break;
      }
      case 2: {
        cmplt(ComparePred.VnH(), Mask, Vector.Z().VnH(), 0);
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector.Z().VnH());
        orr(VTMP1.Z().VnH(), ComparePred.Merging(), VTMP1.Z().VnH(), Vector.Z().VnH());
        break;
      }
      case 4: {
        cmplt(ComparePred.VnS(), Mask, Vector.Z().VnS(), 0);
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector.Z().VnS());
        orr(VTMP1.Z().VnS(), ComparePred.Merging(), VTMP1.Z().VnS(), Vector.Z().VnS());
        break;
      }
      case 8: {
        cmplt(ComparePred.VnD(), Mask, Vector.Z().VnD(), 0);
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector.Z().VnD());
        orr(VTMP1.Z().VnD(), ComparePred.Merging(), VTMP1.Z().VnD(), Vector.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 4: {
          cmlt(Dst.S(), Vector.S(), 0);
          break;
        }
        case 8: {
          cmlt(Dst.D(), Vector.D(), 0);
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 1: {
          cmlt(Dst.V16B(), Vector.V16B(), 0);
          break;
        }
        case 2: {
          cmlt(Dst.V8H(), Vector.V8H(), 0);
          break;
        }
        case 4: {
          cmlt(Dst.V4S(), Vector.V4S(), 0);
          break;
        }
        case 8: {
          cmlt(Dst.V2D(), Vector.V2D(), 0);
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFCMPEQ) {
  const auto Op = IROp->C<IR::IROp_VFCMPEQ>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure we have no junk in the temporary.
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    switch (ElementSize) {
      case 2: {
        fcmeq(ComparePred.VnH(), Mask, Vector1.Z().VnH(), Vector2.Z().VnH());
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector1.Z().VnH());
        orr(VTMP1.Z().VnH(), ComparePred.Merging(), VTMP1.Z().VnH(), Vector1.Z().VnH());
        break;
      }
      case 4: {
        fcmeq(ComparePred.VnS(), Mask, Vector1.Z().VnS(), Vector2.Z().VnS());
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector1.Z().VnS());
        orr(VTMP1.Z().VnS(), ComparePred.Merging(), VTMP1.Z().VnS(), Vector1.Z().VnS());
        break;
      }
      case 8: {
        fcmeq(ComparePred.VnD(), Mask, Vector1.Z().VnD(), Vector2.Z().VnD());
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector1.Z().VnD());
        orr(VTMP1.Z().VnD(), ComparePred.Merging(), VTMP1.Z().VnD(), Vector1.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fcmeq(Dst.H(), Vector1.H(), Vector2.H());
          break;
        }
        case 4: {
          fcmeq(Dst.S(), Vector1.S(), Vector2.S());
          break;
        }
        case 8: {
          fcmeq(Dst.D(), Vector1.D(), Vector2.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fcmeq(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
          break;
        }
        case 4: {
          fcmeq(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
          break;
        }
        case 8: {
          fcmeq(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFCMPNEQ) {
  const auto Op = IROp->C<IR::IROp_VFCMPNEQ>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure we have no junk in the temporary.
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    switch (ElementSize) {
      case 2: {
        fcmne(ComparePred.VnH(), Mask, Vector1.Z().VnH(), Vector2.Z().VnH());
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector1.Z().VnH());
        orr(VTMP1.Z().VnH(), ComparePred.Merging(), VTMP1.Z().VnH(), Vector1.Z().VnH());
        break;
      }
      case 4: {
        fcmne(ComparePred.VnS(), Mask, Vector1.Z().VnS(), Vector2.Z().VnS());
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector1.Z().VnS());
        orr(VTMP1.Z().VnS(), ComparePred.Merging(), VTMP1.Z().VnS(), Vector1.Z().VnS());
        break;
      }
      case 8: {
        fcmne(ComparePred.VnD(), Mask, Vector1.Z().VnD(), Vector2.Z().VnD());
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector1.Z().VnD());
        orr(VTMP1.Z().VnD(), ComparePred.Merging(), VTMP1.Z().VnD(), Vector1.Z().VnD());
        break;
      }
      default:
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fcmeq(Dst.H(), Vector1.H(), Vector2.H());
          break;
        }
        case 4: {
          fcmeq(Dst.S(), Vector1.S(), Vector2.S());
          break;
        }
        case 8: {
          fcmeq(Dst.D(), Vector1.D(), Vector2.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
      mvn(Dst.V8B(), Dst.V8B());
    } else {
      switch (ElementSize) {
        case 2: {
          fcmeq(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
          break;
        }
        case 4: {
          fcmeq(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
          break;
        }
        case 8: {
          fcmeq(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
      mvn(Dst.V16B(), Dst.V16B());
    }
  }
}

DEF_OP(VFCMPLT) {
  const auto Op = IROp->C<IR::IROp_VFCMPLT>();
  const auto  OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure we have no junk in the temporary.
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    switch (ElementSize) {
      case 2: {
        fcmgt(ComparePred.VnH(), Mask, Vector2.Z().VnH(), Vector1.Z().VnH());
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector2.Z().VnH());
        orr(VTMP1.Z().VnH(), ComparePred.Merging(), VTMP1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        fcmgt(ComparePred.VnS(), Mask, Vector2.Z().VnS(), Vector1.Z().VnS());
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector2.Z().VnS());
        orr(VTMP1.Z().VnS(), ComparePred.Merging(), VTMP1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        fcmgt(ComparePred.VnD(), Mask, Vector2.Z().VnD(), Vector1.Z().VnD());
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector2.Z().VnD());
        orr(VTMP1.Z().VnD(), ComparePred.Merging(), VTMP1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fcmgt(Dst.H(), Vector2.H(), Vector1.H());
          break;
        }
        case 4: {
          fcmgt(Dst.S(), Vector2.S(), Vector1.S());
          break;
        }
        case 8: {
          fcmgt(Dst.D(), Vector2.D(), Vector1.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fcmgt(Dst.V8H(), Vector2.V8H(), Vector1.V8H());
          break;
        }
        case 4: {
          fcmgt(Dst.V4S(), Vector2.V4S(), Vector1.V4S());
          break;
        }
        case 8: {
          fcmgt(Dst.V2D(), Vector2.V2D(), Vector1.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFCMPGT) {
  const auto Op = IROp->C<IR::IROp_VFCMPGT>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure there's no junk in the temporary.
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    switch (ElementSize) {
      case 2: {
        fcmgt(ComparePred.VnH(), Mask, Vector1.Z().VnH(), Vector2.Z().VnH());
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector1.Z().VnH());
        orr(VTMP1.Z().VnH(), ComparePred.Merging(), VTMP1.Z().VnH(), Vector1.Z().VnH());
        break;
      }
      case 4: {
        fcmgt(ComparePred.VnS(), Mask, Vector1.Z().VnS(), Vector2.Z().VnS());
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector1.Z().VnS());
        orr(VTMP1.Z().VnS(), ComparePred.Merging(), VTMP1.Z().VnS(), Vector1.Z().VnS());
        break;
      }
      case 8: {
        fcmgt(ComparePred.VnD(), Mask, Vector1.Z().VnD(), Vector2.Z().VnD());
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector1.Z().VnD());
        orr(VTMP1.Z().VnD(), ComparePred.Merging(), VTMP1.Z().VnD(), Vector1.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fcmgt(Dst.H(), Vector1.H(), Vector2.H());
          break;
        }
        case 4: {
          fcmgt(Dst.S(), Vector1.S(), Vector2.S());
          break;
        }
        case 8: {
          fcmgt(Dst.D(), Vector1.D(), Vector2.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fcmgt(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
          break;
        }
        case 4: {
          fcmgt(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
          break;
        }
        case 8: {
          fcmgt(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFCMPLE) {
  const auto Op = IROp->C<IR::IROp_VFCMPLE>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure there's no junk in the temporary.
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    switch (ElementSize) {
      case 2: {
        fcmge(ComparePred.VnH(), Mask, Vector2.Z().VnH(), Vector1.Z().VnH());
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector2.Z().VnH());
        orr(VTMP1.Z().VnH(), ComparePred.Merging(), VTMP1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        fcmge(ComparePred.VnS(), Mask, Vector2.Z().VnS(), Vector1.Z().VnS());
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector2.Z().VnS());
        orr(VTMP1.Z().VnS(), ComparePred.Merging(), VTMP1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        fcmge(ComparePred.VnD(), Mask, Vector2.Z().VnD(), Vector1.Z().VnD());
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector2.Z().VnD());
        orr(VTMP1.Z().VnD(), ComparePred.Merging(), VTMP1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fcmge(Dst.H(), Vector2.H(), Vector1.H());
          break;
        }
        case 4: {
          fcmge(Dst.S(), Vector2.S(), Vector1.S());
          break;
        }
        case 8: {
          fcmge(Dst.D(), Vector2.D(), Vector1.D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fcmge(Dst.V8H(), Vector2.V8H(), Vector1.V8H());
          break;
        }
        case 4: {
          fcmge(Dst.V4S(), Vector2.V4S(), Vector1.V4S());
          break;
        }
        case 8: {
          fcmge(Dst.V2D(), Vector2.V2D(), Vector1.V2D());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFCMPORD) {
  const auto Op = IROp->C<IR::IROp_VFCMPORD>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure there's no junk in the temporary.
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    // The idea is like comparing for unordered, but we just
    // invert the predicate from the comparison to instead
    // select all ordered elements in the vector.

    switch (ElementSize) {
      case 2: {
        fcmuo(ComparePred.VnH(), Mask, Vector1.Z().VnH(), Vector2.Z().VnH());
        not_(ComparePred.VnB(), Mask, ComparePred.VnB());
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector1.Z().VnH());
        orr(VTMP1.Z().VnH(), ComparePred.Merging(), VTMP1.Z().VnH(), Vector1.Z().VnH());
        break;
      }
      case 4: {
        fcmuo(ComparePred.VnS(), Mask, Vector1.Z().VnS(), Vector2.Z().VnS());
        not_(ComparePred.VnB(), Mask, ComparePred.VnB());
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector1.Z().VnS());
        orr(VTMP1.Z().VnS(), ComparePred.Merging(), VTMP1.Z().VnS(), Vector1.Z().VnS());
        break;
      }
      case 8: {
        fcmuo(ComparePred.VnD(), Mask, Vector1.Z().VnD(), Vector2.Z().VnD());
        not_(ComparePred.VnB(), Mask, ComparePred.VnB());
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector1.Z().VnD());
        orr(VTMP1.Z().VnD(), ComparePred.Merging(), VTMP1.Z().VnD(), Vector1.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fcmge(VTMP1.H(), Vector1.H(), Vector2.H());
          fcmgt(VTMP2.H(), Vector2.H(), Vector1.H());
          orr(Dst.V8B(), VTMP1.V8B(), VTMP2.V8B());
          break;
        }
        case 4: {
          fcmge(VTMP1.S(), Vector1.S(), Vector2.S());
          fcmgt(VTMP2.S(), Vector2.S(), Vector1.S());
          orr(Dst.V8B(), VTMP1.V8B(), VTMP2.V8B());
          break;
        }
        case 8: {
          fcmge(VTMP1.D(), Vector1.D(), Vector2.D());
          fcmgt(VTMP2.D(), Vector2.D(), Vector1.D());
          orr(Dst.V8B(), VTMP1.V8B(), VTMP2.V8B());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fcmge(VTMP1.V8H(), Vector1.V8H(), Vector2.V8H());
          fcmgt(VTMP2.V8H(), Vector2.V8H(), Vector1.V8H());
          orr(Dst.V16B(), VTMP1.V16B(), VTMP2.V16B());
          break;
        }
        case 4: {
          fcmge(VTMP1.V4S(), Vector1.V4S(), Vector2.V4S());
          fcmgt(VTMP2.V4S(), Vector2.V4S(), Vector1.V4S());
          orr(Dst.V16B(), VTMP1.V16B(), VTMP2.V16B());
          break;
        }
        case 8: {
          fcmge(VTMP1.V2D(), Vector1.V2D(), Vector2.V2D());
          fcmgt(VTMP2.V2D(), Vector2.V2D(), Vector1.V2D());
          orr(Dst.V16B(), VTMP1.V16B(), VTMP2.V16B());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VFCMPUNO) {
  const auto Op = IROp->C<IR::IROp_VFCMPUNO>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto IsScalar = ElementSize == OpSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit && !IsScalar) {
    const auto Mask = PRED_TMP_32B.Zeroing();
    const auto ComparePred = p0;

    // Ensure there's no junk in the temporary.
    eor(VTMP1.Z().VnD(), VTMP1.Z().VnD(), VTMP1.Z().VnD());

    switch (ElementSize) {
      case 2: {
        fcmuo(ComparePred.VnH(), Mask, Vector1.Z().VnH(), Vector2.Z().VnH());
        not_(VTMP1.Z().VnH(), ComparePred.Merging(), Vector1.Z().VnH());
        orr(VTMP1.Z().VnH(), ComparePred.Merging(), VTMP1.Z().VnH(), Vector1.Z().VnH());
        break;
      }
      case 4: {
        fcmuo(ComparePred.VnS(), Mask, Vector1.Z().VnS(), Vector2.Z().VnS());
        not_(VTMP1.Z().VnS(), ComparePred.Merging(), Vector1.Z().VnS());
        orr(VTMP1.Z().VnS(), ComparePred.Merging(), VTMP1.Z().VnS(), Vector1.Z().VnS());
        break;
      }
      case 8: {
        fcmuo(ComparePred.VnD(), Mask, Vector1.Z().VnD(), Vector2.Z().VnD());
        not_(VTMP1.Z().VnD(), ComparePred.Merging(), Vector1.Z().VnD());
        orr(VTMP1.Z().VnD(), ComparePred.Merging(), VTMP1.Z().VnD(), Vector1.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    if (IsScalar) {
      switch (ElementSize) {
        case 2: {
          fcmge(VTMP1.H(), Vector1.H(), Vector2.H());
          fcmgt(VTMP2.H(), Vector2.H(), Vector1.H());
          orr(Dst.V8B(), VTMP1.V8B(), VTMP2.V8B());
          mvn(Dst.V8B(), Dst.V8B());
          break;
        }
        case 4: {
          fcmge(VTMP1.S(), Vector1.S(), Vector2.S());
          fcmgt(VTMP2.S(), Vector2.S(), Vector1.S());
          orr(Dst.V8B(), VTMP1.V8B(), VTMP2.V8B());
          mvn(Dst.V8B(), Dst.V8B());
          break;
        }
        case 8: {
          fcmge(VTMP1.D(), Vector1.D(), Vector2.D());
          fcmgt(VTMP2.D(), Vector2.D(), Vector1.D());
          orr(Dst.V8B(), VTMP1.V8B(), VTMP2.V8B());
          mvn(Dst.V8B(), Dst.V8B());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 2: {
          fcmge(VTMP1.V8H(), Vector1.V8H(), Vector2.V8H());
          fcmgt(VTMP2.V8H(), Vector2.V8H(), Vector1.V8H());
          orr(Dst.V16B(), VTMP1.V16B(), VTMP2.V16B());
          mvn(Dst.V16B(), Dst.V16B());
          break;
        }
        case 4: {
          fcmge(VTMP1.V4S(), Vector1.V4S(), Vector2.V4S());
          fcmgt(VTMP2.V4S(), Vector2.V4S(), Vector1.V4S());
          orr(Dst.V16B(), VTMP1.V16B(), VTMP2.V16B());
          mvn(Dst.V16B(), Dst.V16B());
          break;
        }
        case 8: {
          fcmge(VTMP1.V2D(), Vector1.V2D(), Vector2.V2D());
          fcmgt(VTMP2.V2D(), Vector2.V2D(), Vector1.V2D());
          orr(Dst.V16B(), VTMP1.V16B(), VTMP2.V16B());
          mvn(Dst.V16B(), Dst.V16B());
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VUShl) {
  LOGMAN_MSG_A_FMT("Unimplemented");
}

DEF_OP(VUShr) {
  LOGMAN_MSG_A_FMT("Unimplemented");
}

DEF_OP(VSShr) {
  LOGMAN_MSG_A_FMT("Unimplemented");
}

DEF_OP(VUShlS) {
  const auto Op = IROp->C<IR::IROp_VUShlS>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto ShiftScalar = GetVReg(Op->ShiftScalar.ID());
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    const auto Mask = PRED_TMP_32B.Merging();

    // NOTE: SVE LSL is a destructive operation.

    switch (ElementSize) {
      case 1: {
        dup(VTMP1.Z().VnB(), ShiftScalar.Z().VnB(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        lsl(Dst.Z().VnB(), Mask, Dst.Z().VnB(), VTMP1.Z().VnB());
        break;
      }
      case 2: {
        dup(VTMP1.Z().VnH(), ShiftScalar.Z().VnH(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        lsl(Dst.Z().VnH(), Mask, Dst.Z().VnH(), VTMP1.Z().VnH());
        break;
      }
      case 4: {
        dup(VTMP1.Z().VnS(), ShiftScalar.Z().VnS(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        lsl(Dst.Z().VnS(), Mask, Dst.Z().VnS(), VTMP1.Z().VnS());
        break;
      }
      case 8: {
        dup(VTMP1.Z().VnD(), ShiftScalar.Z().VnD(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        lsl(Dst.Z().VnD(), Mask, Dst.Z().VnD(), VTMP1.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 1: {
        dup(VTMP1.V16B(), ShiftScalar.V16B(), 0);
        ushl(Dst.V16B(), Vector.V16B(), VTMP1.V16B());
        break;
      }
      case 2: {
        dup(VTMP1.V8H(), ShiftScalar.V8H(), 0);
        ushl(Dst.V8H(), Vector.V8H(), VTMP1.V8H());
        break;
      }
      case 4: {
        dup(VTMP1.V4S(), ShiftScalar.V4S(), 0);
        ushl(Dst.V4S(), Vector.V4S(), VTMP1.V4S());
        break;
      }
      case 8: {
        dup(VTMP1.V2D(), ShiftScalar.V2D(), 0);
        ushl(Dst.V2D(), Vector.V2D(), VTMP1.V2D());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VUShrS) {
  const auto Op = IROp->C<IR::IROp_VUShrS>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto ShiftScalar = GetVReg(Op->ShiftScalar.ID());
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    const auto Mask = PRED_TMP_32B.Merging();

    // NOTE: SVE LSR is a destructive operation.

    switch (ElementSize) {
      case 1: {
        dup(VTMP1.Z().VnB(), ShiftScalar.Z().VnB(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        lsr(Dst.Z().VnB(), Mask, Dst.Z().VnB(), VTMP1.Z().VnB());
        break;
      }
      case 2: {
        dup(VTMP1.Z().VnH(), ShiftScalar.Z().VnH(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        lsr(Dst.Z().VnH(), Mask, Dst.Z().VnH(), VTMP1.Z().VnH());
        break;
      }
      case 4: {
        dup(VTMP1.Z().VnS(), ShiftScalar.Z().VnS(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        lsr(Dst.Z().VnS(), Mask, Dst.Z().VnS(), VTMP1.Z().VnS());
        break;
      }
      case 8: {
        dup(VTMP1.Z().VnD(), ShiftScalar.Z().VnD(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        lsr(Dst.Z().VnD(), Mask, Dst.Z().VnD(), VTMP1.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 1: {
        dup(VTMP1.V16B(), ShiftScalar.V16B(), 0);
        neg(VTMP1.V16B(), VTMP1.V16B());
        ushl(Dst.V16B(), Vector.V16B(), VTMP1.V16B());
        break;
      }
      case 2: {
        dup(VTMP1.V8H(), ShiftScalar.V8H(), 0);
        neg(VTMP1.V8H(), VTMP1.V8H());
        ushl(Dst.V8H(), Vector.V8H(), VTMP1.V8H());
        break;
      }
      case 4: {
        dup(VTMP1.V4S(), ShiftScalar.V4S(), 0);
        neg(VTMP1.V4S(), VTMP1.V4S());
        ushl(Dst.V4S(), Vector.V4S(), VTMP1.V4S());
        break;
      }
      case 8: {
        dup(VTMP1.V2D(), ShiftScalar.V2D(), 0);
        neg(VTMP1.V2D(), VTMP1.V2D());
        ushl(Dst.V2D(), Vector.V2D(), VTMP1.V2D());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VSShrS) {
  const auto Op = IROp->C<IR::IROp_VSShrS>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto ShiftScalar = GetVReg(Op->ShiftScalar.ID());
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    const auto Mask = PRED_TMP_32B.Merging();

    // NOTE: SVE ASR is a destructive operation.

    switch (ElementSize) {
      case 1: {
        dup(VTMP1.Z().VnB(), ShiftScalar.Z().VnB(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        asr(Dst.Z().VnB(), Mask, Dst.Z().VnB(), VTMP1.Z().VnB());
        break;
      }
      case 2: {
        dup(VTMP1.Z().VnH(), ShiftScalar.Z().VnH(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        asr(Dst.Z().VnH(), Mask, Dst.Z().VnH(), VTMP1.Z().VnH());
        break;
      }
      case 4: {
        dup(VTMP1.Z().VnS(), ShiftScalar.Z().VnS(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        asr(Dst.Z().VnS(), Mask, Dst.Z().VnS(), VTMP1.Z().VnS());
        break;
      }
      case 8: {
        dup(VTMP1.Z().VnD(), ShiftScalar.Z().VnD(), 0);
        movprfx(Dst.Z().VnD(), Vector.Z().VnD());
        asr(Dst.Z().VnD(), Mask, Dst.Z().VnD(), VTMP1.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 1: {
        dup(VTMP1.V16B(), ShiftScalar.V16B(), 0);
        neg(VTMP1.V16B(), VTMP1.V16B());
        sshl(Dst.V16B(), Vector.V16B(), VTMP1.V16B());
        break;
      }
      case 2: {
        dup(VTMP1.V8H(), ShiftScalar.V8H(), 0);
        neg(VTMP1.V8H(), VTMP1.V8H());
        sshl(Dst.V8H(), Vector.V8H(), VTMP1.V8H());
        break;
      }
      case 4: {
        dup(VTMP1.V4S(), ShiftScalar.V4S(), 0);
        neg(VTMP1.V4S(), VTMP1.V4S());
        sshl(Dst.V4S(), Vector.V4S(), VTMP1.V4S());
        break;
      }
      case 8: {
        dup(VTMP1.V2D(), ShiftScalar.V2D(), 0);
        neg(VTMP1.V2D(), VTMP1.V2D());
        sshl(Dst.V2D(), Vector.V2D(), VTMP1.V2D());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VInsElement) {
  const auto Op = IROp->C<IR::IROp_VInsElement>();
  const auto OpSize = IROp->Size;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto ElementSize = Op->Header.ElementSize;

  const auto DestIdx = Op->DestIdx;
  const auto SrcIdx = Op->SrcIdx;

  const auto Dst = GetVReg(Node);
  const auto SrcVector = GetVReg(Op->SrcVector.ID());
  auto Reg = GetVReg(Op->DestVector.ID());

  if (HostSupportsSVE && Is256Bit) {
    // We're going to use this to create our predicate register literal.
    // On an SVE 256-bit capable system, the predicate register will be
    // 32-bit in size. We want to set up only the element corresponding
    // to the destination index, since we're going to copy over the equivalent
    // indexed element from the source vector.
    auto Data = [ElementSize, DestIdx] {
      using LiteralType = aarch64::Literal<uint32_t>;

      switch (ElementSize) {
        case 1:
          LOGMAN_THROW_AA_FMT(DestIdx <= 31, "DestIdx out of range: {}", DestIdx);
          return LiteralType{1U << DestIdx};
        case 2:
          LOGMAN_THROW_AA_FMT(DestIdx <= 15, "DestIdx out of range: {}", DestIdx);
          return LiteralType{1U << (DestIdx * 2)};
        case 4:
          LOGMAN_THROW_AA_FMT(DestIdx <= 7, "DestIdx out of range: {}", DestIdx);
          return LiteralType{1U << (DestIdx * 4)};
        case 8:
          LOGMAN_THROW_AA_FMT(DestIdx <= 3, "DestIdx out of range: {}", DestIdx);
          return LiteralType{1U << (DestIdx * 8)};
        case 16:
          LOGMAN_THROW_AA_FMT(DestIdx <= 1, "DestIdx out of range: {}", DestIdx);
          // Predicates can't be subdivided into the Q format, so we can just set up
          // the predicate to select the two adjacent doublewords.
          return LiteralType{0x101U << (DestIdx * 16)};
        default:
          return LiteralType{UINT32_MAX};
      }
    }();

    // Load our predicate register.
    const auto Predicate = p0;
    aarch64::Label DataLocation;
    adr(TMP1, &DataLocation);
    ldr(Predicate, SVEMemOperand(TMP1));

    // Broadcast our source value across a temporary,
    // then combine with the destination.
    switch (ElementSize) {
      case 1: {
        LOGMAN_THROW_AA_FMT(SrcIdx <= 31, "SrcIdx out of range: {}", SrcIdx);
        dup(VTMP2.Z().VnB(), SrcVector.Z().VnB(), SrcIdx);
        mov(Dst.Z().VnD(), Reg.Z().VnD());
        mov(Dst.Z().VnB(), Predicate.Merging(), VTMP2.Z().VnB());
        break;
      }
      case 2: {
        LOGMAN_THROW_AA_FMT(SrcIdx <= 15, "SrcIdx out of range: {}", SrcIdx);
        dup(VTMP2.Z().VnH(), SrcVector.Z().VnH(), SrcIdx);
        mov(Dst.Z().VnD(), Reg.Z().VnD());
        mov(Dst.Z().VnH(), Predicate.Merging(), VTMP2.Z().VnH());
        break;
      }
      case 4: {
        LOGMAN_THROW_AA_FMT(SrcIdx <= 7, "SrcIdx out of range: {}", SrcIdx);
        dup(VTMP2.Z().VnS(), SrcVector.Z().VnS(), SrcIdx);
        mov(Dst.Z().VnD(), Reg.Z().VnD());
        mov(Dst.Z().VnS(), Predicate.Merging(), VTMP2.Z().VnS());
        break;
      }
      case 8: {
        LOGMAN_THROW_AA_FMT(SrcIdx <= 3, "SrcIdx out of range: {}", SrcIdx);
        dup(VTMP2.Z().VnD(), SrcVector.Z().VnD(), SrcIdx);
        mov(Dst.Z().VnD(), Reg.Z().VnD());
        mov(Dst.Z().VnD(), Predicate.Merging(), VTMP2.Z().VnD());
        break;
      case 16:
        LOGMAN_THROW_AA_FMT(SrcIdx <= 1, "SrcIdx out of range: {}", SrcIdx);
        dup(VTMP2.Z().VnQ(), SrcVector.Z().VnQ(), SrcIdx);
        mov(Dst.Z().VnD(), Reg.Z().VnD());
        mov(Dst.Z().VnD(), Predicate.Merging(), VTMP2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    // Set up a label to jump over the data we inserted, so we don't try and execute it.
    aarch64::Label PastConstant;
    b(&PastConstant);
    bind(&DataLocation);
    place(&Data);
    bind(&PastConstant);
  } else {
    if (Dst.GetCode() != Reg.GetCode()) {
      mov(VTMP1, Reg);
      Reg = VTMP1;
    }

    switch (ElementSize) {
      case 1: {
        mov(Reg.V16B(), DestIdx, SrcVector.V16B(), SrcIdx);
        break;
      }
      case 2: {
        mov(Reg.V8H(), DestIdx, SrcVector.V8H(), SrcIdx);
        break;
      }
      case 4: {
        mov(Reg.V4S(), DestIdx, SrcVector.V4S(), SrcIdx);
        break;
      }
      case 8: {
        mov(Reg.V2D(), DestIdx, SrcVector.V2D(), SrcIdx);
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    if (Dst.GetCode() != Reg.GetCode()) {
      mov(Dst, Reg);
    }
  }
}

DEF_OP(VDupElement) {
  const auto Op = IROp->C<IR::IROp_VDupElement>();
  const auto OpSize = IROp->Size;

  const auto Index = Op->Index;
  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 1:
        dup(Dst.Z().VnB(), Vector.Z().VnB(), Index);
        break;
      case 2:
        dup(Dst.Z().VnH(), Vector.Z().VnH(), Index);
        break;
      case 4:
        dup(Dst.Z().VnS(), Vector.Z().VnS(), Index);
        break;
      case 8:
        dup(Dst.Z().VnD(), Vector.Z().VnD(), Index);
        break;
      case 16:
        dup(Dst.Z().VnQ(), Vector.Z().VnQ(), Index);
        break;
      default:
        LOGMAN_MSG_A_FMT("Unhandled VDupElement element size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 1:
        dup(Dst.V16B(), Vector.V16B(), Index);
        break;
      case 2:
        dup(Dst.V8H(), Vector.V8H(), Index);
        break;
      case 4:
        dup(Dst.V4S(), Vector.V4S(), Index);
        break;
      case 8:
        dup(Dst.V2D(), Vector.V2D(), Index);
        break;
      default: 
        LOGMAN_MSG_A_FMT("Unhandled VDupElement element size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VExtr) {
  const auto Op = IROp->C<IR::IROp_VExtr>();
  const auto OpSize = IROp->Size;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  // AArch64 ext op has bit arrangement as [Vm:Vn] so arguments need to be swapped
  const auto Dst = GetVReg(Node);
  auto UpperBits = GetVReg(Op->VectorLower.ID());
  auto LowerBits = GetVReg(Op->VectorUpper.ID());

  const auto ElementSize = Op->Header.ElementSize;
  auto Index = Op->Index;

  if (Index >= OpSize) {
    // Upper bits have moved in to the lower bits
    LowerBits = UpperBits;

    // Upper bits are all now zero
    UpperBits = VTMP1;
    eor(VTMP1.V16B(), VTMP1.V16B(), VTMP1.V16B());
    Index -= OpSize;
  }

  const auto CopyFromByte = Index * ElementSize;

  if (HostSupportsSVE && Is256Bit) {
    movprfx(VTMP2.Z().VnD(), LowerBits.Z().VnD());
    ext(VTMP2.Z().VnB(), VTMP2.Z().VnB(), UpperBits.Z().VnB(), CopyFromByte);
    mov(Dst.Z().VnD(), VTMP2.Z().VnD());
  } else {
    if (OpSize == 8) {
      ext(Dst.V8B(), LowerBits.V8B(), UpperBits.V8B(), CopyFromByte);
    } else {
      ext(Dst.V16B(), LowerBits.V16B(), UpperBits.V16B(), CopyFromByte);
    }
  }
}

DEF_OP(VUShrI) {
  const auto Op = IROp->C<IR::IROp_VUShrI>();
  const auto OpSize = IROp->Size;

  const auto BitShift = Op->BitShift;
  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (BitShift >= (ElementSize * 8)) {
    eor(Dst.V16B(), Dst.V16B(), Dst.V16B());
  } else {
    if (HostSupportsSVE && Is256Bit) {
      const auto Mask = PRED_TMP_32B.Merging();

      // SVE LSR is destructive, so lets set up the destination.
      movprfx(Dst.Z().VnD(), Vector.Z().VnD());

      switch (ElementSize) {
        case 1: {
          lsr(Dst.Z().VnB(), Mask, Dst.Z().VnB(), BitShift);
          break;
        }
        case 2: {
          lsr(Dst.Z().VnH(), Mask, Dst.Z().VnH(), BitShift);
          break;
        }
        case 4: {
          lsr(Dst.Z().VnS(), Mask, Dst.Z().VnS(), BitShift);
          break;
        }
        case 8: {
          lsr(Dst.Z().VnD(), Mask, Dst.Z().VnD(), BitShift);
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 1: {
          ushr(Dst.V16B(), Vector.V16B(), BitShift);
          break;
        }
        case 2: {
          ushr(Dst.V8H(), Vector.V8H(), BitShift);
          break;
        }
        case 4: {
          ushr(Dst.V4S(), Vector.V4S(), BitShift);
          break;
        }
        case 8: {
          ushr(Dst.V2D(), Vector.V2D(), BitShift);
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VSShrI) {
  const auto Op = IROp->C<IR::IROp_VSShrI>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Shift = std::min(uint8_t(ElementSize * 8 - 1), Op->BitShift);
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    const auto Mask = PRED_TMP_32B.Merging();

    // SVE ASR is destructive, so lets set up the destination.
    movprfx(Dst.Z().VnD(), Vector.Z().VnD());

    switch (ElementSize) {
      case 1: {
        asr(Dst.Z().VnB(), Mask, Dst.Z().VnB(), Shift);
        break;
      }
      case 2: {
        asr(Dst.Z().VnH(), Mask, Dst.Z().VnH(), Shift);
        break;
      }
      case 4: {
        asr(Dst.Z().VnS(), Mask, Dst.Z().VnS(), Shift);
        break;
      }
      case 8: {
        asr(Dst.Z().VnD(), Mask, Dst.Z().VnD(), Shift);
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 1: {
        sshr(Dst.V16B(), Vector.V16B(), Shift);
        break;
      }
      case 2: {
        sshr(Dst.V8H(), Vector.V8H(), Shift);
        break;
      }
      case 4: {
        sshr(Dst.V4S(), Vector.V4S(), Shift);
        break;
      }
      case 8: {
        sshr(Dst.V2D(), Vector.V2D(), Shift);
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VShlI) {
  const auto Op = IROp->C<IR::IROp_VShlI>();
  const auto OpSize = IROp->Size;

  const auto BitShift = Op->BitShift;
  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (BitShift >= (ElementSize * 8)) {
    eor(Dst.V16B(), Dst.V16B(), Dst.V16B());
  } else {
    if (HostSupportsSVE && Is256Bit) {
      const auto Mask = PRED_TMP_32B.Merging();

      // SVE LSL is destructive, so lets set up the destination.
      movprfx(Dst.Z().VnD(), Vector.Z().VnD());

      switch (ElementSize) {
        case 1: {
          lsl(Dst.Z().VnB(), Mask, Dst.Z().VnB(), BitShift);
          break;
        }
        case 2: {
          lsl(Dst.Z().VnH(), Mask, Dst.Z().VnH(), BitShift);
          break;
        }
        case 4: {
          lsl(Dst.Z().VnS(), Mask, Dst.Z().VnS(), BitShift);
          break;
        }
        case 8: {
          lsl(Dst.Z().VnD(), Mask, Dst.Z().VnD(), BitShift);
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    } else {
      switch (ElementSize) {
        case 1: {
          shl(Dst.V16B(), Vector.V16B(), BitShift);
          break;
        }
        case 2: {
          shl(Dst.V8H(), Vector.V8H(), BitShift);
          break;
        }
        case 4: {
          shl(Dst.V4S(), Vector.V4S(), BitShift);
          break;
        }
        case 8: {
          shl(Dst.V2D(), Vector.V2D(), BitShift);
          break;
        }
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          break;
      }
    }
  }
}

DEF_OP(VUShrNI) {
  const auto Op = IROp->C<IR::IROp_VUShrNI>();
  const auto OpSize = IROp->Size;

  const auto BitShift = Op->BitShift;
  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 1: {
        shrnb(Dst.Z().VnB(), Vector.Z().VnH(), BitShift);
        uzp1(Dst.Z().VnB(), Dst.Z().VnB(), Dst.Z().VnB());
        break;
      }
      case 2: {
        shrnb(Dst.Z().VnH(), Vector.Z().VnS(), BitShift);
        uzp1(Dst.Z().VnH(), Dst.Z().VnH(), Dst.Z().VnH());
        break;
      }
      case 4: {
        shrnb(Dst.Z().VnS(), Vector.Z().VnD(), BitShift);
        uzp1(Dst.Z().VnS(), Dst.Z().VnS(), Dst.Z().VnS());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 1: {
        shrn(Dst.V8B(), Vector.V8H(), BitShift);
        break;
      }
      case 2: {
        shrn(Dst.V4H(), Vector.V4S(), BitShift);
        break;
      }
      case 4: {
        shrn(Dst.V2S(), Vector.V2D(), BitShift);
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VUShrNI2) {
  const auto Op = IROp->C<IR::IROp_VUShrNI2>();
  const auto OpSize = IROp->Size;

  const auto BitShift = Op->BitShift;
  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto VectorLower = GetVReg(Op->VectorLower.ID());
  const auto VectorUpper = GetVReg(Op->VectorUpper.ID());

  if (HostSupportsSVE && Is256Bit) {
    mov(VTMP1.Z().VnD(), VectorLower.Z().VnD());

    const auto Mask = PRED_TMP_16B;

    switch (ElementSize) {
      case 1: {
        shrnb(VTMP2.Z().VnB(), VectorUpper.Z().VnH(), BitShift);
        uzp1(VTMP2.Z().VnB(), VTMP2.Z().VnB(), VTMP2.Z().VnB());
        splice(VTMP1.Z().VnB(), Mask, VTMP1.Z().VnB(), VTMP2.Z().VnB());
        break;
      }
      case 2: {
        shrnb(VTMP2.Z().VnH(), VectorUpper.Z().VnS(), BitShift);
        uzp1(VTMP2.Z().VnH(), VTMP2.Z().VnH(), VTMP2.Z().VnH());
        splice(VTMP1.Z().VnH(), Mask, VTMP1.Z().VnH(), VTMP2.Z().VnH());
        break;
      }
      case 4: {
        shrnb(VTMP2.Z().VnS(), VectorUpper.Z().VnD(), BitShift);
        uzp1(VTMP2.Z().VnS(), VTMP2.Z().VnS(), VTMP2.Z().VnS());
        splice(VTMP1.Z().VnS(), Mask, VTMP1.Z().VnS(), VTMP2.Z().VnS());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    mov(VTMP1, VectorLower);

    switch (ElementSize) {
      case 1: {
        shrn2(VTMP1.V16B(), VectorUpper.V8H(), BitShift);
        break;
      }
      case 2: {
        shrn2(VTMP1.V8H(), VectorUpper.V4S(), BitShift);
        break;
      }
      case 4: {
        shrn2(VTMP1.V4S(), VectorUpper.V2D(), BitShift);
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst, VTMP1);
  }
}

DEF_OP(VSXTL) {
  const auto Op = IROp->C<IR::IROp_VSXTL>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 2:
        sunpklo(Dst.Z().VnH(), Vector.Z().VnB());
        break;
      case 4:
        sunpklo(Dst.Z().VnS(), Vector.Z().VnH());
        break;
      case 8:
        sunpklo(Dst.Z().VnD(), Vector.Z().VnS());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 2:
        sxtl(Dst.V8H(), Vector.V8B());
        break;
      case 4:
        sxtl(Dst.V4S(), Vector.V4H());
        break;
      case 8:
        sxtl(Dst.V2D(), Vector.V2S());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VSXTL2) {
  const auto Op = IROp->C<IR::IROp_VSXTL2>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 2:
        sunpkhi(Dst.Z().VnH(), Vector.Z().VnB());
        break;
      case 4:
        sunpkhi(Dst.Z().VnS(), Vector.Z().VnH());
        break;
      case 8:
        sunpkhi(Dst.Z().VnD(), Vector.Z().VnS());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 2:
        sxtl2(Dst.V8H(), Vector.V16B());
        break;
      case 4:
        sxtl2(Dst.V4S(), Vector.V8H());
        break;
      case 8:
        sxtl2(Dst.V2D(), Vector.V4S());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VUXTL) {
  const auto Op = IROp->C<IR::IROp_VUXTL>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 2:
        uunpklo(Dst.Z().VnH(), Vector.Z().VnB());
        break;
      case 4:
        uunpklo(Dst.Z().VnS(), Vector.Z().VnH());
        break;
      case 8:
        uunpklo(Dst.Z().VnD(), Vector.Z().VnS());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 2:
        uxtl(Dst.V8H(), Vector.V8B());
        break;
      case 4:
        uxtl(Dst.V4S(), Vector.V4H());
        break;
      case 8:
        uxtl(Dst.V2D(), Vector.V2S());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VUXTL2) {
  const auto Op = IROp->C<IR::IROp_VUXTL2>();

  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 2:
        uunpkhi(Dst.Z().VnH(), Vector.Z().VnB());
        break;
      case 4:
        uunpkhi(Dst.Z().VnS(), Vector.Z().VnH());
        break;
      case 8:
        uunpkhi(Dst.Z().VnD(), Vector.Z().VnS());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 2:
        uxtl2(Dst.V8H(), Vector.V16B());
        break;
      case 4:
        uxtl2(Dst.V4S(), Vector.V8H());
        break;
      case 8:
        uxtl2(Dst.V2D(), Vector.V4S());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VSQXTN) {
  const auto Op = IROp->C<IR::IROp_VSQXTN>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    // Note that SVE SQXTNB and SQXTNT are a tad different
    // in behavior compared to most other [name]B and [name]T
    // instructions.
    //
    // Most other bottom and top instructions operate
    // on even (bottom) or odd (top) elements and store each
    // result into the next subsequent element in the destination
    // vector
    //
    // SQXTNB and SQXTNT will operate on the same elements regardless
    // of which one is chosen, but will instead place results from
    // the operation into either each subsequent even (bottom) element
    // or odd (top) element. However the bottom instruction will zero the
    // odd elements out in the destination vector, while the top instruction
    // will leave the even elements alone (in a behavior similar to Adv.SIMD's
    // SQXTN/SQXTN2 instructions).
    //
    // e.g. consider this 64-bit (for brevity) vector with four 16-bit elements:
    //
    // ╔═══════════╗╔═══════════╗╔═══════════╗╔═══════════╗
    // ║  Value 3  ║║  Value 2  ║║  Value 1  ║║  Value 0  ║ 
    // ╚═══════════╝╚═══════════╝╚═══════════╝╚═══════════╝
    //
    // SQXTNB Dst.VnB, Src.VnH will result in:
    //
    // ╔═════╗╔═════╗╔═════╗╔═════╗╔═════╗╔═════╗╔═════╗╔═════╗
    // ║  0  ║║ V3  ║║  0  ║║ V2  ║║  0  ║║ V1  ║║  0  ║║ V0  ║ 
    // ╚═════╝╚═════╝╚═════╝╚═════╝╚═════╝╚═════╝╚═════╝╚═════╝
    //
    // This is kind of convenient, considering we only need
    // to use the bottom variant and then concatenate all the
    // even elements with SVE UZP1.

    switch (ElementSize) {
      case 1:
        sqxtnb(Dst.Z().VnB(), Vector.Z().VnH());
        uzp1(Dst.Z().VnB(), Dst.Z().VnB(), Dst.Z().VnB());
        break;
      case 2:
        sqxtnb(Dst.Z().VnH(), Vector.Z().VnS());
        uzp1(Dst.Z().VnH(), Dst.Z().VnH(), Dst.Z().VnH());
        break;
      case 4:
        sqxtnb(Dst.Z().VnS(), Vector.Z().VnD());
        uzp1(Dst.Z().VnS(), Dst.Z().VnS(), Dst.Z().VnS());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 1:
        sqxtn(Dst.V8B(), Vector.V8H());
        break;
      case 2:
        sqxtn(Dst.V4H(), Vector.V4S());
        break;
      case 4:
        sqxtn(Dst.V2S(), Vector.V2D());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VSQXTN2) {
  const auto Op = IROp->C<IR::IROp_VSQXTN2>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto VectorLower = GetVReg(Op->VectorLower.ID());
  const auto VectorUpper = GetVReg(Op->VectorUpper.ID());

  if (HostSupportsSVE && Is256Bit) {
    // Need to use the destructive variant of SPLICE, since
    // the constructive variant requires a register list, and
    // we can't guarantee VectorLower and VectorUpper will always
    // have consecutive indexes with one another.
    mov(VTMP1.Z().VnD(), VectorLower.Z().VnD());

    // We use the 16 byte mask due to how SPLICE works. We only
    // want to get at the first 16 bytes in the lower vector, so
    // that SPLICE will then begin copying the first 16 bytes
    // from the upper vector and begin placing them after the
    // previously copied lower 16 bytes.
    const auto Mask = PRED_TMP_16B;

    switch (ElementSize) {
      case 1:
        sqxtnb(VTMP2.Z().VnB(), VectorUpper.Z().VnH());
        uzp1(VTMP2.Z().VnB(), VTMP2.Z().VnB(), VTMP2.Z().VnB());
        splice(VTMP1.Z().VnB(), Mask, VTMP1.Z().VnB(), VTMP2.Z().VnB());
        break;
      case 2:
        sqxtnb(VTMP2.Z().VnH(), VectorUpper.Z().VnS());
        uzp1(VTMP2.Z().VnH(), VTMP2.Z().VnH(), VTMP2.Z().VnH());
        splice(VTMP1.Z().VnH(), Mask, VTMP1.Z().VnH(), VTMP2.Z().VnH());
        break;
      case 4:
        sqxtnb(VTMP2.Z().VnS(), VectorUpper.Z().VnD());
        uzp1(VTMP2.Z().VnS(), VTMP2.Z().VnS(), VTMP2.Z().VnS());
        splice(VTMP1.Z().VnS(), Mask, VTMP1.Z().VnS(), VTMP2.Z().VnS());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    mov(VTMP1, VectorLower);

    if (OpSize == 8) {
      switch (ElementSize) {
        case 1:
          sqxtn(VTMP2.V8B(), VectorUpper.V8H());
          ins(VTMP1.V4S(), 1, VTMP2.V4S(), 0);
          break;
        case 2:
          sqxtn(VTMP2.V4H(), VectorUpper.V4S());
          ins(VTMP1.V4S(), 1, VTMP2.V4S(), 0);
          break;
        case 4:
          sqxtn(VTMP2.V2S(), VectorUpper.V2D());
          ins(VTMP1.V4S(), 1, VTMP2.V4S(), 0);
          break;
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          return;
      }
    } else {
      switch (ElementSize) {
        case 1:
          sqxtn2(VTMP1.V16B(), VectorUpper.V8H());
          break;
        case 2:
          sqxtn2(VTMP1.V8H(), VectorUpper.V4S());
          break;
        case 4:
          sqxtn2(VTMP1.V4S(), VectorUpper.V2D());
          break;
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          return;
      }
    }

    mov(Dst, VTMP1);
  }
}

DEF_OP(VSQXTUN) {
  const auto Op = IROp->C<IR::IROp_VSQXTUN>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 1:
        sqxtunb(Dst.Z().VnB(), Vector.Z().VnH());
        uzp1(Dst.Z().VnB(), Dst.Z().VnB(), Dst.Z().VnB());
        break;
      case 2:
        sqxtunb(Dst.Z().VnH(), Vector.Z().VnS());
        uzp1(Dst.Z().VnH(), Dst.Z().VnH(), Dst.Z().VnH());
        break;
      case 4:
        sqxtunb(Dst.Z().VnS(), Vector.Z().VnD());
        uzp1(Dst.Z().VnS(), Dst.Z().VnS(), Dst.Z().VnS());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 1:
        sqxtun(Dst.V8B(), Vector.V8H());
        break;
      case 2:
        sqxtun(Dst.V4H(), Vector.V4S());
        break;
      case 4:
        sqxtun(Dst.V2S(), Vector.V2D());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VSQXTUN2) {
  const auto Op = IROp->C<IR::IROp_VSQXTUN2>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto VectorLower = GetVReg(Op->VectorLower.ID());
  const auto VectorUpper = GetVReg(Op->VectorUpper.ID());

  if (HostSupportsSVE && Is256Bit) {
    // NOTE: See VSQXTN2 implementation for an in-depth explanation
    //       of everything going on here.

    mov(VTMP1.Z().VnD(), VectorLower.Z().VnD());

    const auto Mask = PRED_TMP_16B;

    switch (ElementSize) {
      case 1:
        sqxtunb(VTMP2.Z().VnB(), VectorUpper.Z().VnH());
        uzp1(VTMP2.Z().VnB(), VTMP2.Z().VnB(), VTMP2.Z().VnB());
        splice(VTMP1.Z().VnB(), Mask, VTMP1.Z().VnB(), VTMP2.Z().VnB());
        break;
      case 2:
        sqxtunb(VTMP2.Z().VnH(), VectorUpper.Z().VnS());
        uzp1(VTMP2.Z().VnH(), VTMP2.Z().VnH(), VTMP2.Z().VnH());
        splice(VTMP1.Z().VnH(), Mask, VTMP1.Z().VnH(), VTMP2.Z().VnH());
        break;
      case 4:
        sqxtunb(VTMP2.Z().VnS(), VectorUpper.Z().VnD());
        uzp1(VTMP2.Z().VnS(), VTMP2.Z().VnS(), VTMP2.Z().VnS());
        splice(VTMP1.Z().VnS(), Mask, VTMP1.Z().VnS(), VTMP2.Z().VnS());
        break;
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        return;
    }

    mov(Dst.Z().VnD(), VTMP1.Z().VnD());
  } else {
    mov(VTMP1, VectorLower);
    if (OpSize == 8) {
      switch (ElementSize) {
        case 1:
          sqxtun(VTMP2.V8B(), VectorUpper.V8H());
          ins(VTMP1.V4S(), 1, VTMP2.V4S(), 0);
          break;
        case 2:
          sqxtun(VTMP2.V4H(), VectorUpper.V4S());
          ins(VTMP1.V4S(), 1, VTMP2.V4S(), 0);
          break;
        case 4:
          sqxtun(VTMP2.V2S(), VectorUpper.V2D());
          ins(VTMP1.V4S(), 1, VTMP2.V4S(), 0);
          break;
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          return;
      }
    } else {
      switch (ElementSize) {
        case 1:
          sqxtun2(VTMP1.V16B(), VectorUpper.V8H());
          break;
        case 2:
          sqxtun2(VTMP1.V8H(), VectorUpper.V4S());
          break;
        case 4:
          sqxtun2(VTMP1.V4S(), VectorUpper.V2D());
          break;
        default:
          LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
          return;
      }
    }
    mov(Dst, VTMP1);
  }
}

DEF_OP(VMul) {
  const auto Op = IROp->C<IR::IROp_VUMul>();

  const auto ElementSize = Op->Header.ElementSize;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE) {
    switch (ElementSize) {
      case 1: {
        mul(Dst.Z().VnB(), Vector1.Z().VnB(), Vector2.Z().VnB());
        break;
      }
      case 2: {
        mul(Dst.Z().VnH(), Vector1.Z().VnH(), Vector2.Z().VnH());
        break;
      }
      case 4: {
        mul(Dst.Z().VnS(), Vector1.Z().VnS(), Vector2.Z().VnS());
        break;
      }
      case 8: {
        mul(Dst.Z().VnD(), Vector1.Z().VnD(), Vector2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 1: {
        mul(Dst.V16B(), Vector1.V16B(), Vector2.V16B());
        break;
      }
      case 2: {
        mul(Dst.V8H(), Vector1.V8H(), Vector2.V8H());
        break;
      }
      case 4: {
        mul(Dst.V4S(), Vector1.V4S(), Vector2.V4S());
        break;
      }
      case 8: {
        mul(Dst.V2D(), Vector1.V2D(), Vector2.V2D());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize);
        break;
    }
  }
}

DEF_OP(VUMull) {
  const auto Op = IROp->C<IR::IROp_VUMull>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 2: {
        umullb(VTMP1.Z().VnH(), Vector1.Z().VnB(), Vector2.Z().VnB());
        umullt(VTMP2.Z().VnH(), Vector1.Z().VnB(), Vector2.Z().VnB());
        zip1(Dst.Z().VnH(), VTMP1.Z().VnH(), VTMP2.Z().VnH());
        break;
      }
      case 4: {
        umullb(VTMP1.Z().VnS(), Vector1.Z().VnH(), Vector2.Z().VnH());
        umullt(VTMP2.Z().VnS(), Vector1.Z().VnH(), Vector2.Z().VnH());
        zip1(Dst.Z().VnS(), VTMP1.Z().VnS(), VTMP2.Z().VnS());
        break;
      }
      case 8: {
        umullb(VTMP1.Z().VnD(), Vector1.Z().VnS(), Vector2.Z().VnS());
        umullt(VTMP2.Z().VnD(), Vector1.Z().VnS(), Vector2.Z().VnS());
        zip1(Dst.Z().VnD(), VTMP1.Z().VnD(), VTMP2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize >> 1);
        break;
    }
  } else {
    switch (ElementSize) {
      case 2: {
        umull(Dst.V8H(), Vector1.V8B(), Vector2.V8B());
        break;
      }
      case 4: {
        umull(Dst.V4S(), Vector1.V4H(), Vector2.V4H());
        break;
      }
      case 8: {
        umull(Dst.V2D(), Vector1.V2S(), Vector2.V2S());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize >> 1);
        break;
    }
  }
}

DEF_OP(VSMull) {
  const auto Op = IROp->C<IR::IROp_VSMull>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 2: {
        smullb(VTMP1.Z().VnH(), Vector1.Z().VnB(), Vector2.Z().VnB());
        smullt(VTMP2.Z().VnH(), Vector1.Z().VnB(), Vector2.Z().VnB());
        zip1(Dst.Z().VnH(), VTMP1.Z().VnH(), VTMP2.Z().VnH());
        break;
      }
      case 4: {
        smullb(VTMP1.Z().VnS(), Vector1.Z().VnH(), Vector2.Z().VnH());
        smullt(VTMP2.Z().VnS(), Vector1.Z().VnH(), Vector2.Z().VnH());
        zip1(Dst.Z().VnS(), VTMP1.Z().VnS(), VTMP2.Z().VnS());
        break;
      }
      case 8: {
        smullb(VTMP1.Z().VnD(), Vector1.Z().VnS(), Vector2.Z().VnS());
        smullt(VTMP2.Z().VnD(), Vector1.Z().VnS(), Vector2.Z().VnS());
        zip1(Dst.Z().VnD(), VTMP1.Z().VnD(), VTMP2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize >> 1);
        break;
    }
  } else {
    switch (ElementSize) {
      case 2: {
        smull(Dst.V8H(), Vector1.V8B(), Vector2.V8B());
        break;
      }
      case 4: {
        smull(Dst.V4S(), Vector1.V4H(), Vector2.V4H());
        break;
      }
      case 8: {
        smull(Dst.V2D(), Vector1.V2S(), Vector2.V2S());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize >> 1);
        break;
    }
  }
}

DEF_OP(VUMull2) {
  const auto Op = IROp->C<IR::IROp_VUMull2>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 2: {
        umullb(VTMP1.Z().VnH(), Vector1.Z().VnB(), Vector2.Z().VnB());
        umullt(VTMP2.Z().VnH(), Vector1.Z().VnB(), Vector2.Z().VnB());
        zip2(Dst.Z().VnH(), VTMP1.Z().VnH(), VTMP2.Z().VnH());
        break;
      }
      case 4: {
        umullb(VTMP1.Z().VnS(), Vector1.Z().VnH(), Vector2.Z().VnH());
        umullt(VTMP2.Z().VnS(), Vector1.Z().VnH(), Vector2.Z().VnH());
        zip2(Dst.Z().VnS(), VTMP1.Z().VnS(), VTMP2.Z().VnS());
        break;
      }
      case 8: {
        umullb(VTMP1.Z().VnD(), Vector1.Z().VnS(), Vector2.Z().VnS());
        umullt(VTMP2.Z().VnD(), Vector1.Z().VnS(), Vector2.Z().VnS());
        zip2(Dst.Z().VnD(), VTMP1.Z().VnD(), VTMP2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize >> 1);
        break;
    }
  } else {
    switch (ElementSize) {
      case 2: {
        umull2(Dst.V8H(), Vector1.V16B(), Vector2.V16B());
        break;
      }
      case 4: {
        umull2(Dst.V4S(), Vector1.V8H(), Vector2.V8H());
        break;
      }
      case 8: {
        umull2(Dst.V2D(), Vector1.V4S(), Vector2.V4S());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize >> 1);
        break;
    }
  }
}

DEF_OP(VSMull2) {
  const auto Op = IROp->C<IR::IROp_VSMull2>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit) {
    switch (ElementSize) {
      case 2: {
        smullb(VTMP1.Z().VnH(), Vector1.Z().VnB(), Vector2.Z().VnB());
        smullt(VTMP2.Z().VnH(), Vector1.Z().VnB(), Vector2.Z().VnB());
        zip2(Dst.Z().VnH(), VTMP1.Z().VnH(), VTMP2.Z().VnH());
        break;
      }
      case 4: {
        smullb(VTMP1.Z().VnS(), Vector1.Z().VnH(), Vector2.Z().VnH());
        smullt(VTMP2.Z().VnS(), Vector1.Z().VnH(), Vector2.Z().VnH());
        zip2(Dst.Z().VnS(), VTMP1.Z().VnS(), VTMP2.Z().VnS());
        break;
      }
      case 8: {
        smullb(VTMP1.Z().VnD(), Vector1.Z().VnS(), Vector2.Z().VnS());
        smullt(VTMP2.Z().VnD(), Vector1.Z().VnS(), Vector2.Z().VnS());
        zip2(Dst.Z().VnD(), VTMP1.Z().VnD(), VTMP2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize >> 1);
        break;
    }
  } else {
    switch (ElementSize) {
      case 2: {
        smull2(Dst.V8H(), Vector1.V16B(), Vector2.V16B());
        break;
      }
      case 4: {
        smull2(Dst.V4S(), Vector1.V8H(), Vector2.V8H());
        break;
      }
      case 8: {
        smull2(Dst.V2D(), Vector1.V4S(), Vector2.V4S());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize >> 1);
        break;
    }
  }
}

DEF_OP(VUABDL) {
  const auto Op = IROp->C<IR::IROp_VUABDL>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector1 = GetVReg(Op->Vector1.ID());
  const auto Vector2 = GetVReg(Op->Vector2.ID());

  if (HostSupportsSVE && Is256Bit) {
    // To mimic the behavior of AdvSIMD UABDL, we need to get the
    // absolute difference of the even elements (UADBLB), get the
    // absolute difference of the odd elemenets (UABDLT), then
    // interleave the results in both vectors together.
    
    switch (ElementSize) {
      case 2: {
        uabdlb(VTMP1.Z().VnH(), Vector1.Z().VnB(), Vector2.Z().VnB());
        uabdlt(VTMP2.Z().VnH(), Vector1.Z().VnB(), Vector2.Z().VnB());
        zip1(Dst.Z().VnH(), VTMP1.Z().VnH(), VTMP2.Z().VnH());
        break;
      }
      case 4: {
        uabdlb(VTMP1.Z().VnS(), Vector1.Z().VnH(), Vector2.Z().VnH());
        uabdlt(VTMP2.Z().VnS(), Vector1.Z().VnH(), Vector2.Z().VnH());
        zip1(Dst.Z().VnS(), VTMP1.Z().VnS(), VTMP2.Z().VnS());
        break;
      }
      case 8: {
        uabdlb(VTMP1.Z().VnD(), Vector1.Z().VnS(), Vector2.Z().VnS());
        uabdlt(VTMP2.Z().VnD(), Vector1.Z().VnS(), Vector2.Z().VnS());
        zip1(Dst.Z().VnD(), VTMP1.Z().VnD(), VTMP2.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize >> 1);
        return;
    }
  } else {
    switch (ElementSize) {
      case 2: {
        uabdl(Dst.V8H(), Vector1.V8B(), Vector2.V8B());
        break;
      }
      case 4: {
        uabdl(Dst.V4S(), Vector1.V4H(), Vector2.V4H());
        break;
      }
      case 8: {
        uabdl(Dst.V2D(), Vector1.V2S(), Vector2.V2S());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Unknown Element Size: {}", ElementSize >> 1);
        break;
    }
  }
}

DEF_OP(VTBL1) {
  const auto Op = IROp->C<IR::IROp_VTBL1>();
  const auto OpSize = IROp->Size;

  const auto Dst = GetVReg(Node);
  const auto VectorIndices = GetVReg(Op->VectorIndices.ID());
  const auto VectorTable = GetVReg(Op->VectorTable.ID());

  switch (OpSize) {
    case 8: {
      tbl(Dst.V8B(), VectorTable.V16B(), VectorIndices.V8B());
      break;
    }
    case 16: {
      tbl(Dst.V16B(), VectorTable.V16B(), VectorIndices.V16B());
      break;
    }
    case 32: {
      LOGMAN_THROW_AA_FMT(HostSupportsSVE,
                          "Host does not support SVE. Cannot perform 256-bit table lookup");

      tbl(Dst.Z().VnB(), VectorTable.Z().VnB(), VectorIndices.Z().VnB());
      break;
    }
    default:
      LOGMAN_MSG_A_FMT("Unknown OpSize: {}", OpSize);
      break;
  }
}

DEF_OP(VRev64) {
  const auto Op = IROp->C<IR::IROp_VRev64>();
  const auto OpSize = IROp->Size;

  const auto ElementSize = Op->Header.ElementSize;
  const auto Elements = OpSize / ElementSize;
  const auto Is256Bit = OpSize == Core::CPUState::XMM_AVX_REG_SIZE;

  const auto Dst = GetVReg(Node);
  const auto Vector = GetVReg(Op->Vector.ID());

  if (HostSupportsSVE && Is256Bit) {
    const auto Mask = PRED_TMP_32B.Merging();

    switch (ElementSize) {
      case 1: {
        revb(Dst.Z().VnD(), Mask, Vector.Z().VnD());
        break;
      }
      case 2: {
        revh(Dst.Z().VnD(), Mask, Vector.Z().VnD());
        break;
      }
      case 4: {
        revw(Dst.Z().VnD(), Mask, Vector.Z().VnD());
        break;
      }
      default:
        LOGMAN_MSG_A_FMT("Invalid Element Size: {}", ElementSize);
        break;
    }
  } else {
    switch (ElementSize) {
      case 1:
      case 2:
      case 4:
        rev64(Dst.VCast(OpSize * 8, Elements), Vector.VCast(OpSize * 8, Elements));
        break;
      default:
        LOGMAN_MSG_A_FMT("Invalid Element Size: {}", ElementSize);
        break;
    }
  }
}

#undef DEF_OP
void Arm64JITCore::RegisterVectorHandlers() {
#define REGISTER_OP(op, x) OpHandlers[FEXCore::IR::IROps::OP_##op] = &Arm64JITCore::Op_##x
  REGISTER_OP(VECTORZERO,        VectorZero);
  REGISTER_OP(VECTORIMM,         VectorImm);
  REGISTER_OP(VMOV,              VMov);
  REGISTER_OP(VAND,              VAnd);
  REGISTER_OP(VBIC,              VBic);
  REGISTER_OP(VOR,               VOr);
  REGISTER_OP(VXOR,              VXor);
  REGISTER_OP(VADD,              VAdd);
  REGISTER_OP(VSUB,              VSub);
  REGISTER_OP(VUQADD,            VUQAdd);
  REGISTER_OP(VUQSUB,            VUQSub);
  REGISTER_OP(VSQADD,            VSQAdd);
  REGISTER_OP(VSQSUB,            VSQSub);
  REGISTER_OP(VADDP,             VAddP);
  REGISTER_OP(VADDV,             VAddV);
  REGISTER_OP(VUMINV,            VUMinV);
  REGISTER_OP(VURAVG,            VURAvg);
  REGISTER_OP(VABS,              VAbs);
  REGISTER_OP(VPOPCOUNT,         VPopcount);
  REGISTER_OP(VFADD,             VFAdd);
  REGISTER_OP(VFADDP,            VFAddP);
  REGISTER_OP(VFSUB,             VFSub);
  REGISTER_OP(VFMUL,             VFMul);
  REGISTER_OP(VFDIV,             VFDiv);
  REGISTER_OP(VFMIN,             VFMin);
  REGISTER_OP(VFMAX,             VFMax);
  REGISTER_OP(VFRECP,            VFRecp);
  REGISTER_OP(VFSQRT,            VFSqrt);
  REGISTER_OP(VFRSQRT,           VFRSqrt);
  REGISTER_OP(VNEG,              VNeg);
  REGISTER_OP(VFNEG,             VFNeg);
  REGISTER_OP(VNOT,              VNot);
  REGISTER_OP(VUMIN,             VUMin);
  REGISTER_OP(VSMIN,             VSMin);
  REGISTER_OP(VUMAX,             VUMax);
  REGISTER_OP(VSMAX,             VSMax);
  REGISTER_OP(VZIP,              VZip);
  REGISTER_OP(VZIP2,             VZip2);
  REGISTER_OP(VUNZIP,            VUnZip);
  REGISTER_OP(VUNZIP2,           VUnZip2);
  REGISTER_OP(VBSL,              VBSL);
  REGISTER_OP(VCMPEQ,            VCMPEQ);
  REGISTER_OP(VCMPEQZ,           VCMPEQZ);
  REGISTER_OP(VCMPGT,            VCMPGT);
  REGISTER_OP(VCMPGTZ,           VCMPGTZ);
  REGISTER_OP(VCMPLTZ,           VCMPLTZ);
  REGISTER_OP(VFCMPEQ,           VFCMPEQ);
  REGISTER_OP(VFCMPNEQ,          VFCMPNEQ);
  REGISTER_OP(VFCMPLT,           VFCMPLT);
  REGISTER_OP(VFCMPGT,           VFCMPGT);
  REGISTER_OP(VFCMPLE,           VFCMPLE);
  REGISTER_OP(VFCMPORD,          VFCMPORD);
  REGISTER_OP(VFCMPUNO,          VFCMPUNO);
  REGISTER_OP(VUSHL,             VUShl);
  REGISTER_OP(VUSHR,             VUShr);
  REGISTER_OP(VSSHR,             VSShr);
  REGISTER_OP(VUSHLS,            VUShlS);
  REGISTER_OP(VUSHRS,            VUShrS);
  REGISTER_OP(VSSHRS,            VSShrS);
  REGISTER_OP(VINSELEMENT,       VInsElement);
  REGISTER_OP(VDUPELEMENT,       VDupElement);
  REGISTER_OP(VEXTR,             VExtr);
  REGISTER_OP(VUSHRI,            VUShrI);
  REGISTER_OP(VSSHRI,            VSShrI);
  REGISTER_OP(VSHLI,             VShlI);
  REGISTER_OP(VUSHRNI,           VUShrNI);
  REGISTER_OP(VUSHRNI2,          VUShrNI2);
  REGISTER_OP(VSXTL,             VSXTL);
  REGISTER_OP(VSXTL2,            VSXTL2);
  REGISTER_OP(VUXTL,             VUXTL);
  REGISTER_OP(VUXTL2,            VUXTL2);
  REGISTER_OP(VSQXTN,            VSQXTN);
  REGISTER_OP(VSQXTN2,           VSQXTN2);
  REGISTER_OP(VSQXTUN,           VSQXTUN);
  REGISTER_OP(VSQXTUN2,          VSQXTUN2);
  REGISTER_OP(VUMUL,             VMul);
  REGISTER_OP(VSMUL,             VMul);
  REGISTER_OP(VUMULL,            VUMull);
  REGISTER_OP(VSMULL,            VSMull);
  REGISTER_OP(VUMULL2,           VUMull2);
  REGISTER_OP(VSMULL2,           VSMull2);
  REGISTER_OP(VUABDL,            VUABDL);
  REGISTER_OP(VTBL1,             VTBL1);
  REGISTER_OP(VREV64,            VRev64);
#undef REGISTER_OP
}
}

