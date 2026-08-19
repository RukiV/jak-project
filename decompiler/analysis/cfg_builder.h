#pragma once

namespace decompiler {
class Function;
class Form;
void build_initial_forms(Function& function);

/*!
 * Is a break's dead-code region nothing but a run of dead register definitions?
 * Exposed for testing; see the definition in cfg_builder.cpp.
 */
bool dead_code_is_only_var_defs(Form* dead_code);
}  // namespace decompiler
