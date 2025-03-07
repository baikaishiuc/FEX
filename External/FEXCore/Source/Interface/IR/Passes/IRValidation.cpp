/*
$info$
tags: ir|opts
desc: Sanity checking pass
$end_info$
*/

#include "Interface/IR/PassManager.h"
#include "Interface/IR/Passes/IRValidation.h"
#include "Interface/IR/Passes/RegisterAllocationPass.h"

#include <FEXCore/IR/IR.h>
#include <FEXCore/IR/IREmitter.h>
#include <FEXCore/IR/IntrusiveIRList.h>
#include <FEXCore/IR/RegisterAllocationData.h>
#include <FEXCore/Utils/LogManager.h>
#include <FEXCore/Utils/Profiler.h>

#include <cstdint>
#include <memory>
#include <stddef.h>
#include <string>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace FEXCore::IR::Validation {


IRValidation::~IRValidation() {
  NodeIsLive.Free();
}

bool IRValidation::Run(IREmitter *IREmit) {
  FEXCORE_PROFILE_SCOPED("PassManager::IRValidation");

  bool HadError = false;
  bool HadWarning = false;

  std::ostringstream Errors;
  std::ostringstream Warnings;

  auto CurrentIR = IREmit->ViewIR();

  OffsetToBlockMap.clear();
  EntryBlock = nullptr;

  if (CurrentIR.GetSSACount() > MaxNodes) {
    NodeIsLive.Realloc(CurrentIR.GetSSACount());
  }

  std::vector<uint32_t> Uses(CurrentIR.GetSSACount(), 0);

#if defined(ASSERTIONS_ENABLED) && ASSERTIONS_ENABLED
  auto HeaderOp = CurrentIR.GetHeader();
  LOGMAN_THROW_A_FMT(HeaderOp->Header.Op == OP_IRHEADER, "First op wasn't IRHeader");
#endif

  IR::RegisterAllocationData * RAData{};
  if (Manager->HasPass("RA")) {
    RAData = Manager->GetPass<IR::RegisterAllocationPass>("RA")->GetAllocationData();
  }

  NodeIsLive.Set(1); // IRHEADER

  for (auto [BlockNode, BlockHeader] : CurrentIR.GetBlocks()) {
    auto BlockIROp = BlockHeader->CW<FEXCore::IR::IROp_CodeBlock>();
    LOGMAN_THROW_AA_FMT(BlockIROp->Header.Op == OP_CODEBLOCK, "IR type failed to be a code block");

    if (!EntryBlock) {
      EntryBlock = BlockNode;
    }

    const auto BlockID = CurrentIR.GetID(BlockNode);
    BlockInfo *CurrentBlock = &OffsetToBlockMap.try_emplace(BlockID).first->second;

    for (auto [CodeNode, IROp] : CurrentIR.GetCode(BlockNode)) {
      const auto ID = CurrentIR.GetID(CodeNode);
      const uint8_t OpSize = IROp->Size;

      if (IROp->HasDest) {
        HadError |= OpSize == 0;
        // Does the op have a destination of size 0?
        if (OpSize == 0) {
          Errors << "%ssa" << ID << ": Had destination but with no size" << std::endl;
        }

        // Does the node have zero uses? Should have been DCE'd
        if (CodeNode->GetUses() == 0) {
          HadWarning |= true;
          Warnings << "%ssa" << ID << ": Destination created but had no uses" << std::endl;
        }

        if (RAData) {
          // If we have a register allocator then the destination needs to be assigned a register and class
          auto PhyReg = RAData->GetNodeRegister(ID);

          FEXCore::IR::RegisterClassType ExpectedClass = IR::GetRegClass(IROp->Op);
          FEXCore::IR::RegisterClassType AssignedClass = FEXCore::IR::RegisterClassType{PhyReg.Class};

          // If no register class was assigned
          if (AssignedClass == IR::InvalidClass) {
            HadError |= true;
            Errors << "%ssa" << ID << ": Had destination but with no register class assigned" << std::endl;
          }

          // If no physical register was assigned
          if (PhyReg.Reg == IR::InvalidReg) {
            HadError |= true;
            Errors << "%ssa" << ID << ": Had destination but with no register assigned" << std::endl;
          }

          // Assigned class wasn't the expected class and it is a non-complex op
          if (AssignedClass != ExpectedClass &&
              ExpectedClass != IR::ComplexClass) {
            HadWarning |= true;
            Warnings << "%ssa" << ID << ": Destination had register class " << AssignedClass.Val << " When register class " << ExpectedClass.Val << " Was expected" << std::endl;
          }
        }
      }

      uint8_t NumArgs = IR::GetArgs(IROp->Op);

      if (NumArgs != IROp->NumArgs) {
        switch (IROp->Op) {
          case OP_BEGINBLOCK:
          case OP_ENDBLOCK:
          case OP_PHI:
          case OP_PHIVALUE:
          case OP_CONDJUMP:
          case OP_JUMP:
            // These override the number of args for RA, so ignore them.
            break;
          default:
            HadError |= true;
            Errors << "%ssa" << ID << ": Has wrong number of Args" << std::endl;
        }
      }
      for (uint32_t i = 0; i < NumArgs; ++i) {
        OrderedNodeWrapper Arg = IROp->Args[i];
        const auto ArgID = Arg.ID();

        // Was an argument defined after this node?
        if (ArgID >= ID) {
          HadError |= true;
          Errors << "%ssa" << ID << ": Arg[" << i << "] has definition after use at %ssa" << ArgID << std::endl;
        }

        if (ArgID.IsValid() && !NodeIsLive.Get(ArgID.Value)) {
          HadError |= true;
          Errors << "%ssa" << ID << ": Arg[" << i << "] references dead %ssa" << ArgID << std::endl;
        }

        if (ArgID.IsValid()) {
          Uses[ArgID.Value]++;
        }
      }

      NodeIsLive.Set(ID.Value);

      switch (IROp->Op) {
        case IR::OP_EXITFUNCTION: {
          CurrentBlock->HasExit = true;
        break;
        }
        case IR::OP_CONDJUMP: {
          auto Op = IROp->C<IR::IROp_CondJump>();

          OrderedNode *TrueTargetNode = CurrentIR.GetNode(Op->TrueBlock);
          OrderedNode *FalseTargetNode = CurrentIR.GetNode(Op->FalseBlock);

          CurrentBlock->Successors.emplace_back(TrueTargetNode);
          CurrentBlock->Successors.emplace_back(FalseTargetNode);

          FEXCore::IR::IROp_Header const *TrueTargetOp = CurrentIR.GetOp<IROp_Header>(TrueTargetNode);
          FEXCore::IR::IROp_Header const *FalseTargetOp = CurrentIR.GetOp<IROp_Header>(FalseTargetNode);

          if (TrueTargetOp->Op != OP_CODEBLOCK) {
            HadError |= true;
            Errors << "CondJump %ssa" << ID << ": True Target Jumps to Op that isn't the begining of a block" << std::endl;
          }
          else {
            auto Block = OffsetToBlockMap.try_emplace(Op->TrueBlock.ID()).first;
            Block->second.Predecessors.emplace_back(BlockNode);
          }

          if (FalseTargetOp->Op != OP_CODEBLOCK) {
            HadError |= true;
            Errors << "CondJump %ssa" << ID << ": False Target Jumps to Op that isn't the begining of a block" << std::endl;
          }
          else {
            auto Block = OffsetToBlockMap.try_emplace(Op->FalseBlock.ID()).first;
            Block->second.Predecessors.emplace_back(BlockNode);
          }

          break;
        }
        case IR::OP_JUMP: {
          auto Op = IROp->C<IR::IROp_Jump>();
          OrderedNode *TargetNode = CurrentIR.GetNode(Op->Header.Args[0]);
          CurrentBlock->Successors.emplace_back(TargetNode);

          FEXCore::IR::IROp_Header const *TargetOp = CurrentIR.GetOp<IROp_Header>(TargetNode);
          if (TargetOp->Op != OP_CODEBLOCK) {
            HadError |= true;
            Errors << "Jump %ssa" << ID << ": Jump to Op that isn't the begining of a block" << std::endl;
          }
          else {
            auto Block = OffsetToBlockMap.try_emplace(Op->Header.Args[0].ID()).first;
            Block->second.Predecessors.emplace_back(BlockNode);
          }
          break;
        }
        default:
          // LOGMAN_MSG_A_FMT("Unknown IR Op: {}({})", IROp->Op, FEXCore::IR::GetName(IROp->Op));
        break;
      }
    }

    // Blocks can only have zero (Exit), 1 (Unconditional branch) or 2 (Conditional) successors
    size_t NumSuccessors = CurrentBlock->Successors.size();
    if (NumSuccessors > 2) {
      HadError |= true;
      Errors << "%ssa" << BlockID << " Has " << NumSuccessors << " successors which is too many" << std::endl;
    }

    {
      auto GetOp = [](auto Code) {
        auto [CodeNode, IROp] = Code();
        return IROp->Op;
      };

      auto CodeCurrent = CurrentIR.at(BlockIROp->Last);

      // Last instruction in the block must be EndBlock
      {
        auto Op = GetOp(CodeCurrent);
        if (Op != IR::OP_ENDBLOCK) {
          HadError |= true;
          Errors << "%ssa" << BlockID << " Failed to end block with EndBlock" << std::endl;
        }
      }

      --CodeCurrent;

      // Blocks need to have an instruction that leaves the block in some way before the EndBlock instruction
      {
        auto Op = GetOp(CodeCurrent);
        if (!IsBlockExit(Op)) {
          HadError |= true;
          Errors << "%ssa" << BlockID << " Didn't have a block exit IR op as its last instruction" << std::endl;
        }
      }
    }
  }

  for (uint32_t i = 0; i < CurrentIR.GetSSACount(); i++) {
    auto [Node, IROp] = CurrentIR.at(IR::NodeID{i})();
    if (Node->NumUses != Uses[i] && IROp->Op != OP_CODEBLOCK && IROp->Op != OP_IRHEADER) {
      HadError |= true;
      Errors << "%ssa" << i << " Has " << Uses[i] << " Uses, but reports " << Node->NumUses << std::endl;
    }
  }

  HadWarning = false;
  if (HadError || HadWarning) {
    std::stringstream Out;
    FEXCore::IR::Dump(&Out, &CurrentIR, RAData);

    if (HadError) {
      Out << "Errors:" << std::endl << Errors.str() << std::endl;
    }

    if (HadWarning) {
      Out << "Warnings:" << std::endl << Warnings.str() << std::endl;
    }

    LogMan::Msg::EFmt("{}", Out.str());

    LOGMAN_MSG_A_FMT("Encountered IR validation Error");

    Errors.clear();
    Warnings.clear();
  }

  return false;
}

std::unique_ptr<FEXCore::IR::Pass> CreateIRValidation() {
  return std::make_unique<IRValidation>();
}
}
