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
  BasicBlock *entry  = BasicBlock::Create(m->getContext(), "entry",  func);
  BasicBlock *cond   = BasicBlock::Create(m->getContext(), "cond",   func);
  BasicBlock *update = BasicBlock::Create(m->getContext(), "update", func);
  BasicBlock *end    = BasicBlock::Create(m->getContext(), "end",    func);
  Function::arg_iterator iter = func->arg_begin();
  Value *flag     = iter;
  Value *len      = ++iter;
  Value *posArray = ++iter;
  Value *keyArray = ++iter;
  Value *num      = ++iter;
  IRBuilder<> irb(entry);
  Value *i = irb.CreateAlloca(irb.getInt32Ty());
  irb.CreateStore(irb.getInt32(0), i);
  irb.CreateCondBr(irb.CreateICmpEQ(flag, irb.getInt8(0)), cond, end);

  irb.SetInsertPoint(cond);
  irb.CreateCondBr(
      irb.CreateICmpSLT(irb.CreateLoad(irb.getInt32Ty(), i), len),
      update, end);

  irb.SetInsertPoint(update);
  Value *pos = irb.CreateLoad(
      irb.getInt32Ty(),
      irb.CreateGEP(irb.getInt32Ty(), posArray,
                    irb.CreateLoad(irb.getInt32Ty(), i)));
  Value *key = irb.CreateGEP(irb.getInt32Ty(), keyArray, pos);
  irb.CreateStore(
      irb.CreateXor(irb.CreateLoad(irb.getInt32Ty(), key), num), key);
  irb.CreateStore(
      irb.CreateAdd(irb.CreateLoad(irb.getInt32Ty(), i), irb.getInt32(1)), i);
  irb.CreateBr(cond);

  irb.SetInsertPoint(end);
  irb.CreateRetVoid();
  return func;
}

void Flattening::doFlatten(Function *f, int seed, Function *updateFunc) {

  srand(seed);
  std::vector<BasicBlock *> origBB;
  for (BasicBlock &basicBlock : *f)
    origBB.push_back(&basicBlock);
  if (origBB.size() <= 1)
    return;

  unsigned int rand_val = seed;
  Function::iterator tmp = f->begin();
  BasicBlock *oldEntry = &*tmp;
  origBB.erase(origBB.begin());

  BranchInst *firstBr = NULL;
  if (isa<BranchInst>(oldEntry->getTerminator()))
    firstBr = cast<BranchInst>(oldEntry->getTerminator());

  BasicBlock *firstbb = oldEntry->getTerminator()->getSuccessor(0);

  if ((firstBr != NULL && firstBr->isConditional()) ||
      oldEntry->getTerminator()->getNumSuccessors() > 2) {
    BasicBlock::iterator iter = oldEntry->end();
    iter--;
    if (oldEntry->size() > 1)
      iter--;
    BasicBlock *splited =
        oldEntry->splitBasicBlock(iter, Twine("__entry_split"));
    firstbb = splited;
    origBB.insert(origBB.begin(), splited);
  }

  // ── Create dispatcher infrastructure ─────────────────────────────────────

  BasicBlock *newEntry   = oldEntry;
  BasicBlock *loopBegin  = BasicBlock::Create(f->getContext(), "__blk_a", f, newEntry);
  BasicBlock *defaultCase= BasicBlock::Create(f->getContext(), "__blk_b", f, newEntry);
  BasicBlock *loopEnd    = BasicBlock::Create(f->getContext(), "__blk_c", f, newEntry);

  newEntry->moveBefore(loopBegin);
  BranchInst::Create(loopEnd, defaultCase);
  BranchInst::Create(loopBegin, loopEnd);
  newEntry->getTerminator()->eraseFromParent();
  BranchInst::Create(loopBegin, newEntry);

  // ── switchVar — state variable ────────────────────────────────────────────
  AllocaInst *switchVar =
      new AllocaInst(Type::getInt32Ty(f->getContext()), 0,
                     Twine("__sv"), newEntry->getTerminator());

  // ── Generate a random XOR key for this function ───────────────────────────
  // This key is a compile-time constant unique per function per build.
  // It is embedded as an immediate in the XOR instruction.
  // The load → XOR → switch sequence breaks Microsoft's
  // graph-based LummaC signature which expects load → switch directly.
  std::vector<unsigned int> rand_list;
  unsigned int xor_key = getUniqueNumber(rand_list);
  rand_list.push_back(xor_key);

  LLVMContext &ctx  = f->getContext();
  IntegerType *i32  = Type::getInt32Ty(ctx);
  ConstantInt *xorKeyConst = ConstantInt::get(i32, xor_key);

  // ── Dispatcher: load → XOR decode → switch ───────────────────────────────
  // Old pattern (flagged by Microsoft):
  //   %lv  = load i32, ptr %sv
  //   switch i32 %lv, ...
  //
  // New pattern (breaks the graph signature):
  //   %lv  = load i32, ptr %sv
  //   %dec = xor i32 %lv, <key>   ← new node inserted in CFG
  //   switch i32 %dec, ...

  LoadInst *loadedVal =
      new LoadInst(switchVar->getAllocatedType(), switchVar, "__lv", loopBegin);

  // XOR decode before the switch
  BinaryOperator *decodedVal =
      BinaryOperator::Create(Instruction::Xor, loadedVal, xorKeyConst,
                             "__dec", loopBegin);

  SwitchInst *sw =
      SwitchInst::Create(decodedVal, defaultCase, 0, loopBegin);

  // ── Assign case numbers and XOR-encode start value ───────────────────────
  unsigned int startNum = 0;

  for (std::vector<BasicBlock *>::iterator b = origBB.begin();
       b != origBB.end(); b++) {
    BasicBlock *block = *b;
    block->moveBefore(loopEnd);

    // Generate unique raw case number
    unsigned int num = getUniqueNumber(rand_list);
    rand_list.push_back(num);

    if (block == firstbb)
      startNum = num;

    // Add raw case number to switch (switch compares against decoded value)
    ConstantInt *numCase =
        cast<ConstantInt>(ConstantInt::get(sw->getCondition()->getType(), num));
    sw->addCase(numCase, block);
  }

  // Store XOR-encoded initial state into switchVar
  // At runtime: load → XOR(encoded, key) = raw → switch matches raw case
  unsigned int encodedStart = startNum ^ xor_key;
  ConstantInt *startVal =
      cast<ConstantInt>(ConstantInt::get(sw->getCondition()->getType(),
                                         encodedStart));
  new StoreInst(startVal, switchVar, newEntry->getTerminator());

  errs() << "Put Block Into Switch\n";

  // ── Rewrite block terminators ─────────────────────────────────────────────
  for (std::vector<BasicBlock *>::iterator b = origBB.begin();
       b != origBB.end(); b++) {
    BasicBlock *block = *b;

    if (block->getTerminator()->getNumSuccessors() == 1) {
      errs() << "This block has 1 successor\n";

      BasicBlock *succ    = block->getTerminator()->getSuccessor(0);
      ConstantInt *caseNum = sw->findCaseDest(succ);

      if (caseNum == NULL) {
        unsigned int num = getUniqueNumber(rand_list);
        rand_list.push_back(num);
        caseNum = cast<ConstantInt>(
            ConstantInt::get(sw->getCondition()->getType(), num));
      }

      // XOR-encode the next state before storing
      // switch will decode it back via the XOR in the dispatcher
      unsigned int rawCase     = caseNum->getValue().getZExtValue();
      unsigned int encodedCase = rawCase ^ xor_key;
      ConstantInt *encodedConst =
          cast<ConstantInt>(ConstantInt::get(sw->getCondition()->getType(),
                                              encodedCase));

      block->getTerminator()->eraseFromParent();
      new StoreInst(encodedConst, switchVar, block);
      BranchInst::Create(loopEnd, block);

    } else if (block->getTerminator()->getNumSuccessors() == 2) {
      errs() << "This block has 2 successors\n";

      BasicBlock *succTrue  = block->getTerminator()->getSuccessor(0);
      BasicBlock *succFalse = block->getTerminator()->getSuccessor(1);
      ConstantInt *numTrue  = sw->findCaseDest(succTrue);
      ConstantInt *numFalse = sw->findCaseDest(succFalse);

      if (numTrue == NULL) {
        unsigned int num = getUniqueNumber(rand_list);
        rand_list.push_back(num);
        numTrue = cast<ConstantInt>(
            ConstantInt::get(sw->getCondition()->getType(), num));
      }
      if (numFalse == NULL) {
        unsigned int num = getUniqueNumber(rand_list);
        rand_list.push_back(num);
        numFalse = cast<ConstantInt>(
            ConstantInt::get(sw->getCondition()->getType(), num));
      }

      // XOR-encode both branch targets
      unsigned int rawTrue     = numTrue->getValue().getZExtValue();
      unsigned int rawFalse    = numFalse->getValue().getZExtValue();
      unsigned int encTrue     = rawTrue  ^ xor_key;
      unsigned int encFalse    = rawFalse ^ xor_key;
      ConstantInt *encTrueConst =
          cast<ConstantInt>(ConstantInt::get(sw->getCondition()->getType(),
                                              encTrue));
      ConstantInt *encFalseConst =
          cast<ConstantInt>(ConstantInt::get(sw->getCondition()->getType(),
                                              encFalse));

      BranchInst *oldBr = cast<BranchInst>(block->getTerminator());

      // select(cond, encoded_true, encoded_false)
      SelectInst *select =
          SelectInst::Create(oldBr->getCondition(),
                             encTrueConst, encFalseConst,
                             Twine("__sel"), block->getTerminator());

      block->getTerminator()->eraseFromParent();
      new StoreInst(select, switchVar, block);
      BranchInst::Create(loopEnd, block);

    } else {
      continue;
    }
  }

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