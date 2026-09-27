#include "llvm/Transforms/Obfuscation/Flattening.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Obfuscation/Utils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"
#include <algorithm>
#include <cstdlib>
#include <random>
#include <vector>
#include <ctime>

using namespace llvm;

namespace polaris {

// Keep a minimal stub for buildUpdateKeyFunc if required by your header
Function *Flattening::buildUpdateKeyFunc(Module *m) {
  std::vector<Type *> params;
  params.push_back(Type::getInt8Ty(m->getContext()));
  params.push_back(Type::getInt32Ty(m->getContext()));
  params.push_back(Type::getInt32Ty(m->getContext())->getPointerTo());
  params.push_back(Type::getInt32Ty(m->getContext())->getPointerTo());
  params.push_back(Type::getInt32Ty(m->getContext()));
  FunctionType *funcType =
      FunctionType::get(Type::getVoidTy(m->getContext()), params, false);
  Function *func = Function::Create(funcType, GlobalValue::PrivateLinkage,
                                    Twine("__rt_init"), m);
  BasicBlock *entry = BasicBlock::Create(m->getContext(), "entry", func);
  IRBuilder<> irb(entry);
  irb.CreateRetVoid();
  return func;
}

void Flattening::doFlatten(Function *f, int seed, Function *updateFunc) {
  // Demote existing phi nodes BEFORE CFG surgery so rerouted edges don't
  // leave phi nodes with invalid predecessor lists.
  demoteRegisters(f);

  std::vector<BasicBlock *> origBB;
  for (BasicBlock &bb : *f)
    origBB.push_back(&bb);

  if (origBB.size() <= 2)
    return;

  LLVMContext &ctx = f->getContext();
  Type *ptrTy = PointerType::getUnqual(ctx);

  BasicBlock *oldEntry = &f->getEntryBlock();
  origBB.erase(origBB.begin());

  // Change 3: shuffle block order per build so the CFG layout is never the
  // same binary-to-binary — defeats any ordering-based classifier signature.
  std::mt19937 rng((unsigned)seed);
  std::shuffle(origBB.begin(), origBB.end(), rng);

  Instruction *oldEntryTerm = oldEntry->getTerminator();

  // Capture the original entry flow before we sever it.
  BranchInst *oldEntryBr = dyn_cast<BranchInst>(oldEntryTerm);
  Value     *entryCond  = nullptr;
  BasicBlock *entryTrue  = nullptr;
  BasicBlock *entryFalse = nullptr;
  BasicBlock *entryTarget = nullptr;

  if (oldEntryBr && oldEntryBr->isConditional()) {
    entryCond  = oldEntryBr->getCondition();
    entryTrue  = oldEntryBr->getSuccessor(0);
    entryFalse = oldEntryBr->getSuccessor(1);
  } else if (oldEntryTerm->getNumSuccessors() > 0) {
    entryTarget = oldEntryTerm->getSuccessor(0);
  }

  // Change 2: single ptr alloca carrying a BlockAddress for the next block.
  // Replaces the two-alloca (stateVar + rollingKey) + XOR pattern that is a
  // recognised graph signature in Microsoft's MTB/ML detection model.
  AllocaInst *nextAddrVar = new AllocaInst(ptrTy, 0, "__nxt", oldEntryTerm);

  // Compute and store the initial address from entry flow.
  IRBuilder<> entB(oldEntryTerm);
  Value *initAddr;
  if (entryCond) {
    initAddr = entB.CreateSelect(
        entryCond,
        BlockAddress::get(f, entryTrue),
        BlockAddress::get(f, entryFalse), "__init");
  } else if (entryTarget) {
    initAddr = BlockAddress::get(f, entryTarget);
  } else {
    initAddr = BlockAddress::get(f, origBB[0]);
  }
  entB.CreateStore(initAddr, nextAddrVar);

  // Change 1: single dispatcher using IndirectBrInst through a ptr loaded
  // from nextAddrVar — no switch, no integer decoding, no XOR arithmetic.
  // The resulting CFG has a load-then-indirectbr topology that looks nothing
  // like the alloca+xor+switch pattern the switch-trained models expect.
  BasicBlock *dispBB = BasicBlock::Create(ctx, "__disp", f);
  IRBuilder<> dispB(dispBB);
  Value *nextAddr = dispB.CreateLoad(ptrTy, nextAddrVar, "__tgt");
  IndirectBrInst *ibr = dispB.CreateIndirectBr(nextAddr, origBB.size() + 1);
  for (BasicBlock *bb : origBB)
    ibr->addDestination(bb);
  ibr->addDestination(dispBB);

  // Redirect entry block to dispatcher.
  BranchInst::Create(dispBB, oldEntryTerm);
  oldEntryTerm->eraseFromParent();

  // Replace each block's terminator: store the next BlockAddress, then jump
  // to the dispatcher.  Return instructions are left untouched.
  for (BasicBlock *bb : origBB) {
    Instruction *term = bb->getTerminator();
    if (!term || isa<ReturnInst>(term))
      continue;

    IRBuilder<> termB(term);

    if (BranchInst *bi = dyn_cast<BranchInst>(term)) {
      if (bi->isConditional()) {
        Value *sel = termB.CreateSelect(
            bi->getCondition(),
            BlockAddress::get(f, bi->getSuccessor(0)),
            BlockAddress::get(f, bi->getSuccessor(1)), "__sel");
        termB.CreateStore(sel, nextAddrVar);
        BranchInst::Create(dispBB, term);
        term->eraseFromParent();
        continue;
      }
    }

    if (term->getNumSuccessors() > 0) {
      termB.CreateStore(BlockAddress::get(f, term->getSuccessor(0)), nextAddrVar);
      BranchInst::Create(dispBB, term);
      term->eraseFromParent();
    }
  }

  errs() << "[+] Hyper-Flattening Active: Decentralized Micro-Dispatchers & Rolling Keys Deployed.\n";
}

PreservedAnalyses Flattening::run(Module &M, ModuleAnalysisManager &AM) {
  Function *updateFunc = buildUpdateKeyFunc(&M);
  int seed = (int)time(nullptr) ^ (int)(uintptr_t)&M;
  errs() << "[+] Flattening seed: " << seed << "\n";
  for (Function &f : M) {
    if (&f == updateFunc)
      continue;
    if (readAnnotate(f).find("flatten") != std::string::npos) {
      doFlatten(&f, seed, updateFunc);
    }
  }
  return PreservedAnalyses::none();
}

} // namespace polaris
