#include "find_defpartgroup.h"

#include "common/goos/PrettyPrinter.h"
#include "common/util/BitUtils.h"

#include "decompiler/IR2/Env.h"
#include "decompiler/IR2/Form.h"
#include "decompiler/IR2/GenericElementMatcher.h"
#include "decompiler/ObjectFile/LinkedObjectFile.h"
#include "decompiler/util/data_decompile.h"

namespace decompiler {

namespace {

const goos::Object& car(const goos::Object* x) {
  return x->as_pair()->car;
}

const goos::Object* cdr(const goos::Object* x) {
  return &x->as_pair()->cdr;
}

void read_static_group_data(const DecompilerLabel& lab,
                            const Env& env,
                            DefpartgroupElement::StaticInfo& group) {
  // looks like:

  // Jak 3
  /*
    .type sparticle-launch-group
L81:
    .word 0xbb80042
    .word 0x405dc
    .word L83
    .word L82
    .word 0x0
    .word 0x0
    .word 0x0
    .word 0x0
    .word 0x0
    .word 0x0
    .word 0x47800000
L82:
   */

  // Jak X
  /*
    .type sparticle-launch-group
L23:
    .word 0x5dc0006
    .word 0x2105dc
    .word L25
    .word L24
    .word 0x0
    .word 0x0
    .word 0x0
    .word 0x3f800000
    .word 0x3f800000
    .word 0x3f800000
    .word 0x0
    .word 0x0
    .word 0x0
    .word 0x0
    .word 0x47700000
   */

  int word_idx = (lab.offset / 4) - 1;
  auto& words = env.file->words_by_seg.at(lab.target_segment);

  auto& first_word = words.at(word_idx++);
  if (first_word.kind() != LinkedWord::TYPE_PTR ||
      first_word.symbol_name() != "sparticle-launch-group") {
    env.func->warnings.error_and_throw(
        "Reference to sparticle-launch-group bad: invalid type pointer");
  }

  auto& word_1 = words.at(word_idx++);
  s16 len = word_1.data & 0xffff;
  group.duration = (word_1.data >> 16) & 0xffff;
  auto& word_2 = words.at(word_idx++);
  group.linger = word_2.data & 0xffff;
  group.flags = (word_2.data >> 16) & 0xffff;

  auto& string_word = words.at(word_idx++);
  if (string_word.kind() != LinkedWord::PTR) {
    env.func->warnings.error_and_throw(
        "Reference to sparticle-launch-group bad: invalid name label");
  }
  group.name = env.file->get_goal_string_by_label(
      env.file->get_label_by_name(env.file->get_label_name(string_word.label_id())));

  auto& array_word = words.at(word_idx++);
  if (array_word.kind() != LinkedWord::PTR) {
    env.func->warnings.error_and_throw(
        "Reference to sparticle-launch-group bad: invalid array label");
  }
  auto& array_lab = env.file->get_label_by_name(env.file->get_label_name(array_word.label_id()));
  auto& array_words = env.file->words_by_seg.at(array_lab.target_segment);
  int array_start_word_idx = array_lab.offset / 4;
  group.elts.clear();
  for (int i = 0; i < len; ++i) {
    int item_idx = i * 8 + array_start_word_idx;
    auto& item = group.elts.emplace_back();
    item.part_id = array_words.at(item_idx + 0).data;
    item.fade = *reinterpret_cast<float*>(&array_words.at(item_idx + 1).data);
    item.falloff = *reinterpret_cast<float*>(&array_words.at(item_idx + 2).data);
    item.flags = array_words.at(item_idx + 3).data & 0xffff;
    item.period = (array_words.at(item_idx + 3).data >> 16) & 0xffff;
    item.length = array_words.at(item_idx + 4).data & 0xffff;
    item.offset = (array_words.at(item_idx + 4).data >> 16) & 0xffff;
    item.hour_mask = array_words.at(item_idx + 5).data;
    item.binding = array_words.at(item_idx + 6).data;
  }

  if (env.version != GameVersion::Jak1) {
    // added fields in jak 2
    for (int i = 0; i < 3; i++) {
      auto& word = words.at(word_idx++);
      if (word.kind() != LinkedWord::PLAIN_DATA) {
        env.func->warnings.error_and_throw("Reference to sparticle-launch-group bad: invalid rot");
      }
      group.rot[i] = *reinterpret_cast<float*>(&word.data);
    }
    for (int i = 0; i < 3; i++) {
      auto& word = words.at(word_idx++);
      if (word.kind() != LinkedWord::PLAIN_DATA) {
        env.func->warnings.error_and_throw(
            "Reference to sparticle-launch-group bad: invalid scale");
      }
      group.scale[i] = *reinterpret_cast<float*>(&word.data);
    }
  }

  word_idx = align4(word_idx);
  for (int i = 0; i < 4; i++) {
    auto& word = words.at(word_idx + i);
    if (word.kind() != LinkedWord::PLAIN_DATA) {
      env.func->warnings.error_and_throw("Reference to sparticle-launch-group bad: invalid bounds");
    }
    group.bounds[i] = *reinterpret_cast<float*>(&word.data);
  }
  word_idx += 4;
}

void read_static_part_data(DecompiledDataElement* src,
                           const Env& env,
                           DefpartElement::StaticInfo& part) {
  auto lab = src->label();
  // looks like:

  // Jak 3
  /*
    .type sparticle-launcher
L79:
    .word 0x0
    .word 0x0
    .word L80
L80:
    .word 0x1
    .word 0x201200
    .word 0x0
    .word 0x0
    .word 0x10006
    .word 0x3dcccccd
    .word 0x0
    .word 0x3f800000
   */

  // Jak X
  /*
    .type sparticle-launcher
L11:
    .word 0x0
    .word L12
    .word 0x0
L12:
    .word 0x1
    .word 0x401000
    .word 0x0
    .word 0x0
    .word 0x10006
    .word 0x41200000
    .word 0x0
    .word 0x3f800000
   */

  int start_word_idx = (lab.offset / 4) - 1;
  auto& words = env.file->words_by_seg.at(lab.target_segment);

  auto& first_word = words.at(start_word_idx);
  if (first_word.kind() != LinkedWord::TYPE_PTR ||
      first_word.symbol_name() != "sparticle-launcher") {
    env.func->warnings.error_and_throw("Reference to sparticle-launcher bad: invalid type pointer");
  }

  auto empty2_idx = start_word_idx + (env.version != GameVersion::JakX ? 2 : 3);
  auto array_word_idx = start_word_idx + (env.version != GameVersion::JakX ? 3 : 2);

  auto& empty1 = words.at(start_word_idx + 1);
  auto& empty2 = words.at(empty2_idx);
  if (empty1.kind() != LinkedWord::PLAIN_DATA || empty1.data != 0 ||
      empty2.kind() != LinkedWord::PLAIN_DATA || empty2.data != 0) {
    env.func->warnings.error_and_throw("Reference to sparticle-launcher bad: accums not empty");
  }

  auto& array_word = words.at(array_word_idx);
  if (array_word.kind() != LinkedWord::PTR) {
    env.func->warnings.error_and_throw("Reference to sparticle-launcher bad: invalid array label");
  }
  auto& array_lab = env.file->get_label_by_name(env.file->get_label_name(array_word.label_id()));
  auto& array_words = env.file->words_by_seg.at(array_lab.target_segment);
  int array_start_word_idx = array_lab.offset / 4;
  part.fields.clear();
  src->do_decomp(env, env.file);
  auto obj = src->to_form(env);
  obj = car(cdr(cdr(&obj)));
  auto cur_field = cdr(&obj);
  for (int i = 0; true; ++i) {
    int field_idx = i * 4 + array_start_word_idx;
    auto& item = part.fields.emplace_back();
    item.field_id = array_words.at(field_idx + 0).data & 0xffff;
    item.flags = (array_words.at(field_idx + 0).data >> 16) & 0xffff;
    item.data.push_back(array_words.at(field_idx + 0));
    item.data.push_back(array_words.at(field_idx + 1));
    item.data.push_back(array_words.at(field_idx + 2));
    item.data.push_back(array_words.at(field_idx + 3));
    if (item.flags == 4) {
      auto& fld = car(cur_field);
      item.sound_spec = cdr(cdr(cdr(cdr(&fld))))->as_pair()->car;
    }
    item.userdata = car(cur_field);
    if (item.is_sp_end(env.version)) {
      // sp-end
      break;
    }
    cur_field = cdr(cur_field);
  }
}

}  // namespace

void run_defpartgroup(Function& top_level_func,
                      std::unordered_map<u32, std::string>& part_group_table) {
  auto& env = top_level_func.ir2.env;
  auto& pool = *top_level_func.ir2.form_pool;
  if (!top_level_func.ir2.top_form) {
    return;
  }
  top_level_func.ir2.top_form->apply_form([&](Form* form) {
    auto& elts = form->elts();
    for (size_t idx = 0; idx < elts.size(); idx++) {
      // The usual, fully-folded shape:
      //   (set! (-> *part-group-id-table* 188) (new 'static 'sparticle-launch-group ...
      // The compiler's own symbol-load and label-load feed directly into the store
      // because convert_to_expressions (FormExpressionAnalysis.cpp) was able to
      // inline them, so the whole thing is one SetFormFormElement.
      auto as_set = dynamic_cast<SetFormFormElement*>(elts.at(idx));
      if (as_set) {
        if (as_set->dst()->elts().size() != 1) {
          continue;
        }
        auto dest = dynamic_cast<DerefElement*>(as_set->dst()->elts().at(0));
        if (!dest)
          continue;
        if (dest->tokens().size() != 1)
          continue;
        if (dest->tokens().at(0).kind() != DerefToken::Kind::INTEGER_CONSTANT)
          continue;
        if (dest->base()->elts().size() != 1)
          continue;
        auto dest_base = dynamic_cast<SimpleExpressionElement*>(dest->base()->elts().at(0));
        if (!dest_base || !dest_base->expr().is_identity() || dest_base->expr().args() < 1)
          continue;
        auto src = dynamic_cast<DecompiledDataElement*>(as_set->src()->elts().at(0));
        if (!src)
          continue;
        auto& sym = dest_base->expr().get_arg(0);
        if (!sym.is_sym_val())
          continue;

        int id = dest->tokens().at(0).int_constant();
        FormElement* rewritten = nullptr;
        if (sym.get_str() == "*part-group-id-table*") {
          DefpartgroupElement::StaticInfo group;
          read_static_group_data(src->label(), env, group);
          part_group_table.emplace(id, group.name);
          rewritten = pool.alloc_element<DefpartgroupElement>(group, id);
        } else if (sym.get_str() == "*part-id-table*") {
          DefpartElement::StaticInfo part;
          read_static_part_data(src, env, part);
          rewritten = pool.alloc_element<DefpartElement>(part, id);
        }
        if (rewritten) {
          elts.at(idx) = rewritten;
        }
        continue;
      }

      // jakx shape (wvehicle-hud): an unrelated instruction later in the SAME
      // top-level function can trip types2::run's "Failed to guess label use"
      // bailout (types2.cpp, the unknown_label_tag/selected_type check), which
      // marks types_succeeded false for the whole function. ir2_build_expressions
      // (ObjectFileDB_IR2.cpp) skips any function whose types_succeeded is false,
      // so convert_to_expressions never runs on it: the id-table store stays the
      // raw StorePlainDeref that build_initial_forms/StoreOp::get_as_form produced
      // (AtomicOpForm.cpp; the array index is a compile-time constant, so it is a
      // plain resolved field offset, not an OBJECT_PLUS_PRODUCT_WITH_CONSTANT
      // stride access), and its base and value registers stay two separate,
      // preceding SetVarElement statements (SetVarOp::get_as_form) instead of
      // being inlined into one SetFormFormElement the way
      // StorePlainDeref::push_to_stack (FormExpressionAnalysis.cpp) would have
      // done. This happens even for statements that type-propagated cleanly on
      // their own before the later, unrelated failure. Concretely, three
      // top-level siblings instead of one:
      //   (set! v1-N L5xx)                    ; value def: a bare label address
      //   (set! a0-N *part-group-id-table*)   ; base def: a bare symbol value
      //   (set! (-> a0-N id) v1-N)            ; the store, a StorePlainDeref
      // Recover the same rewrite by reading the label and symbol back out of the
      // two immediately preceding sibling statements instead of requiring them
      // pre-folded. This is not gated to GameVersion::JakX: the mechanism that
      // produces the shape (a whole-function types_succeeded bailout) is generic
      // decompiler machinery, not a jakx-specific code path; it simply has not
      // been observed to fire on a jak1/2/3 defpartgroup site.
      auto as_store = dynamic_cast<StorePlainDeref*>(elts.at(idx));
      if (!as_store || idx < 2 || !as_store->expr().is_var()) {
        continue;
      }
      if (as_store->dst()->elts().size() != 1) {
        continue;
      }
      auto dest = dynamic_cast<DerefElement*>(as_store->dst()->elts().at(0));
      if (!dest)
        continue;
      if (dest->tokens().size() != 1)
        continue;
      if (dest->tokens().at(0).kind() != DerefToken::Kind::INTEGER_CONSTANT)
        continue;
      if (dest->base()->elts().size() != 1)
        continue;
      auto dest_base = dynamic_cast<SimpleExpressionElement*>(dest->base()->elts().at(0));
      if (!dest_base || !dest_base->expr().is_identity() || dest_base->expr().args() < 1)
        continue;
      auto& base_arg = dest_base->expr().get_arg(0);
      if (!base_arg.is_var())
        continue;
      int id = dest->tokens().at(0).int_constant();

      auto base_def = dynamic_cast<SetVarElement*>(elts.at(idx - 1));
      auto value_def = dynamic_cast<SetVarElement*>(elts.at(idx - 2));
      if (!base_def || !value_def) {
        continue;
      }
      if (!same_expression_var(base_def->dst(), base_arg.var()) ||
          !same_expression_var(value_def->dst(), as_store->expr().var())) {
        continue;
      }

      if (base_def->src()->elts().size() != 1) {
        continue;
      }
      auto base_def_src = dynamic_cast<SimpleExpressionElement*>(base_def->src()->elts().at(0));
      if (!base_def_src || !base_def_src->expr().is_identity()) {
        continue;
      }
      auto& sym = base_def_src->expr().get_arg(0);
      if (!sym.is_sym_val())
        continue;

      if (value_def->src()->elts().size() != 1) {
        continue;
      }
      auto value_def_src = dynamic_cast<SimpleExpressionElement*>(value_def->src()->elts().at(0));
      if (!value_def_src || !value_def_src->expr().is_identity()) {
        continue;
      }
      auto& label_atom = value_def_src->expr().get_arg(0);
      if (!label_atom.is_label())
        continue;

      auto lab = env.file->labels.at(label_atom.label());
      FormElement* rewritten = nullptr;
      if (sym.get_str() == "*part-group-id-table*") {
        DefpartgroupElement::StaticInfo group;
        read_static_group_data(lab, env, group);
        part_group_table.emplace(id, group.name);
        rewritten = pool.alloc_element<DefpartgroupElement>(group, id);
      } else if (sym.get_str() == "*part-id-table*") {
        const auto& hint = env.file->label_db->lookup(label_atom.label());
        if (!hint.known) {
          continue;
        }
        DefpartElement::StaticInfo part;
        auto data_elt = pool.alloc_element<DecompiledDataElement>(lab, hint);
        read_static_part_data(data_elt, env, part);
        rewritten = pool.alloc_element<DefpartElement>(part, id);
      }

      if (rewritten) {
        elts.at(idx) = rewritten;
        elts.erase(elts.begin() + (idx - 2), elts.begin() + idx);
        idx -= 2;
      }
    }
  });
}
}  // namespace decompiler
