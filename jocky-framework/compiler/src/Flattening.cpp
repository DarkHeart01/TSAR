#include "llvm/Transforms/Obfuscation/Flattening.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Obfuscation/Utils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"
#include <algorithm>
#include <cstdlib>
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
  srand(seed);
  std::vector<BasicBlock *> origBB;
  for (BasicBlock &basicBlock : *f)
    origBB.push_back(&basicBlock);
  
  // If the function is too small, flattening is unnecessary
  if (origBB.size() <= 2)
    return;

  LLVMContext &ctx = f->getContext();
  IntegerType *i32 = Type::getInt32Ty(ctx);

  // Separate entry block from the rest
  BasicBlock *oldEntry = &f->getEntryBlock();
  origBB.erase(origBB.begin());

  // ── 1. Shard blocks into decentralized micro-clusters ─────────────────────
  size_t clusterSize = 3; // Small clusters break the central hub-and-spoke signature
  std::vector<std::vector<BasicBlock *>> clusters;
  for (size_t i = 0; i < origBB.size(); i += clusterSize) {
    auto endIt = std::min(origBB.begin() + i + clusterSize, origBB.end());
    clusters.emplace_back(origBB.begin() + i, endIt);
  }

  // ── 2. Create State and Rolling Key Variables in Entry ────────────────────
  // Capture what the original entry block actually flows into *before* we
  // sever that edge below, so the initial dispatch state can be made to
  // point at the real first block instead of an arbitrary random tag.
  Instruction *oldEntryTerm = oldEntry->getTerminator();
  BasicBlock *entryTarget =
      (oldEntryTerm->getNumSuccessors() > 0) ? oldEntryTerm->getSuccessor(0) : nullptr;

  AllocaInst *stateVar = new AllocaInst(i32, 0, "__hyper_state", oldEntryTerm);
  AllocaInst *rollingKey = new AllocaInst(i32, 0, "__rolling_key", oldEntryTerm);

  std::vector<unsigned int> keyList;
  unsigned int baseKey = getUniqueNumber(keyList);
  keyList.push_back(baseKey);

  // ── 3. Build Decentralized Micro-Dispatchers ──────────────────────────────
  // Instead of one giant switch, we create multiple localized routers.
  std::vector<BasicBlock *> dispatchers;
  for (size_t c = 0; c < clusters.size(); c++) {
    BasicBlock *dispBB = BasicBlock::Create(ctx, "__micro_disp_" + std::to_string(c), f);
    dispatchers.push_back(dispBB);
  }

  // Redirect old entry to the first micro-dispatcher
  BranchInst *entryBranch = BranchInst::Create(dispatchers[0], oldEntryTerm);
  oldEntryTerm->eraseFromParent();

  // Map each basic block to a unique tag
  std::map<BasicBlock *, unsigned int> blockTags;
  unsigned int currentTag = 100;
  for (auto &cluster : clusters) {
    for (BasicBlock *bb : cluster) {
      blockTags[bb] = currentTag++;
    }
  }

  // The initial state must decode to the tag of whatever block the function
  // originally entered into. Fall back to the very first clustered block if
  // that target couldn't be resolved (e.g. entry ended in something other
  // than a simple branch).
  unsigned int initialTag = (entryTarget && blockTags.count(entryTarget))
                                ? blockTags[entryTarget]
                                : blockTags[clusters[0][0]];

  IRBuilder<> entryBuilder(entryBranch);
  entryBuilder.CreateStore(entryBuilder.getInt32(baseKey), rollingKey);
  entryBuilder.CreateStore(entryBuilder.getInt32(initialTag ^ baseKey), stateVar);

  // Populate micro-dispatchers
  for (size_t c = 0; c < clusters.size(); c++) {
    BasicBlock *dispBB = dispatchers[c];
    IRBuilder<> dispB(dispBB);

    Value *currKey = dispB.CreateLoad(i32, rollingKey);
    Value *currState = dispB.CreateLoad(i32, stateVar);
    
    // Dynamic rolling cryptographic unblinding
    Value *unmaskedState = dispB.CreateXor(currState, currKey);

    // Fallback block if state is out of bounds
    BasicBlock *nextDispatcher = (c + 1 < dispatchers.size()) ? dispatchers[c + 1] : dispatchers[0];
    SwitchInst *microSw = dispB.CreateSwitch(unmaskedState, nextDispatcher, clusters[c].size());

    for (BasicBlock *bb : clusters[c]) {
      bb->moveBefore(nextDispatcher);
      unsigned int tag = blockTags[bb];
      microSw->addCase(ConstantInt::get(i32, tag), bb);

      // FIX 1: TerminatorInst is removed in modern LLVM. Use Instruction* instead.
      Instruction *term = bb->getTerminator();
      if (!term) continue;

      // Do not touch return statements
      if (isa<ReturnInst>(term)) {
        continue; 
      }

      // If it's a conditional branch, preserve its original targets in the state machine
      if (BranchInst *bi = dyn_cast<BranchInst>(term)) {
        if (bi->isConditional()) {
          IRBuilder<> termB(term);
          unsigned int trueTag = blockTags.count(bi->getSuccessor(0)) ? blockTags[bi->getSuccessor(0)] : tag;
          unsigned int falseTag = blockTags.count(bi->getSuccessor(1)) ? blockTags[bi->getSuccessor(1)] : tag;
          
          Value *cond = bi->getCondition();
          Value *nextTargetTag = termB.CreateSelect(cond, termB.getInt32(trueTag), termB.getInt32(falseTag));
          
          unsigned int newKeyVal = baseKey + (unsigned int)(uintptr_t)bb;
          Value *encodedNext = termB.CreateXor(nextTargetTag, termB.getInt32(newKeyVal));

          termB.CreateStore(termB.getInt32(newKeyVal), rollingKey);
          termB.CreateStore(encodedNext, stateVar);
          
          // [!] THE FIX: Route the flow to the micro-dispatcher and erase the old conditional branch
          BranchInst::Create(dispatchers[(c + 1) % dispatchers.size()], term);
          term->eraseFromParent();
          
          continue;
        }
      }

      // For standard unconditional blocks, route through the micro-dispatcher chain
      IRBuilder<> termB(term);
      // Safe successor check
      BasicBlock *succ = (term->getNumSuccessors() > 0) ? term->getSuccessor(0) : clusters[(c + 1) % clusters.size()][0];
      unsigned int nextTag = blockTags.count(succ) ? blockTags[succ] : blockTags[clusters[(c + 1) % clusters.size()][0]];
      
      unsigned int newKeyVal = baseKey + (unsigned int)(uintptr_t)bb;
      unsigned int encodedNext = nextTag ^ newKeyVal;

      termB.CreateStore(termB.getInt32(newKeyVal), rollingKey);
      termB.CreateStore(termB.getInt32(encodedNext), stateVar);

      BranchInst::Create(dispatchers[(c + 1) % clusters.size()], term);
      term->eraseFromParent();
    }
  }

  errs() << "[+] Hyper-Flattening Active: Decentralized Micro-Dispatchers & Rolling Keys Deployed.\n";
  demoteRegisters(f);
}

PreservedAnalyses Flattening::run(Module &M, ModuleAnalysisManager &AM) {
  Function *updateFunc = buildUpdateKeyFunc(&M);
  int seed = (int)time(nullptr);
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
