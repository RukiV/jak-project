#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "decompiler/Function/Function.h"
#include "decompiler/config.h"
#include "decompiler/util/DecompilerTypeSystem.h"
#include "decompiler/util/TP_Type.h"

namespace decompiler::types2 {

// Backprop tag types:
//  these classes are "tags" that can be added to types to give a path to propagate constraints
//  backward. For example, if we encounter a function call, and we know the expected argument types,
//  the type pass will use these tags to propagate information backward.

/*!
 * Represents a case where there are multiple possible fields that could be accessed.
 * For example, (&-> matrix vector 0), (&-> matrix data 0). Or any case with overlapping fields.
 */
struct AmbiguousFieldAccess {
  struct Possibility {
    TypeSpec type;
    // TODO: probably stash more info here.
  };
  std::vector<Possibility> possibilities;
  int selected_possibility = -1;  // -1 if not selected.
};

/*!
 * Tag to link an unknown type back to a label with unknown type.
 */
struct UnknownLabel {
  int label_idx = -1;
  std::string label_name;  // just for debug prints
  std::optional<TypeSpec> selected_type;
  // How many times backprop_tagged_type has overwritten selected_type with a
  // different guess. See backprop_tagged_type's UNKNOWN_LABEL case and
  // kMaxTagFlips: past that cap this stops counting as a "change" so a contested
  // guess can't spin types2::run's outer loop forever (docket 2026-08-13/14
  // "Types2 non-termination").
  int flip_count = 0;
  // set true the one time the contested-guess warning fires for this tag.
  bool contested_warned = false;
  // function that owns the instruction this tag is attached to, so a contested
  // guess can be reported as a func-level warning. Set at tag creation time
  // (types2_for_label), not owned.
  Function* owning_func = nullptr;
};

/*!
 * Tag to link an unknown type back to a stack structure with unknown type.
 */
struct UnknownStackStructure {
  int stack_offset = -1;
  std::optional<TypeSpec> selected_type;
  // see UnknownLabel::flip_count.
  int flip_count = 0;
  bool contested_warned = false;
  // set at tag creation time (types2_addr_on_stack), not owned.
  Function* owning_func = nullptr;
};

/*!
 * Tag to link a type back to something outside the block.
 */
struct BlockEntryType {
  Register reg;
  bool is_reg = true;
  int stack_slot = -1;
  std::optional<TP_Type> selected_type;
  bool updated = false;
  std::optional<TP_Type>* type_to_clear = nullptr;
};

struct AmbiguousIntOrFloatConstant {
  std::optional<bool> is_float;
};

/*!
 * Union of all tag types.
 */
struct Tag {
  bool has_tag() { return kind != NONE; }
  enum Kind {
    FIELD_ACCESS,
    UNKNOWN_LABEL,
    UNKNOWN_STACK_STRUCTURE,
    BLOCK_ENTRY,
    INT_OR_FLOAT,
    NONE
  } kind = NONE;

  union {
    AmbiguousFieldAccess* field_access;
    BlockEntryType* block_entry;
    UnknownLabel* unknown_label;
    UnknownStackStructure* unknown_stack_structure;
    AmbiguousIntOrFloatConstant* int_or_float;
  };
};

/*!
 * The basic "type" that we're trying to figure out for each register on each instruction.
 */
struct Type {
  Tag tag;                      // may have type "none"
  std::optional<TP_Type> type;  // may be unknown
};

struct RegType {
  Type type;
  Register reg;
};

struct StackSlotType {
  Type type;
  int slot = -1;
};

struct TypeState {
  Type* gpr_types[32];
  Type* fpr_types[32];
  Type* next_state_type = nullptr;

  Type*& operator[](const Register& reg) {
    switch (reg.get_kind()) {
      case Reg::FPR:
        return fpr_types[reg.get_fpr()];
      case Reg::GPR:
        return gpr_types[reg.get_gpr()];
      default:
        ASSERT(false);
    }
  }

  const Type* operator[](const Register& reg) const {
    switch (reg.get_kind()) {
      case Reg::FPR:
        return fpr_types[reg.get_fpr()];
      case Reg::GPR:
        return gpr_types[reg.get_gpr()];
      default:
        ASSERT(false);
    }
  }

  Type* try_find_stack_spill_slot(int slot) {
    for (auto ss : stack_slot_types) {
      if (ss->slot == slot) {
        return &ss->type;
      }
    }
    return nullptr;
  }

  const Type* try_find_stack_spill_slot(int slot) const {
    for (auto& ss : stack_slot_types) {
      if (ss->slot == slot) {
        return &ss->type;
      }
    }
    return nullptr;
  }

  template <typename T>
  void for_each_type(T&& f) {
    for (auto gpr_type : gpr_types) {
      f(*gpr_type);
    }
    for (auto fpr_type : fpr_types) {
      f(*fpr_type);
    }
    for (auto spill : stack_slot_types) {
      f(spill->type);
    }
    f(*next_state_type);
  }

  std::vector<StackSlotType*> stack_slot_types;
};

struct Instruction {
  TypeState types;
  size_t aop_idx = -1;
  std::vector<RegType> written_reg_types;
  std::optional<StackSlotType> written_stack_slot_type;
  std::optional<Type> written_next_state_type;
  std::unique_ptr<AmbiguousFieldAccess> field_access_tag;
  std::unique_ptr<UnknownLabel> unknown_label_tag;
  std::unique_ptr<UnknownStackStructure> unknown_stack_structure_tag;
  std::unique_ptr<AmbiguousIntOrFloatConstant> int_or_float;
};

struct BlockStartTypes {
  Type gpr_types[32];
  Type fpr_types[32];
  Type next_state_type;
  std::vector<StackSlotType> stack_slot_types;

  StackSlotType* try_find_stack_spill_slot(int slot) {
    for (auto& ss : stack_slot_types) {
      if (ss.slot == slot) {
        return &ss;
      }
    }
    return nullptr;
  }

  Type& operator[](const Register& reg) {
    switch (reg.get_kind()) {
      case Reg::FPR:
        return fpr_types[reg.get_fpr()];
      case Reg::GPR:
        return gpr_types[reg.get_gpr()];
      default:
        ASSERT(false);
    }
  }
};

struct Block {
  bool needs_run = false;
  BlockStartTypes start_types;
  TypeState start_type_state;
  std::vector<Instruction*> instructions;
  std::vector<std::shared_ptr<BlockEntryType>> block_entry_tags;
};

struct FunctionCache {
  std::vector<Block> blocks;
  std::vector<Instruction> instructions;
  std::vector<RegType> reg_type_casts;
  std::vector<StackSlotType> stack_slot_casts;
  std::vector<int> block_visit_order;
};

struct Output {
  std::vector<::decompiler::TypeState> block_init_types;
  std::vector<::decompiler::TypeState> op_end_types;
  std::vector<StackStructureHint> stack_structure_hints;
  bool succeeded = false;
};

struct Input {
  TypeSpec function_type;
  DecompilerTypeSystem* dts;
  Function* func;
};

struct TypePropExtras {
  bool needs_rerun = false;
  bool tags_locked = false;
};

// Cap on how many times backprop_tagged_type may flip a single UnknownStackStructure
// or UnknownLabel tag's guessed type before treating the guess as genuinely
// contested (two consumers demanding incompatible types for one slot) rather than
// still converging, and giving up on it instead of looping forever. LCA is
// deliberately not used here: lca(vector, nav-poly) = structure broke working
// loads in the investigation that root-caused this (docket 2026-08-13/14,
// "Types2 non-termination").
//
// Measured 2026-08-14 against the full jakx corpus (2476 objects, ntsc_v1): the
// highest legitimate flip_count reached by any tag was 11 (a stack structure guess
// in expand-bounding-box-from-nav-meshes, which converges cleanly). The nav-mesh
// slot-19/44 signature-drift poison case (navloop repro) flips the same tag once
// per outer iteration with no bound, reaching 8.3 million flips in 90 seconds
// before the process was killed. 128 gives ~11.6x headroom over the measured
// healthy maximum while still being reached by the poison case in microseconds.
constexpr int kMaxTagFlips = 128;

// Safety-net cap on the outer worklist loop in types2::run (types2.cpp). If a
// function's types still fail to converge for any reason (including a
// kMaxTagFlips-tripped tag that individually stops signalling changes but leaves
// other parts of the function still settling), bail via the existing
// hit_error/goto end_type_pass path (asm punt) rather than spin forever.
//
// Measured 2026-08-14 against the same full jakx corpus: the highest
// outer_iterations reached by any function was 24 ((top-level-login
// cam-update-h)), which converges cleanly. 256 gives ~10.7x headroom.
constexpr int kMaxOuterIterations = 256;

void run(Output& out, const Input& input);

bool backprop_tagged_type(const TP_Type& expected_type,
                          types2::Type& actual_type,
                          const DecompilerTypeSystem& dts);

}  // namespace decompiler::types2