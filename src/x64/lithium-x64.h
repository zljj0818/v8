// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_X64_LITHIUM_X64_H_
#define V8_X64_LITHIUM_X64_H_

#include "hydrogen.h"
#include "lithium-allocator.h"
#include "lithium.h"
#include "safepoint-table.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LCodeGen;


// Type hierarchy:
//
// LInstruction
//   LDeoptimize
//   LGap
//     LLabel
//   LGoto
//   LLazyBailout
//   LOsrEntry

#define LITHIUM_ALL_INSTRUCTION_LIST(V)         \
  LITHIUM_CONCRETE_INSTRUCTION_LIST(V)

#define LITHIUM_CONCRETE_INSTRUCTION_LIST(V)    \
  V(Deoptimize)                                 \
  V(Gap)                                        \
  V(Goto)                                       \
  V(Label)                                      \
  V(LazyBailout)                                \
  V(OsrEntry)


#define DECLARE_INSTRUCTION(type)                \
  virtual bool Is##type() const { return true; } \
  static L##type* cast(LInstruction* instr) {    \
    ASSERT(instr->Is##type());                   \
    return reinterpret_cast<L##type*>(instr);    \
  }


#define DECLARE_CONCRETE_INSTRUCTION(type, mnemonic)        \
  virtual void CompileToNative(LCodeGen* generator);        \
  virtual const char* Mnemonic() const { return mnemonic; } \
  DECLARE_INSTRUCTION(type)


#define DECLARE_HYDROGEN_ACCESSOR(type)     \
  H##type* hydrogen() const {               \
    return H##type::cast(hydrogen_value()); \
  }


class LInstruction: public ZoneObject {
 public:
  LInstruction()
      : hydrogen_value_(NULL) { }
  virtual ~LInstruction() { }

  virtual void CompileToNative(LCodeGen* generator) = 0;
  virtual const char* Mnemonic() const = 0;
  virtual void PrintTo(StringStream* stream);
  virtual void PrintDataTo(StringStream* stream) { }

  // Declare virtual type testers.
#define DECLARE_DO(type) virtual bool Is##type() const { return false; }
  LITHIUM_ALL_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO
  virtual bool IsControl() const { return false; }

  void set_environment(LEnvironment* env) { environment_.set(env); }
  LEnvironment* environment() const { return environment_.get(); }
  bool HasEnvironment() const { return environment_.is_set(); }

  void set_pointer_map(LPointerMap* p) { pointer_map_.set(p); }
  LPointerMap* pointer_map() const { return pointer_map_.get(); }
  bool HasPointerMap() const { return pointer_map_.is_set(); }

  virtual bool HasResult() const = 0;

  void set_hydrogen_value(HValue* value) { hydrogen_value_ = value; }
  HValue* hydrogen_value() const { return hydrogen_value_; }

  void set_deoptimization_environment(LEnvironment* env) {
    deoptimization_environment_.set(env);
  }
  LEnvironment* deoptimization_environment() const {
    return deoptimization_environment_.get();
  }
  bool HasDeoptimizationEnvironment() const {
    return deoptimization_environment_.is_set();
  }

 private:
  SetOncePointer<LEnvironment> environment_;
  SetOncePointer<LPointerMap> pointer_map_;
  HValue* hydrogen_value_;
  SetOncePointer<LEnvironment> deoptimization_environment_;
};


template <int Result>
class LTemplateInstruction: public LInstruction { };


template<>
class LTemplateInstruction<0>: public LInstruction {
  virtual bool HasResult() const { return false; }
};


template<>
class LTemplateInstruction<1>: public LInstruction {
 public:
  static LTemplateInstruction<1>* cast(LInstruction* instr)  {
    ASSERT(instr->HasResult());
    return reinterpret_cast<LTemplateInstruction<1>*>(instr);
  }
  void set_result(LOperand* operand) { result_.set(operand); }
  LOperand* result() const { return result_.get(); }
  virtual bool HasResult() const { return result_.is_set(); }
 private:
  SetOncePointer<LOperand> result_;
};


class LGap: public LTemplateInstruction<0> {
 public:
  explicit LGap(HBasicBlock* block)
      : block_(block) {
    parallel_moves_[BEFORE] = NULL;
    parallel_moves_[START] = NULL;
    parallel_moves_[END] = NULL;
    parallel_moves_[AFTER] = NULL;
  }

  DECLARE_CONCRETE_INSTRUCTION(Gap, "gap")
  virtual void PrintDataTo(StringStream* stream);

  bool IsRedundant() const;

  HBasicBlock* block() const { return block_; }

  enum InnerPosition {
    BEFORE,
    START,
    END,
    AFTER,
    FIRST_INNER_POSITION = BEFORE,
    LAST_INNER_POSITION = AFTER
  };

  LParallelMove* GetOrCreateParallelMove(InnerPosition pos) {
    if (parallel_moves_[pos] == NULL) parallel_moves_[pos] = new LParallelMove;
    return parallel_moves_[pos];
  }

  LParallelMove* GetParallelMove(InnerPosition pos)  {
    return parallel_moves_[pos];
  }

 private:
  LParallelMove* parallel_moves_[LAST_INNER_POSITION + 1];
  HBasicBlock* block_;
};


class LGoto: public LTemplateInstruction<0> {
 public:
  LGoto(int block_id, bool include_stack_check = false)
    : block_id_(block_id), include_stack_check_(include_stack_check) { }

  DECLARE_CONCRETE_INSTRUCTION(Goto, "goto")
  virtual void PrintDataTo(StringStream* stream);
  virtual bool IsControl() const { return true; }

  int block_id() const { return block_id_; }
  bool include_stack_check() const { return include_stack_check_; }

 private:
  int block_id_;
  bool include_stack_check_;
};


class LLazyBailout: public LTemplateInstruction<0> {
 public:
  LLazyBailout() : gap_instructions_size_(0) { }

  DECLARE_CONCRETE_INSTRUCTION(LazyBailout, "lazy-bailout")

  void set_gap_instructions_size(int gap_instructions_size) {
    gap_instructions_size_ = gap_instructions_size;
  }
  int gap_instructions_size() { return gap_instructions_size_; }

 private:
  int gap_instructions_size_;
};


class LDeoptimize: public LTemplateInstruction<0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(Deoptimize, "deoptimize")
};


class LLabel: public LGap {
 public:
  explicit LLabel(HBasicBlock* block)
      : LGap(block), replacement_(NULL) { }

  DECLARE_CONCRETE_INSTRUCTION(Label, "label")

  virtual void PrintDataTo(StringStream* stream);

  int block_id() const { return block()->block_id(); }
  bool is_loop_header() const { return block()->IsLoopHeader(); }
  Label* label() { return &label_; }
  LLabel* replacement() const { return replacement_; }
  void set_replacement(LLabel* label) { replacement_ = label; }
  bool HasReplacement() const { return replacement_ != NULL; }

 private:
  Label label_;
  LLabel* replacement_;
};


class LOsrEntry: public LTemplateInstruction<0> {
 public:
  LOsrEntry();

  DECLARE_CONCRETE_INSTRUCTION(OsrEntry, "osr-entry")

  LOperand** SpilledRegisterArray() { return register_spills_; }
  LOperand** SpilledDoubleRegisterArray() { return double_register_spills_; }

  void MarkSpilledRegister(int allocation_index, LOperand* spill_operand);
  void MarkSpilledDoubleRegister(int allocation_index,
                                 LOperand* spill_operand);

 private:
  // Arrays of spill slot operands for registers with an assigned spill
  // slot, i.e., that must also be restored to the spill slot on OSR entry.
  // NULL if the register has no assigned spill slot.  Indexed by allocation
  // index.
  LOperand* register_spills_[Register::kNumAllocatableRegisters];
  LOperand* double_register_spills_[DoubleRegister::kNumAllocatableRegisters];
};


class LChunkBuilder;
class LChunk: public ZoneObject {
 public:
  explicit LChunk(HGraph* graph)
    : spill_slot_count_(0),
      graph_(graph),
      instructions_(32),
      pointer_maps_(8),
      inlined_closures_(1) { }

  int spill_slot_count() const { return spill_slot_count_; }
  HGraph* graph() const { return graph_; }
  const ZoneList<LInstruction*>* instructions() const { return &instructions_; }
  const ZoneList<LPointerMap*>* pointer_maps() const { return &pointer_maps_; }
  const ZoneList<Handle<JSFunction> >* inlined_closures() const {
    return &inlined_closures_;
  }

  LOperand* GetNextSpillSlot(bool double_slot) {
    UNIMPLEMENTED();
    return NULL;
  }

  LConstantOperand* DefineConstantOperand(HConstant* constant) {
    UNIMPLEMENTED();
    return NULL;
  }

  LLabel* GetLabel(int block_id) const {
    UNIMPLEMENTED();
    return NULL;
  }

  int GetParameterStackSlot(int index) const {
    UNIMPLEMENTED();
    return 0;
  }

  void AddGapMove(int index, LOperand* from, LOperand* to) { UNIMPLEMENTED(); }

  LGap* GetGapAt(int index) const {
    UNIMPLEMENTED();
    return NULL;
  }

  bool IsGapAt(int index) const {
    UNIMPLEMENTED();
    return false;
  }

  int NearestGapPos(int index) const {
    UNIMPLEMENTED();
    return 0;
  }

  void MarkEmptyBlocks() { UNIMPLEMENTED(); }

#ifdef DEBUG
  void Verify() { }
#endif

 private:
  int spill_slot_count_;
  HGraph* const graph_;
  ZoneList<LInstruction*> instructions_;
  ZoneList<LPointerMap*> pointer_maps_;
  ZoneList<Handle<JSFunction> > inlined_closures_;
};


class LChunkBuilder BASE_EMBEDDED {
 public:
  LChunkBuilder(HGraph* graph, LAllocator* allocator)
      : chunk_(NULL),
        graph_(graph),
        status_(UNUSED),
        current_instruction_(NULL),
        current_block_(NULL),
        next_block_(NULL),
        argument_count_(0),
        allocator_(allocator),
        position_(RelocInfo::kNoPosition),
        instructions_pending_deoptimization_environment_(NULL),
        pending_deoptimization_ast_id_(AstNode::kNoNumber) { }

  // Build the sequence for the graph.
  LChunk* Build();

  // Declare methods that deal with the individual node types.
#define DECLARE_DO(type) LInstruction* Do##type(H##type* node) { \
    UNIMPLEMENTED(); \
    return NULL; \
  }
  HYDROGEN_CONCRETE_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

 private:
  enum Status {
    UNUSED,
    BUILDING,
    DONE,
    ABORTED
  };

  LChunk* chunk() const { return chunk_; }
  HGraph* graph() const { return graph_; }

  bool is_unused() const { return status_ == UNUSED; }
  bool is_building() const { return status_ == BUILDING; }
  bool is_done() const { return status_ == DONE; }
  bool is_aborted() const { return status_ == ABORTED; }

  void Abort(const char* format, ...);

  void DoBasicBlock(HBasicBlock* block, HBasicBlock* next_block);

  LChunk* chunk_;
  HGraph* const graph_;
  Status status_;
  HInstruction* current_instruction_;
  HBasicBlock* current_block_;
  HBasicBlock* next_block_;
  int argument_count_;
  LAllocator* allocator_;
  int position_;
  LInstruction* instructions_pending_deoptimization_environment_;
  int pending_deoptimization_ast_id_;

  DISALLOW_COPY_AND_ASSIGN(LChunkBuilder);
};


} }  // namespace v8::internal

#endif  // V8_X64_LITHIUM_X64_H_
