// clang-format off
#include "game/mips2c/mips2c_private.h"
#include "game/kernel/jakx/kscheme.h"
using ::jakx::intern_from_c;
namespace Mips2C::jakx {
namespace get_string_length_asm {
struct Cache {
  void* font_work; // *font-work*
  void* font12_table; // *font12-table*
  void* font24_table; // *font24-table*
} cache;

u64 execute(void* ctxt) {
  auto* c = (ExecutionContext*)ctxt;
  printf("M2C: %s\n", __PRETTY_FUNCTION__);
  bool bc = false;
  u32 call_addr = 0;
  c->vmove(DEST::xyzw, vf23, vf0);                  // vmove.xyzw vf23, vf0
  c->vmove(DEST::xyzw, vf24, vf0);                  // vmove.xyzw vf24, vf0
  c->lw(v1, 12, a1);                                // lw v1, 12(a1)
  c->load_symbol2(a1, cache.font_work);             // lw a1, *font-work*(s7)
  c->mov64(a1, a1);                                 // or a1, a1, r0
  c->vmove(DEST::xyzw, vf1, vf0);                   // vmove.xyzw vf1, vf0
  // nop                                            // sll r0, r0, 0
  c->andi(a2, v1, 32);                              // andi a2, v1, 32
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(a2) != 0;                          // bne a2, r0, L8
  c->load_symbol2(a2, cache.font12_table);          // lw a2, *font12-table*(s7)
  if (bc) {goto block_2;}                           // branch non-likely

  c->mov64(a2, a2);                                 // or a2, a2, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 320, a1);                           // lqc2 vf13, 320(a1)
  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->lqc2(vf14, 336, a1);                           // lqc2 vf14, 336(a1)
  goto block_3;                                     // branch always

  
block_2:
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a2, cache.font24_table);          // lw a2, *font24-table*(s7)
  c->mov64(a2, a2);                                 // or a2, a2, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 368, a1);                           // lqc2 vf13, 368(a1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 384, a1);                           // lqc2 vf14, 384(a1)
  
block_3:
  c->lbu(a3, 4, a0);                                // lbu a3, 4(a0)
  c->daddiu(a0, a0, 1);                             // daddiu a0, a0, 1
  bc = c->sgpr64(a3) == 0;                          // beq a3, r0, L24
  c->daddiu(t0, a3, -3);                            // daddiu t0, a3, -3
  if (bc) {goto block_56;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t0)) <= 0;                   // blez t0, L19
  c->daddiu(t0, a3, -126);                          // daddiu t0, a3, -126
  if (bc) {goto block_48;}                          // branch non-likely

  bc = c->sgpr64(t0) != 0;                          // bne t0, r0, L20
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_49;}                          // branch non-likely

  c->lbu(a3, 4, a0);                                // lbu a3, 4(a0)
  c->daddiu(a0, a0, 1);                             // daddiu a0, a0, 1
  c->addiu(t0, r0, 0);                              // addiu t0, r0, 0
  c->addiu(t1, r0, 0);                              // addiu t1, r0, 0
  bc = c->sgpr64(a3) == 0;                          // beq a3, r0, L24
  c->daddiu(t2, a3, -43);                           // daddiu t2, a3, -43
  if (bc) {goto block_56;}                          // branch non-likely

  c->movz(t0, a3, t2);                              // movz t0, a3, t2
  c->daddiu(t2, a3, -45);                           // daddiu t2, a3, -45
  c->movz(t0, a3, t2);                              // movz t0, a3, t2
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t0) != 0;                          // bne t0, r0, L10
  c->daddiu(t2, a3, -121);                          // daddiu t2, a3, -121
  if (bc) {goto block_15;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L17
  c->daddiu(t1, a3, -89);                           // daddiu t1, a3, -89
  if (bc) {goto block_46;}                          // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L17
  c->daddiu(t1, a3, -122);                          // daddiu t1, a3, -122
  if (bc) {goto block_46;}                          // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L18
  c->daddiu(t1, a3, -90);                           // daddiu t1, a3, -90
  if (bc) {goto block_47;}                          // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L18
  c->daddiu(t1, a3, -48);                           // daddiu t1, a3, -48
  if (bc) {goto block_47;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L20
  c->daddiu(t1, a3, -57);                           // daddiu t1, a3, -57
  if (bc) {goto block_49;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t1)) > 0;                    // bgtz t1, L20
  c->daddiu(t1, a3, -126);                          // daddiu t1, a3, -126
  if (bc) {goto block_49;}                          // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L20
  c->daddiu(t1, a3, -48);                           // daddiu t1, a3, -48
  if (bc) {goto block_49;}                          // branch non-likely

  
block_15:
  c->lbu(a3, 4, a0);                                // lbu a3, 4(a0)
  c->daddiu(a0, a0, 1);                             // daddiu a0, a0, 1
  bc = c->sgpr64(a3) == 0;                          // beq a3, r0, L24
  c->daddiu(t2, a3, -110);                          // daddiu t2, a3, -110
  if (bc) {goto block_56;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L11
  c->daddiu(t2, a3, -78);                           // daddiu t2, a3, -78
  if (bc) {goto block_36;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L11
  c->daddiu(t2, a3, -108);                          // daddiu t2, a3, -108
  if (bc) {goto block_36;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L9
  c->daddiu(t2, a3, -76);                           // daddiu t2, a3, -76
  if (bc) {goto block_3;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L9
  c->daddiu(t2, a3, -119);                          // daddiu t2, a3, -119
  if (bc) {goto block_3;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L9
  c->daddiu(t2, a3, -87);                           // daddiu t2, a3, -87
  if (bc) {goto block_3;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L9
  c->daddiu(t2, a3, -107);                          // daddiu t2, a3, -107
  if (bc) {goto block_3;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L13
  c->daddiu(t2, a3, -75);                           // daddiu t2, a3, -75
  if (bc) {goto block_39;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L13
  c->daddiu(t2, a3, -106);                          // daddiu t2, a3, -106
  if (bc) {goto block_39;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L9
  c->daddiu(t2, a3, -74);                           // daddiu t2, a3, -74
  if (bc) {goto block_3;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L9
  c->daddiu(t2, a3, -104);                          // daddiu t2, a3, -104
  if (bc) {goto block_3;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L14
  c->daddiu(t2, a3, -72);                           // daddiu t2, a3, -72
  if (bc) {goto block_41;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L14
  c->daddiu(t2, a3, -118);                          // daddiu t2, a3, -118
  if (bc) {goto block_41;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L9
  c->daddiu(t2, a3, -86);                           // daddiu t2, a3, -86
  if (bc) {goto block_3;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L9
  c->daddiu(t2, a3, -117);                          // daddiu t2, a3, -117
  if (bc) {goto block_3;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L9
  c->daddiu(t2, a3, -85);                           // daddiu t2, a3, -85
  if (bc) {goto block_3;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L9
  c->daddiu(t2, a3, -48);                           // daddiu t2, a3, -48
  if (bc) {goto block_3;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L9
  c->daddiu(t2, a3, -48);                           // daddiu t2, a3, -48
  if (bc) {goto block_3;}                           // branch non-likely

  bc = ((s64)c->sgpr64(t2)) < 0;                    // bltz t2, L20
  c->daddiu(t3, a3, -57);                           // daddiu t3, a3, -57
  if (bc) {goto block_49;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t3)) > 0;                    // bgtz t3, L20
  c->sll(t3, t1, 2);                                // sll t3, t1, 2
  if (bc) {goto block_49;}                          // branch non-likely

  c->daddu(a3, t1, t3);                             // daddu a3, t1, t3
  // nop                                            // sll r0, r0, 0
  c->sll(a3, a3, 1);                                // sll a3, a3, 1
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L10                                 // beq r0, r0, L10
  c->daddu(t1, a3, t2);                             // daddu t1, a3, t2
  goto block_15;                                    // branch always

  
block_36:
  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L12
  c->load_symbol2(a2, cache.font12_table);          // lw a2, *font12-table*(s7)
  if (bc) {goto block_38;}                          // branch non-likely

  c->mov64(a2, a2);                                 // or a2, a2, r0
  c->addiu(a3, r0, -33);                            // addiu a3, r0, -33
  c->lqc2(vf13, 320, a1);                           // lqc2 vf13, 320(a1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 336, a1);                           // lqc2 vf14, 336(a1)
  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->and_(v1, v1, a3);                              // and v1, v1, a3
  goto block_3;                                     // branch always

  
block_38:
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a2, cache.font24_table);          // lw a2, *font24-table*(s7)
  c->mov64(a2, a2);                                 // or a2, a2, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 368, a1);                           // lqc2 vf13, 368(a1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 384, a1);                           // lqc2 vf14, 384(a1)
  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->ori(v1, v1, 32);                               // ori v1, v1, 32
  goto block_3;                                     // branch always

  
block_39:
  c->addiu(a3, r0, -3);                             // addiu a3, r0, -3
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L9
  c->and_(v1, v1, a3);                              // and v1, v1, a3
  if (bc) {goto block_3;}                           // branch non-likely

  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->ori(v1, v1, 2);                                // ori v1, v1, 2
  goto block_3;                                     // branch always

  
block_41:
  c->mov128_vf_gpr(vf1, t1);                        // qmtc2.i vf1, t1
  c->daddiu(a3, t0, -45);                           // daddiu a3, t0, -45
  bc = c->sgpr64(t0) == 0;                          // beq t0, r0, L16
  c->vitof0(DEST::xyzw, vf1, vf1);                  // vitof0.xyzw vf1, vf1
  if (bc) {goto block_45;}                          // branch non-likely

  bc = c->sgpr64(a3) == 0;                          // beq a3, r0, L15
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_44;}                          // branch non-likely

  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->vadd_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vaddx.x vf23, vf23, vf1
  goto block_3;                                     // branch always

  
block_44:
  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->vsub_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vsubx.x vf23, vf23, vf1
  goto block_3;                                     // branch always

  
block_45:
  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->vadd_bc(DEST::x, BC::x, vf23, vf0, vf1);       // vaddx.x vf23, vf0, vf1
  goto block_3;                                     // branch always

  
block_46:
  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->sqc2(vf23, 496, a1);                           // sqc2 vf23, 496(a1)
  goto block_3;                                     // branch always

  
block_47:
  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->lqc2(vf23, 496, a1);                           // lqc2 vf23, 496(a1)
  goto block_3;                                     // branch always

  
block_48:
  // nop                                            // sll r0, r0, 0
  c->ori(v1, v1, 64);                               // ori v1, v1, 64
  c->lbu(a3, 4, a0);                                // lbu a3, 4(a0)
  c->daddiu(a0, a0, 1);                             // daddiu a0, a0, 1
  //beq r0, r0, L22                                 // beq r0, r0, L22
  c->sll(t0, a3, 4);                                // sll t0, a3, 4
  goto block_52;                                    // branch always

  
block_49:
  // nop                                            // sll r0, r0, 0
  c->addiu(t0, r0, -65);                            // addiu t0, r0, -65
  c->and_(v1, v1, t0);                              // and v1, v1, t0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sll(t0, a3, 4);                                // sll t0, a3, 4
  // nop                                            // sll r0, r0, 0
  c->daddiu(t1, a3, -10);                           // daddiu t1, a3, -10
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L21
  c->daddiu(a3, a3, -13);                           // daddiu a3, a3, -13
  if (bc) {goto block_51;}                          // branch non-likely

  bc = c->sgpr64(a3) != 0;                          // bne a3, r0, L22
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_52;}                          // branch non-likely

  
block_51:
  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->vadd_bc(DEST::x, BC::x, vf23, vf0, vf24);      // vaddx.x vf23, vf0, vf24
  goto block_3;                                     // branch always

  
block_52:
  c->addu(a3, t0, a2);                              // addu a3, t0, a2
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf5, -96, a3);                            // lqc2 vf5, -96(a3)
  // nop                                            // sll r0, r0, 0
  c->vmul(DEST::xyzw, vf19, vf5, vf13);             // vmul.xyzw vf19, vf5, vf13
  c->andi(a3, v1, 2);                               // andi a3, v1, 2
  bc = c->sgpr64(a3) == 0;                          // beq a3, r0, L23
  c->andi(a3, v1, 64);                              // andi a3, v1, 64
  if (bc) {goto block_55;}                          // branch non-likely

  bc = c->sgpr64(a3) != 0;                          // bne a3, r0, L23
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_55;}                          // branch non-likely

  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf19);     // vaddw.x vf23, vf23, vf19
  goto block_3;                                     // branch always

  
block_55:
  //beq r0, r0, L9                                  // beq r0, r0, L9
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf14);     // vaddw.x vf23, vf23, vf14
  goto block_3;                                     // branch always

  
block_56:
  c->mov128_gpr_vf(v0, vf23);                       // qmfc2.i v0, vf23
  //jr ra                                           // jr ra
  c->daddu(sp, sp, r0);                             // daddu sp, sp, r0
  goto end_of_function;                             // return

  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
end_of_function:
  return c->gprs[v0].du64[0];
}

void link() {
  cache.font_work = intern_from_c(-1, 0, "*font-work*").c();
  cache.font12_table = intern_from_c(-1, 0, "*font12-table*").c();
  cache.font24_table = intern_from_c(-1, 0, "*font24-table*").c();
  gLinkedFunctionTable.reg("get-string-length-asm", execute, 512);
}

} // namespace get_string_length_asm
} // namespace Mips2C
// add get_string_length_asm::link to the link callback table for the object file.
// FWD DEC:
namespace get_string_length_asm { extern void link(); }

// clang-format off
using ::jakx::intern_from_c;
namespace Mips2C::jakx {
namespace draw_string3d_asm {
struct Cache {
  void* font_work; // *font-work*
  void* font12_table; // *font12-table*
  void* font24_table; // *font24-table*
  void* draw_string_init_justify; // draw-string-init-justify
} cache;

u64 execute(void* ctxt) {
  auto* c = (ExecutionContext*)ctxt;
  printf("M2C: %s\n", __PRETTY_FUNCTION__);
  bool bc = false;
  u32 call_addr = 0;
  c->daddiu(sp, sp, -64);                           // daddiu sp, sp, -64
  c->sd(ra, 0, sp);                                 // sd ra, 0(sp)
  c->sq(s4, 16, sp);                                // sq s4, 16(sp)
  c->sq(s5, 32, sp);                                // sq s5, 32(sp)
  c->sq(gp, 48, sp);                                // sq gp, 48(sp)
  c->mov64(s5, a0);                                 // or s5, a0, r0
  c->mov64(s4, a1);                                 // or s4, a1, r0
  c->mov64(gp, a2);                                 // or gp, a2, r0
  c->load_symbol2(t9, cache.draw_string_init_justify);// lw t9, draw-string-init-justify(s7)
  c->mov64(a0, s5);                                 // or a0, s5, r0
  c->mov64(a1, s4);                                 // or a1, s4, r0
  c->mov64(a2, gp);                                 // or a2, gp, r0
  call_addr = c->gprs[t9].du32[0];                  // function call:
  c->sll(v0, ra, 0);                                // sll v0, ra, 0
  c->jalr(call_addr);                               // jalr ra, t9
  c->load_symbol2(v1, cache.font_work);             // lw v1, *font-work*(s7)
  c->mov64(v1, v1);                                 // or v1, v1, r0
  c->sw(s4, 6172, v1);                              // sw s4, 6172(v1)
  c->lw(a0, 4, s4);                                 // lw a0, 4(s4)
  c->sw(s5, 6176, v1);                              // sw s5, 6176(v1)
  c->lqc2(vf28, 76, gp);                            // lqc2 vf28, 76(gp)
  c->lqc2(vf29, 92, gp);                            // lqc2 vf29, 92(gp)
  c->lqc2(vf30, 108, gp);                           // lqc2 vf30, 108(gp)
  c->lqc2(vf31, 124, gp);                           // lqc2 vf31, 124(gp)
  c->lqc2(vf16, 416, v1);                           // lqc2 vf16, 416(v1)
  c->lqc2(vf17, 432, v1);                           // lqc2 vf17, 432(v1)
  c->lqc2(vf18, 448, v1);                           // lqc2 vf18, 448(v1)
  c->lqc2(vf27, 4736, v1);                          // lqc2 vf27, 4736(v1)
  c->lqc2(vf26, 4752, v1);                          // lqc2 vf26, 4752(v1)
  c->lqc2(vf25, 156, gp);                           // lqc2 vf25, 156(gp)
  c->lqc2(vf23, 44, gp);                            // lqc2 vf23, 44(gp)
  c->lqc2(vf24, 44, gp);                            // lqc2 vf24, 44(gp)
  c->lqc2(vf1, 44, gp);                             // lqc2 vf1, 44(gp)
  c->lqc2(vf2, 44, gp);                             // lqc2 vf2, 44(gp)
  c->vadd_bc(DEST::x, BC::x, vf1, vf0, vf0);        // vaddx.x vf1, vf0, vf0
  c->vadd_bc(DEST::x, BC::x, vf2, vf0, vf0);        // vaddx.x vf2, vf0, vf0
  c->vadd(DEST::x, vf1, vf0, vf25);                 // vadd.x vf1, vf0, vf25
  c->vmul_bc(DEST::x, BC::w, vf2, vf25, vf16);      // vmulw.x vf2, vf25, vf16
  c->sqc2(vf1, 464, v1);                            // sqc2 vf1, 464(v1)
  c->sqc2(vf2, 480, v1);                            // sqc2 vf2, 480(v1)
  c->lw(a1, 12, gp);                                // lw a1, 12(gp)
  c->vmove(DEST::xyzw, vf1, vf0);                   // vmove.xyzw vf1, vf0
  c->vmove(DEST::xyzw, vf2, vf0);                   // vmove.xyzw vf2, vf0
  c->vmove(DEST::xyzw, vf3, vf0);                   // vmove.xyzw vf3, vf0
  c->vmove(DEST::xyzw, vf4, vf0);                   // vmove.xyzw vf4, vf0
  // nop                                            // sll r0, r0, 0
  c->andi(a1, a1, 32);                              // andi a1, a1, 32
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(a1) != 0;                          // bne a1, r0, L71
  c->load_symbol2(a1, cache.font12_table);          // lw a1, *font12-table*(s7)
  if (bc) {goto block_2;}                           // branch non-likely

  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lq(a2, 192, v1);                               // lq a2, 192(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(a3, 208, v1);                               // lq a3, 208(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t0, 224, v1);                               // lq t0, 224(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 240, v1);                               // lq t1, 240(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a2, 6080, v1);                              // sq a2, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a3, 6096, v1);                              // sq a3, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t0, 6112, v1);                              // sq t0, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 6128, v1);                              // sq t1, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 320, v1);                           // lqc2 vf13, 320(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 336, v1);                           // lqc2 vf14, 336(v1)
  //beq r0, r0, L72                                 // beq r0, r0, L72
  c->lqc2(vf15, 352, v1);                           // lqc2 vf15, 352(v1)
  goto block_3;                                     // branch always

  
block_2:
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a1, cache.font24_table);          // lw a1, *font24-table*(s7)
  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lq(a2, 256, v1);                               // lq a2, 256(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(a3, 272, v1);                               // lq a3, 272(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t0, 288, v1);                               // lq t0, 288(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 304, v1);                               // lq t1, 304(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a2, 6080, v1);                              // sq a2, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a3, 6096, v1);                              // sq a3, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t0, 6112, v1);                              // sq t0, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 6128, v1);                              // sq t1, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 368, v1);                           // lqc2 vf13, 368(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 384, v1);                           // lqc2 vf14, 384(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 400, v1);                           // lqc2 vf15, 400(v1)
  
block_3:
  // nop                                            // sll r0, r0, 0
  c->lb(t0, 180, gp);                               // lb t0, 180(gp)
  c->sll(a3, t0, 4);                                // sll a3, t0, 4
  c->lwu(a2, 8, gp);                                // lwu a2, 8(gp)
  c->daddu(a3, a3, v1);                             // daddu a3, a3, v1
  c->sb(t0, 6168, v1);                              // sb t0, 6168(v1)
  // nop                                            // sll r0, r0, 0
  c->sb(t0, 6169, v1);                              // sb t0, 6169(v1)
  // nop                                            // sll r0, r0, 0
  c->lwu(t0, 4896, a3);                             // lwu t0, 4896(a3)
  // nop                                            // sll r0, r0, 0
  c->lwu(t1, 4900, a3);                             // lwu t1, 4900(a3)
  // nop                                            // sll r0, r0, 0
  c->lwu(a3, 4904, a3);                             // lwu a3, 4904(a3)
  c->pextlb(t0, r0, t0);                            // pextlb t0, r0, t0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pextlh(t0, r0, t0);                            // pextlh t0, r0, t0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pextlb(t1, r0, t1);                            // pextlb t1, r0, t1
  c->sq(t0, 544, v1);                               // sq t0, 544(v1)
  c->pextlh(t1, r0, t1);                            // pextlh t1, r0, t1
  c->sw(a2, 556, v1);                               // sw a2, 556(v1)
  c->pextlb(a3, r0, a3);                            // pextlb a3, r0, a3
  c->sq(t1, 560, v1);                               // sq t1, 560(v1)
  c->pextlh(a3, r0, a3);                            // pextlh a3, r0, a3
  c->sw(a2, 572, v1);                               // sw a2, 572(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a3, 624, v1);                               // sq a3, 624(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t0, 512, v1);                               // sq t0, 512(v1)
  // nop                                            // sll r0, r0, 0
  c->sw(a2, 524, v1);                               // sw a2, 524(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 528, v1);                               // sq t1, 528(v1)
  // nop                                            // sll r0, r0, 0
  c->sw(a2, 540, v1);                               // sw a2, 540(v1)
  // nop                                            // sll r0, r0, 0
  c->sw(a2, 636, v1);                               // sw a2, 636(v1)
  // nop                                            // sll r0, r0, 0
  c->sw(a2, 620, v1);                               // sw a2, 620(v1)
  c->lw(a2, 12, gp);                                // lw a2, 12(gp)
  c->mov64(a3, v1);                                 // or a3, v1, r0
  c->lw(t0, 6176, v1);                              // lw t0, 6176(v1)
  c->lqc2(vf23, 640, a3);                           // lqc2 vf23, 640(a3)
  
block_4:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->lqc2(vf20, 6080, v1);                          // lqc2 vf20, 6080(v1)
  c->andi(t2, a2, 8192);                            // andi t2, a2, 8192
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L74
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_6;}                           // branch non-likely

  c->lqc2(vf20, 6128, v1);                          // lqc2 vf20, 6128(v1)
  // nop                                            // sll r0, r0, 0
  
block_6:
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L98
  c->daddiu(t2, t1, -3);                            // daddiu t2, t1, -3
  if (bc) {goto block_83;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t2)) <= 0;                   // blez t2, L88
  c->daddiu(t2, t1, -126);                          // daddiu t2, t1, -126
  if (bc) {goto block_59;}                          // branch non-likely

  bc = c->sgpr64(t2) != 0;                          // bne t2, r0, L92
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_65;}                          // branch non-likely

  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->addiu(t2, r0, 0);                              // addiu t2, r0, 0
  c->addiu(t3, r0, 0);                              // addiu t3, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L98
  c->daddiu(t4, t1, -43);                           // daddiu t4, t1, -43
  if (bc) {goto block_83;}                          // branch non-likely

  c->movz(t2, t1, t4);                              // movz t2, t1, t4
  c->daddiu(t4, t1, -45);                           // daddiu t4, t1, -45
  c->movz(t2, t1, t4);                              // movz t2, t1, t4
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t2) != 0;                          // bne t2, r0, L75
  c->daddiu(t4, t1, -91);                           // daddiu t4, t1, -91
  if (bc) {goto block_20;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L73
  c->daddiu(t3, t1, -93);                           // daddiu t3, t1, -93
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L73
  c->daddiu(t3, t1, -121);                          // daddiu t3, t1, -121
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L86
  c->daddiu(t3, t1, -89);                           // daddiu t3, t1, -89
  if (bc) {goto block_57;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L86
  c->daddiu(t3, t1, -122);                          // daddiu t3, t1, -122
  if (bc) {goto block_57;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L87
  c->daddiu(t3, t1, -90);                           // daddiu t3, t1, -90
  if (bc) {goto block_58;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L87
  c->daddiu(t3, t1, -48);                           // daddiu t3, t1, -48
  if (bc) {goto block_58;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t3)) < 0;                    // bltz t3, L92
  c->daddiu(t3, t1, -57);                           // daddiu t3, t1, -57
  if (bc) {goto block_65;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t3)) > 0;                    // bgtz t3, L92
  c->daddiu(t3, t1, -126);                          // daddiu t3, t1, -126
  if (bc) {goto block_65;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L92
  c->daddiu(t3, t1, -48);                           // daddiu t3, t1, -48
  if (bc) {goto block_65;}                          // branch non-likely

  
block_20:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L98
  c->daddiu(t4, t1, -110);                          // daddiu t4, t1, -110
  if (bc) {goto block_83;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L76
  c->daddiu(t4, t1, -78);                           // daddiu t4, t1, -78
  if (bc) {goto block_40;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L76
  c->daddiu(t4, t1, -108);                          // daddiu t4, t1, -108
  if (bc) {goto block_40;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L73
  c->daddiu(t4, t1, -76);                           // daddiu t4, t1, -76
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L73
  c->daddiu(t4, t1, -119);                          // daddiu t4, t1, -119
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L78
  c->daddiu(t4, t1, -87);                           // daddiu t4, t1, -87
  if (bc) {goto block_43;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L78
  c->daddiu(t4, t1, -107);                          // daddiu t4, t1, -107
  if (bc) {goto block_43;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L79
  c->daddiu(t4, t1, -75);                           // daddiu t4, t1, -75
  if (bc) {goto block_45;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L79
  c->daddiu(t4, t1, -106);                          // daddiu t4, t1, -106
  if (bc) {goto block_45;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L73
  c->daddiu(t4, t1, -74);                           // daddiu t4, t1, -74
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L73
  c->daddiu(t4, t1, -104);                          // daddiu t4, t1, -104
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L80
  c->daddiu(t4, t1, -72);                           // daddiu t4, t1, -72
  if (bc) {goto block_47;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L80
  c->daddiu(t4, t1, -118);                          // daddiu t4, t1, -118
  if (bc) {goto block_47;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L83
  c->daddiu(t4, t1, -86);                           // daddiu t4, t1, -86
  if (bc) {goto block_52;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L83
  c->daddiu(t4, t1, -117);                          // daddiu t4, t1, -117
  if (bc) {goto block_52;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L73
  c->daddiu(t4, t1, -85);                           // daddiu t4, t1, -85
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L73
  c->daddiu(t4, t1, -48);                           // daddiu t4, t1, -48
  if (bc) {goto block_4;}                           // branch non-likely

  bc = ((s64)c->sgpr64(t4)) < 0;                    // bltz t4, L92
  c->daddiu(t5, t1, -57);                           // daddiu t5, t1, -57
  if (bc) {goto block_65;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t5)) > 0;                    // bgtz t5, L92
  c->sll(t5, t3, 2);                                // sll t5, t3, 2
  if (bc) {goto block_65;}                          // branch non-likely

  c->daddu(t1, t3, t5);                             // daddu t1, t3, t5
  // nop                                            // sll r0, r0, 0
  c->sll(t1, t1, 1);                                // sll t1, t1, 1
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L75                                 // beq r0, r0, L75
  c->daddu(t3, t1, t4);                             // daddu t3, t1, t4
  goto block_20;                                    // branch always

  
block_40:
  bc = c->sgpr64(t3) != 0;                          // bne t3, r0, L77
  c->addiu(t1, r0, -33);                            // addiu t1, r0, -33
  if (bc) {goto block_42;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lq(a1, 192, v1);                               // lq a1, 192(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 208, v1);                               // lq t2, 208(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t3, 224, v1);                               // lq t3, 224(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t4, 240, v1);                               // lq t4, 240(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a1, 6080, v1);                              // sq a1, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 6096, v1);                              // sq t2, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t3, 6112, v1);                              // sq t3, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t4, 6128, v1);                              // sq t4, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a1, cache.font12_table);          // lw a1, *font12-table*(s7)
  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 320, v1);                           // lqc2 vf13, 320(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 336, v1);                           // lqc2 vf14, 336(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 352, v1);                           // lqc2 vf15, 352(v1)
  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->and_(a2, a2, t1);                              // and a2, a2, t1
  goto block_4;                                     // branch always

  
block_42:
  // nop                                            // sll r0, r0, 0
  c->lq(a1, 256, v1);                               // lq a1, 256(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 272, v1);                               // lq t1, 272(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 288, v1);                               // lq t2, 288(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t3, 304, v1);                               // lq t3, 304(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a1, 6080, v1);                              // sq a1, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 6096, v1);                              // sq t1, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 6112, v1);                              // sq t2, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t3, 6128, v1);                              // sq t3, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a1, cache.font24_table);          // lw a1, *font24-table*(s7)
  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 368, v1);                           // lqc2 vf13, 368(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 384, v1);                           // lqc2 vf14, 384(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 400, v1);                           // lqc2 vf15, 400(v1)
  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->ori(a2, a2, 32);                               // ori a2, a2, 32
  goto block_4;                                     // branch always

  
block_43:
  c->addiu(t1, r0, -2);                             // addiu t1, r0, -2
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L73
  c->and_(a2, a2, t1);                              // and a2, a2, t1
  if (bc) {goto block_4;}                           // branch non-likely

  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->ori(a2, a2, 1);                                // ori a2, a2, 1
  goto block_4;                                     // branch always

  
block_45:
  c->addiu(t1, r0, -3);                             // addiu t1, r0, -3
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L73
  c->and_(a2, a2, t1);                              // and a2, a2, t1
  if (bc) {goto block_4;}                           // branch non-likely

  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->ori(a2, a2, 2);                                // ori a2, a2, 2
  goto block_4;                                     // branch always

  
block_47:
  c->mov128_vf_gpr(vf1, t3);                        // qmtc2.i vf1, t3
  c->daddiu(t1, t2, -45);                           // daddiu t1, t2, -45
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L82
  c->vitof0(DEST::xyzw, vf1, vf1);                  // vitof0.xyzw vf1, vf1
  if (bc) {goto block_51;}                          // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L81
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_50;}                          // branch non-likely

  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->vadd_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vaddx.x vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_50:
  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->vsub_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vsubx.x vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_51:
  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->vadd_bc(DEST::x, BC::x, vf23, vf0, vf1);       // vaddx.x vf23, vf0, vf1
  goto block_4;                                     // branch always

  
block_52:
  c->mov128_vf_gpr(vf1, t3);                        // qmtc2.i vf1, t3
  c->daddiu(t1, t2, -45);                           // daddiu t1, t2, -45
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L85
  c->vitof0(DEST::xyzw, vf1, vf1);                  // vitof0.xyzw vf1, vf1
  if (bc) {goto block_56;}                          // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L84
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_55;}                          // branch non-likely

  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->vadd_bc(DEST::y, BC::x, vf23, vf23, vf1);      // vaddx.y vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_55:
  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->vsub_bc(DEST::y, BC::x, vf23, vf23, vf1);      // vsubx.y vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_56:
  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->vadd_bc(DEST::y, BC::x, vf23, vf0, vf1);       // vaddx.y vf23, vf0, vf1
  goto block_4;                                     // branch always

  
block_57:
  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->sqc2(vf23, 496, v1);                           // sqc2 vf23, 496(v1)
  goto block_4;                                     // branch always

  
block_58:
  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->lqc2(vf23, 496, v1);                           // lqc2 vf23, 496(v1)
  goto block_4;                                     // branch always

  
block_59:
  c->daddiu(t2, t1, -3);                            // daddiu t2, t1, -3
  c->ori(a2, a2, 64);                               // ori a2, a2, 64
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L90
  c->daddiu(t1, t1, -2);                            // daddiu t1, t1, -2
  if (bc) {goto block_63;}                          // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L89
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_62;}                          // branch non-likely

  //beq r0, r0, L91                                 // beq r0, r0, L91
  c->lqc2(vf20, 6096, v1);                          // lqc2 vf20, 6096(v1)
  goto block_64;                                    // branch always

  
block_62:
  //beq r0, r0, L91                                 // beq r0, r0, L91
  c->lqc2(vf20, 6112, v1);                          // lqc2 vf20, 6112(v1)
  goto block_64;                                    // branch always

  
block_63:
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf20, 6128, v1);                          // lqc2 vf20, 6128(v1)
  
block_64:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->vadd(DEST::xyz, vf4, vf23, vf15);              // vadd.xyz vf4, vf23, vf15
  c->sll(t2, t1, 4);                                // sll t2, t1, 4
  //beq r0, r0, L94                                 // beq r0, r0, L94
  c->vsub(DEST::xyzw, vf1, vf25, vf23);             // vsub.xyzw vf1, vf25, vf23
  goto block_68;                                    // branch always

  
block_65:
  // nop                                            // sll r0, r0, 0
  c->addiu(t2, r0, -65);                            // addiu t2, r0, -65
  c->and_(a2, a2, t2);                              // and a2, a2, t2
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyz, vf4, vf23, vf15);              // vadd.xyz vf4, vf23, vf15
  c->sll(t2, t1, 4);                                // sll t2, t1, 4
  c->vsub(DEST::xyzw, vf1, vf25, vf23);             // vsub.xyzw vf1, vf25, vf23
  c->daddiu(t3, t1, -10);                           // daddiu t3, t1, -10
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L93
  c->daddiu(t1, t1, -13);                           // daddiu t1, t1, -13
  if (bc) {goto block_67;}                          // branch non-likely

  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L94
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_68;}                          // branch non-likely

  
block_67:
  // nop                                            // sll r0, r0, 0
  c->daddiu(a3, a3, 16);                            // daddiu a3, a3, 16
  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->lqc2(vf23, 640, a3);                           // lqc2 vf23, 640(a3)
  goto block_4;                                     // branch always

  
block_68:
  // nop                                            // sll r0, r0, 0
  c->addu(t1, t2, a1);                              // addu t1, t2, a1
  c->lqc2(vf5, -96, t1);                            // lqc2 vf5, -96(t1)
  c->mov128_gpr_vf(t1, vf1);                        // qmfc2.i t1, vf1
  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L98
  c->vadd(DEST::xyz, vf6, vf5, vf16);               // vadd.xyz vf6, vf5, vf16
  if (bc) {goto block_83;}                          // branch non-likely

  c->sra(t1, t1, 31);                               // sra t1, t1, 31
  c->vadd(DEST::xyz, vf7, vf5, vf17);               // vadd.xyz vf7, vf5, vf17
  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L73
  c->vadd(DEST::xyz, vf8, vf5, vf18);               // vadd.xyz vf8, vf5, vf18
  if (bc) {goto block_4;}                           // branch non-likely

  c->vadd(DEST::xyz, vf1, vf23, vf0);               // vadd.xyz vf1, vf23, vf0
  c->andi(t1, a2, 4097);                            // andi t1, a2, 4097
  c->vadd(DEST::xyz, vf2, vf23, vf13);              // vadd.xyz vf2, vf23, vf13
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyz, vf3, vf23, vf14);              // vadd.xyz vf3, vf23, vf14
  // nop                                            // sll r0, r0, 0
  c->vmul(DEST::xyzw, vf19, vf5, vf13);             // vmul.xyzw vf19, vf5, vf13
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L73
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_4;}                           // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf1);       // vmaddax.xyzw acc, vf28, vf1
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf1);       // vmadday.xyzw acc, vf29, vf1
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf1, vf30, vf1);   // vmaddz.xyzw vf1, vf30, vf1
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf2);       // vmaddax.xyzw acc, vf28, vf2
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf2);       // vmadday.xyzw acc, vf29, vf2
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf2, vf30, vf2);   // vmaddz.xyzw vf2, vf30, vf2
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf3);       // vmaddax.xyzw acc, vf28, vf3
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf3);       // vmadday.xyzw acc, vf29, vf3
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf3, vf30, vf3);   // vmaddz.xyzw vf3, vf30, vf3
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf4);       // vmaddax.xyzw acc, vf28, vf4
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf4);       // vmadday.xyzw acc, vf29, vf4
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf4, vf30, vf4);   // vmaddz.xyzw vf4, vf30, vf4
  c->vdiv(vf25, BC::z, vf1, BC::w);                 // vdiv Q, vf25.z, vf1.w
  c->lq(t1, 64, v1);                                // lq t1, 64(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 80, v1);                                // lq t2, 80(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 0, a0);                                 // sq t1, 0(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 16, a0);                                // sq t2, 16(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf1, vf1);                    // vmulq.xyz vf1, vf1, Q
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  c->vdiv(vf25, BC::z, vf2, BC::w);                 // vdiv Q, vf25.z, vf2.w
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf1, 4832, v1);                           // sqc2 vf1, 4832(v1)
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf1, vf1, vf26);              // vadd.xyzw vf1, vf1, vf26
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 608, v1);                            // lqc2 vf9, 608(v1)
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf1, vf1);                   // vftoi4.xyw vf1, vf1
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf1, vf1);                     // vftoi0.z vf1, vf1
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf2, vf2);                    // vmulq.xyz vf2, vf2, Q
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  c->vdiv(vf25, BC::z, vf3, BC::w);                 // vdiv Q, vf25.z, vf3.w
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 128, a0);                            // sqc2 vf9, 128(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf1, 80, a0);                             // sqc2 vf1, 80(a0)
  c->sqc2(vf2, 4848, v1);                           // sqc2 vf2, 4848(v1)
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf2, vf2, vf26);              // vadd.xyzw vf2, vf2, vf26
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf2, vf2);                   // vftoi4.xyw vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf2, vf2);                     // vftoi0.z vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf3, vf3);                    // vmulq.xyz vf3, vf3, Q
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf6, 96, a0);                             // sqc2 vf6, 96(a0)
  c->vdiv(vf25, BC::z, vf4, BC::w);                 // vdiv Q, vf25.z, vf4.w
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 112, a0);                            // sqc2 vf2, 112(a0)
  c->sqc2(vf3, 4864, v1);                           // sqc2 vf3, 4864(v1)
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf3, vf3, vf26);              // vadd.xyzw vf3, vf3, vf26
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf3, vf3);                   // vftoi4.xyw vf3, vf3
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf3, vf3);                     // vftoi0.z vf3, vf3
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf4, vf4);                    // vmulq.xyz vf4, vf4, Q
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf7, 144, a0);                            // sqc2 vf7, 144(a0)
  c->sqc2(vf4, 4880, v1);                           // sqc2 vf4, 4880(v1)
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf4, vf4, vf26);              // vadd.xyzw vf4, vf4, vf26
  c->sqc2(vf3, 160, a0);                            // sqc2 vf3, 160(a0)
  c->andi(t1, a2, 2);                               // andi t1, a2, 2
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L95
  c->andi(t1, a2, 64);                              // andi t1, a2, 64
  if (bc) {goto block_74;}                          // branch non-likely

  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L95
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_74;}                          // branch non-likely

  //beq r0, r0, L96                                 // beq r0, r0, L96
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf19);     // vaddw.x vf23, vf23, vf19
  goto block_75;                                    // branch always

  
block_74:
  // nop                                            // sll r0, r0, 0
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf14);     // vaddw.x vf23, vf23, vf14
  
block_75:
  c->vftoi4(DEST::xyw, vf4, vf4);                   // vftoi4.xyw vf4, vf4
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf4, vf4);                     // vftoi0.z vf4, vf4
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 176, a0);                            // sqc2 vf8, 176(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf4, 192, a0);                            // sqc2 vf4, 192(a0)
  // nop                                            // sll r0, r0, 0
  c->lw(t2, 80, a0);                                // lw t2, 80(a0)
  // nop                                            // sll r0, r0, 0
  c->lw(t1, 84, a0);                                // lw t1, 84(a0)
  c->ori(t3, r0, 36864);                            // ori t3, r0, 36864
  c->dsubu(t2, t2, t3);                             // dsubu t2, t2, t3
  c->lw(t3, 192, a0);                               // lw t3, 192(a0)
  c->ori(t4, r0, 36096);                            // ori t4, r0, 36096
  c->dsubu(t1, t1, t4);                             // dsubu t1, t1, t4
  c->lw(t4, 196, a0);                               // lw t4, 196(a0)
  bc = ((s64)c->sgpr64(t2)) > 0;                    // bgtz t2, L73
  c->daddiu(t2, t3, -28672);                        // daddiu t2, t3, -28672
  if (bc) {goto block_4;}                           // branch non-likely

  bc = ((s64)c->sgpr64(t1)) > 0;                    // bgtz t1, L73
  c->daddiu(t1, t4, -29440);                        // daddiu t1, t4, -29440
  if (bc) {goto block_4;}                           // branch non-likely

  bc = ((s64)c->sgpr64(t2)) < 0;                    // bltz t2, L73
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_4;}                           // branch non-likely

  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L73
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_4;}                           // branch non-likely

  c->andi(t1, a2, 1);                               // andi t1, a2, 1
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L97
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_81;}                          // branch non-likely

  c->daddiu(a0, a0, 208);                           // daddiu a0, a0, 208
  // nop                                            // sll r0, r0, 0
  
block_81:
  c->andi(t1, a2, 4096);                            // andi t1, a2, 4096
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L73
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_4;}                           // branch non-likely

  c->lqc2(vf1, 4832, v1);                           // lqc2 vf1, 4832(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 624, v1);                            // lqc2 vf9, 624(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf26, 4768, v1);                          // lqc2 vf26, 4768(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 64, v1);                                // lq t1, 64(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 80, v1);                                // lq t2, 80(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 0, a0);                                 // sq t1, 0(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 16, a0);                                // sq t2, 16(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  c->lqc2(vf2, 4848, v1);                           // lqc2 vf2, 4848(v1)
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf1, vf1, vf26);              // vadd.xyzw vf1, vf1, vf26
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf1, vf1);                   // vftoi4.xyw vf1, vf1
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf1, vf1);                     // vftoi0.z vf1, vf1
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 128, a0);                            // sqc2 vf9, 128(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf1, 80, a0);                             // sqc2 vf1, 80(a0)
  c->vadd(DEST::xyzw, vf2, vf2, vf26);              // vadd.xyzw vf2, vf2, vf26
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf3, 4864, v1);                           // lqc2 vf3, 4864(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf2, vf2);                   // vftoi4.xyw vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf2, vf2);                     // vftoi0.z vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf4, 4880, v1);                           // lqc2 vf4, 4880(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf6, 96, a0);                             // sqc2 vf6, 96(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 112, a0);                            // sqc2 vf2, 112(a0)
  c->vadd(DEST::xyzw, vf3, vf3, vf26);              // vadd.xyzw vf3, vf3, vf26
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf3, vf3);                   // vftoi4.xyw vf3, vf3
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf3, vf3);                     // vftoi0.z vf3, vf3
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf7, 144, a0);                            // sqc2 vf7, 144(a0)
  c->vadd(DEST::xyzw, vf4, vf4, vf26);              // vadd.xyzw vf4, vf4, vf26
  c->sqc2(vf3, 160, a0);                            // sqc2 vf3, 160(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf4, vf4);                   // vftoi4.xyw vf4, vf4
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf4, vf4);                     // vftoi0.z vf4, vf4
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 176, a0);                            // sqc2 vf8, 176(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf4, 192, a0);                            // sqc2 vf4, 192(a0)
  c->lqc2(vf1, 4832, v1);                           // lqc2 vf1, 4832(v1)
  c->daddiu(a0, a0, 208);                           // daddiu a0, a0, 208
  c->lqc2(vf26, 4784, v1);                          // lqc2 vf26, 4784(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 64, v1);                                // lq t1, 64(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 80, v1);                                // lq t2, 80(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 0, a0);                                 // sq t1, 0(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 16, a0);                                // sq t2, 16(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  c->lqc2(vf2, 4848, v1);                           // lqc2 vf2, 4848(v1)
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf1, vf1, vf26);              // vadd.xyzw vf1, vf1, vf26
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf1, vf1);                   // vftoi4.xyw vf1, vf1
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf1, vf1);                     // vftoi0.z vf1, vf1
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 128, a0);                            // sqc2 vf9, 128(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf1, 80, a0);                             // sqc2 vf1, 80(a0)
  c->vadd(DEST::xyzw, vf2, vf2, vf26);              // vadd.xyzw vf2, vf2, vf26
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf3, 4864, v1);                           // lqc2 vf3, 4864(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf2, vf2);                   // vftoi4.xyw vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf2, vf2);                     // vftoi0.z vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf4, 4880, v1);                           // lqc2 vf4, 4880(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf6, 96, a0);                             // sqc2 vf6, 96(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 112, a0);                            // sqc2 vf2, 112(a0)
  c->vadd(DEST::xyzw, vf3, vf3, vf26);              // vadd.xyzw vf3, vf3, vf26
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf3, vf3);                   // vftoi4.xyw vf3, vf3
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf3, vf3);                     // vftoi0.z vf3, vf3
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf7, 144, a0);                            // sqc2 vf7, 144(a0)
  c->vadd(DEST::xyzw, vf4, vf4, vf26);              // vadd.xyzw vf4, vf4, vf26
  c->sqc2(vf3, 160, a0);                            // sqc2 vf3, 160(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf4, vf4);                   // vftoi4.xyw vf4, vf4
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf4, vf4);                     // vftoi0.z vf4, vf4
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 176, a0);                            // sqc2 vf8, 176(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf4, 192, a0);                            // sqc2 vf4, 192(a0)
  c->lqc2(vf1, 4832, v1);                           // lqc2 vf1, 4832(v1)
  c->daddiu(a0, a0, 208);                           // daddiu a0, a0, 208
  c->lqc2(vf26, 4800, v1);                          // lqc2 vf26, 4800(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 64, v1);                                // lq t1, 64(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 80, v1);                                // lq t2, 80(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 0, a0);                                 // sq t1, 0(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 16, a0);                                // sq t2, 16(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  c->lqc2(vf2, 4848, v1);                           // lqc2 vf2, 4848(v1)
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf1, vf1, vf26);              // vadd.xyzw vf1, vf1, vf26
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf1, vf1);                   // vftoi4.xyw vf1, vf1
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf1, vf1);                     // vftoi0.z vf1, vf1
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 128, a0);                            // sqc2 vf9, 128(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf1, 80, a0);                             // sqc2 vf1, 80(a0)
  c->vadd(DEST::xyzw, vf2, vf2, vf26);              // vadd.xyzw vf2, vf2, vf26
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf3, 4864, v1);                           // lqc2 vf3, 4864(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf2, vf2);                   // vftoi4.xyw vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf2, vf2);                     // vftoi0.z vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf4, 4880, v1);                           // lqc2 vf4, 4880(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf6, 96, a0);                             // sqc2 vf6, 96(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 112, a0);                            // sqc2 vf2, 112(a0)
  c->vadd(DEST::xyzw, vf3, vf3, vf26);              // vadd.xyzw vf3, vf3, vf26
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf3, vf3);                   // vftoi4.xyw vf3, vf3
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf3, vf3);                     // vftoi0.z vf3, vf3
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf7, 144, a0);                            // sqc2 vf7, 144(a0)
  c->vadd(DEST::xyzw, vf4, vf4, vf26);              // vadd.xyzw vf4, vf4, vf26
  c->sqc2(vf3, 160, a0);                            // sqc2 vf3, 160(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf4, vf4);                   // vftoi4.xyw vf4, vf4
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf4, vf4);                     // vftoi0.z vf4, vf4
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 176, a0);                            // sqc2 vf8, 176(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf4, 192, a0);                            // sqc2 vf4, 192(a0)
  c->lqc2(vf1, 4832, v1);                           // lqc2 vf1, 4832(v1)
  c->daddiu(a0, a0, 208);                           // daddiu a0, a0, 208
  c->lqc2(vf26, 4816, v1);                          // lqc2 vf26, 4816(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 64, v1);                                // lq t1, 64(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 80, v1);                                // lq t2, 80(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 0, a0);                                 // sq t1, 0(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 16, a0);                                // sq t2, 16(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  c->lqc2(vf2, 4848, v1);                           // lqc2 vf2, 4848(v1)
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf1, vf1, vf26);              // vadd.xyzw vf1, vf1, vf26
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf1, vf1);                   // vftoi4.xyw vf1, vf1
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf1, vf1);                     // vftoi0.z vf1, vf1
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 128, a0);                            // sqc2 vf9, 128(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf1, 80, a0);                             // sqc2 vf1, 80(a0)
  c->vadd(DEST::xyzw, vf2, vf2, vf26);              // vadd.xyzw vf2, vf2, vf26
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf3, 4864, v1);                           // lqc2 vf3, 4864(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf2, vf2);                   // vftoi4.xyw vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf2, vf2);                     // vftoi0.z vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf4, 4880, v1);                           // lqc2 vf4, 4880(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf6, 96, a0);                             // sqc2 vf6, 96(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 112, a0);                            // sqc2 vf2, 112(a0)
  c->vadd(DEST::xyzw, vf3, vf3, vf26);              // vadd.xyzw vf3, vf3, vf26
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf3, vf3);                   // vftoi4.xyw vf3, vf3
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf3, vf3);                     // vftoi0.z vf3, vf3
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf7, 144, a0);                            // sqc2 vf7, 144(a0)
  c->vadd(DEST::xyzw, vf4, vf4, vf26);              // vadd.xyzw vf4, vf4, vf26
  c->sqc2(vf3, 160, a0);                            // sqc2 vf3, 160(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf4, vf4);                   // vftoi4.xyw vf4, vf4
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf4, vf4);                     // vftoi0.z vf4, vf4
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 176, a0);                            // sqc2 vf8, 176(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf4, 192, a0);                            // sqc2 vf4, 192(a0)
  c->lqc2(vf26, 4752, v1);                          // lqc2 vf26, 4752(v1)
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L73                                 // beq r0, r0, L73
  c->daddiu(a0, a0, 208);                           // daddiu a0, a0, 208
  goto block_4;                                     // branch always

  
block_83:
  c->lw(a2, 12, gp);                                // lw a2, 12(gp)
  c->mov64(a3, v1);                                 // or a3, v1, r0
  c->lw(t0, 6176, v1);                              // lw t0, 6176(v1)
  c->lqc2(vf23, 640, a3);                           // lqc2 vf23, 640(a3)
  
block_84:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->lqc2(vf20, 6080, v1);                          // lqc2 vf20, 6080(v1)
  c->andi(t2, a2, 8192);                            // andi t2, a2, 8192
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L100
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_86;}                          // branch non-likely

  c->lqc2(vf20, 6128, v1);                          // lqc2 vf20, 6128(v1)
  // nop                                            // sll r0, r0, 0
  
block_86:
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L126
  c->daddiu(t2, t1, -3);                            // daddiu t2, t1, -3
  if (bc) {goto block_163;}                         // branch non-likely

  bc = ((s64)c->sgpr64(t2)) <= 0;                   // blez t2, L117
  c->daddiu(t2, t1, -126);                          // daddiu t2, t1, -126
  if (bc) {goto block_143;}                         // branch non-likely

  bc = c->sgpr64(t2) != 0;                          // bne t2, r0, L121
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_149;}                         // branch non-likely

  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->addiu(t2, r0, 0);                              // addiu t2, r0, 0
  c->addiu(t3, r0, 0);                              // addiu t3, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L126
  c->daddiu(t4, t1, -43);                           // daddiu t4, t1, -43
  if (bc) {goto block_163;}                         // branch non-likely

  c->movz(t2, t1, t4);                              // movz t2, t1, t4
  c->daddiu(t4, t1, -45);                           // daddiu t4, t1, -45
  c->movz(t2, t1, t4);                              // movz t2, t1, t4
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t2) != 0;                          // bne t2, r0, L101
  c->daddiu(t4, t1, -91);                           // daddiu t4, t1, -91
  if (bc) {goto block_100;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L113
  c->daddiu(t3, t1, -93);                           // daddiu t3, t1, -93
  if (bc) {goto block_139;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L114
  c->daddiu(t3, t1, -121);                          // daddiu t3, t1, -121
  if (bc) {goto block_140;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L115
  c->daddiu(t3, t1, -89);                           // daddiu t3, t1, -89
  if (bc) {goto block_141;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L115
  c->daddiu(t3, t1, -122);                          // daddiu t3, t1, -122
  if (bc) {goto block_141;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L116
  c->daddiu(t3, t1, -90);                           // daddiu t3, t1, -90
  if (bc) {goto block_142;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L116
  c->daddiu(t3, t1, -48);                           // daddiu t3, t1, -48
  if (bc) {goto block_142;}                         // branch non-likely

  bc = ((s64)c->sgpr64(t3)) < 0;                    // bltz t3, L121
  c->daddiu(t3, t1, -57);                           // daddiu t3, t1, -57
  if (bc) {goto block_149;}                         // branch non-likely

  bc = ((s64)c->sgpr64(t3)) > 0;                    // bgtz t3, L121
  c->daddiu(t3, t1, -126);                          // daddiu t3, t1, -126
  if (bc) {goto block_149;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L121
  c->daddiu(t3, t1, -48);                           // daddiu t3, t1, -48
  if (bc) {goto block_149;}                         // branch non-likely

  
block_100:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L126
  c->daddiu(t4, t1, -110);                          // daddiu t4, t1, -110
  if (bc) {goto block_163;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L102
  c->daddiu(t4, t1, -78);                           // daddiu t4, t1, -78
  if (bc) {goto block_120;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L102
  c->daddiu(t4, t1, -108);                          // daddiu t4, t1, -108
  if (bc) {goto block_120;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L104
  c->daddiu(t4, t1, -76);                           // daddiu t4, t1, -76
  if (bc) {goto block_123;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L104
  c->daddiu(t4, t1, -119);                          // daddiu t4, t1, -119
  if (bc) {goto block_123;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L99
  c->daddiu(t4, t1, -87);                           // daddiu t4, t1, -87
  if (bc) {goto block_84;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L99
  c->daddiu(t4, t1, -107);                          // daddiu t4, t1, -107
  if (bc) {goto block_84;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L106
  c->daddiu(t4, t1, -75);                           // daddiu t4, t1, -75
  if (bc) {goto block_127;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L106
  c->daddiu(t4, t1, -106);                          // daddiu t4, t1, -106
  if (bc) {goto block_127;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L99
  c->daddiu(t4, t1, -74);                           // daddiu t4, t1, -74
  if (bc) {goto block_84;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L99
  c->daddiu(t4, t1, -104);                          // daddiu t4, t1, -104
  if (bc) {goto block_84;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L107
  c->daddiu(t4, t1, -72);                           // daddiu t4, t1, -72
  if (bc) {goto block_129;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L107
  c->daddiu(t4, t1, -118);                          // daddiu t4, t1, -118
  if (bc) {goto block_129;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L110
  c->daddiu(t4, t1, -86);                           // daddiu t4, t1, -86
  if (bc) {goto block_134;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L110
  c->daddiu(t4, t1, -117);                          // daddiu t4, t1, -117
  if (bc) {goto block_134;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L105
  c->daddiu(t4, t1, -85);                           // daddiu t4, t1, -85
  if (bc) {goto block_125;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L105
  c->daddiu(t4, t1, -48);                           // daddiu t4, t1, -48
  if (bc) {goto block_125;}                         // branch non-likely

  bc = ((s64)c->sgpr64(t4)) < 0;                    // bltz t4, L121
  c->daddiu(t5, t1, -57);                           // daddiu t5, t1, -57
  if (bc) {goto block_149;}                         // branch non-likely

  bc = ((s64)c->sgpr64(t5)) > 0;                    // bgtz t5, L121
  c->sll(t5, t3, 2);                                // sll t5, t3, 2
  if (bc) {goto block_149;}                         // branch non-likely

  c->daddu(t1, t3, t5);                             // daddu t1, t3, t5
  // nop                                            // sll r0, r0, 0
  c->sll(t1, t1, 1);                                // sll t1, t1, 1
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L101                                // beq r0, r0, L101
  c->daddu(t3, t1, t4);                             // daddu t3, t1, t4
  goto block_100;                                   // branch always

  
block_120:
  bc = c->sgpr64(t3) != 0;                          // bne t3, r0, L103
  c->addiu(t1, r0, -33);                            // addiu t1, r0, -33
  if (bc) {goto block_122;}                         // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lq(a1, 192, v1);                               // lq a1, 192(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 208, v1);                               // lq t2, 208(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t3, 224, v1);                               // lq t3, 224(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t4, 240, v1);                               // lq t4, 240(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a1, 6080, v1);                              // sq a1, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 6096, v1);                              // sq t2, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t3, 6112, v1);                              // sq t3, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t4, 6128, v1);                              // sq t4, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a1, cache.font12_table);          // lw a1, *font12-table*(s7)
  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 320, v1);                           // lqc2 vf13, 320(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 336, v1);                           // lqc2 vf14, 336(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 352, v1);                           // lqc2 vf15, 352(v1)
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->and_(a2, a2, t1);                              // and a2, a2, t1
  goto block_84;                                    // branch always

  
block_122:
  // nop                                            // sll r0, r0, 0
  c->lq(a1, 256, v1);                               // lq a1, 256(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 272, v1);                               // lq t1, 272(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 288, v1);                               // lq t2, 288(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t3, 304, v1);                               // lq t3, 304(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a1, 6080, v1);                              // sq a1, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 6096, v1);                              // sq t1, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 6112, v1);                              // sq t2, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t3, 6128, v1);                              // sq t3, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a1, cache.font24_table);          // lw a1, *font24-table*(s7)
  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 368, v1);                           // lqc2 vf13, 368(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 384, v1);                           // lqc2 vf14, 384(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 400, v1);                           // lqc2 vf15, 400(v1)
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->ori(a2, a2, 32);                               // ori a2, a2, 32
  goto block_84;                                    // branch always

  
block_123:
  c->andi(t1, a2, 128);                             // andi t1, a2, 128
  c->sll(t2, t3, 4);                                // sll t2, t3, 4
  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L99
  c->sw(t3, 6168, v1);                              // sw t3, 6168(v1)
  if (bc) {goto block_84;}                          // branch non-likely

  c->daddu(t2, t2, v1);                             // daddu t2, t2, v1
  c->lwu(t1, 8, gp);                                // lwu t1, 8(gp)
  // nop                                            // sll r0, r0, 0
  c->lwu(t3, 4896, t2);                             // lwu t3, 4896(t2)
  // nop                                            // sll r0, r0, 0
  c->lwu(t2, 4900, t2);                             // lwu t2, 4900(t2)
  c->pextlb(t3, r0, t3);                            // pextlb t3, r0, t3
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pextlh(t3, r0, t3);                            // pextlh t3, r0, t3
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pextlb(t2, r0, t2);                            // pextlb t2, r0, t2
  c->sq(t3, 544, v1);                               // sq t3, 544(v1)
  c->pextlh(t2, r0, t2);                            // pextlh t2, r0, t2
  c->sw(t1, 556, v1);                               // sw t1, 556(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 560, v1);                               // sq t2, 560(v1)
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->sw(t1, 572, v1);                               // sw t1, 572(v1)
  goto block_84;                                    // branch always

  
block_125:
  c->andi(t1, a2, 128);                             // andi t1, a2, 128
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L99
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_84;}                          // branch non-likely

  c->pextlb(t2, r0, t3);                            // pextlb t2, r0, t3
  c->lw(t1, 8, gp);                                 // lw t1, 8(gp)
  c->pextlh(t2, r0, t2);                            // pextlh t2, r0, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 544, v1);                               // sq t2, 544(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 560, v1);                               // sq t2, 560(v1)
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->sw(t1, 556, v1);                               // sw t1, 556(v1)
  goto block_84;                                    // branch always

  
block_127:
  c->addiu(t1, r0, -3);                             // addiu t1, r0, -3
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L99
  c->and_(a2, a2, t1);                              // and a2, a2, t1
  if (bc) {goto block_84;}                          // branch non-likely

  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->ori(a2, a2, 2);                                // ori a2, a2, 2
  goto block_84;                                    // branch always

  
block_129:
  c->mov128_vf_gpr(vf1, t3);                        // qmtc2.i vf1, t3
  c->daddiu(t1, t2, -45);                           // daddiu t1, t2, -45
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L109
  c->vitof0(DEST::xyzw, vf1, vf1);                  // vitof0.xyzw vf1, vf1
  if (bc) {goto block_133;}                         // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L108
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_132;}                         // branch non-likely

  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->vadd_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vaddx.x vf23, vf23, vf1
  goto block_84;                                    // branch always

  
block_132:
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->vsub_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vsubx.x vf23, vf23, vf1
  goto block_84;                                    // branch always

  
block_133:
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->vadd_bc(DEST::x, BC::x, vf23, vf0, vf1);       // vaddx.x vf23, vf0, vf1
  goto block_84;                                    // branch always

  
block_134:
  c->mov128_vf_gpr(vf1, t3);                        // qmtc2.i vf1, t3
  c->daddiu(t1, t2, -45);                           // daddiu t1, t2, -45
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L112
  c->vitof0(DEST::xyzw, vf1, vf1);                  // vitof0.xyzw vf1, vf1
  if (bc) {goto block_138;}                         // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L111
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_137;}                         // branch non-likely

  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->vadd_bc(DEST::y, BC::x, vf23, vf23, vf1);      // vaddx.y vf23, vf23, vf1
  goto block_84;                                    // branch always

  
block_137:
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->vsub_bc(DEST::y, BC::x, vf23, vf23, vf1);      // vsubx.y vf23, vf23, vf1
  goto block_84;                                    // branch always

  
block_138:
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->vadd_bc(DEST::y, BC::x, vf23, vf0, vf1);       // vaddx.y vf23, vf0, vf1
  goto block_84;                                    // branch always

  
block_139:
  // nop                                            // sll r0, r0, 0
  c->lb(t1, 6168, v1);                              // lb t1, 6168(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 544, v1);                            // lqc2 vf9, 544(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf10, 560, v1);                           // lqc2 vf10, 560(v1)
  // nop                                            // sll r0, r0, 0
  c->sb(t1, 6169, v1);                              // sb t1, 6169(v1)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 512, v1);                            // sqc2 vf9, 512(v1)
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->sqc2(vf10, 528, v1);                           // sqc2 vf10, 528(v1)
  goto block_84;                                    // branch always

  
block_140:
  // nop                                            // sll r0, r0, 0
  c->lb(t1, 6169, v1);                              // lb t1, 6169(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 512, v1);                            // lqc2 vf9, 512(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf10, 528, v1);                           // lqc2 vf10, 528(v1)
  // nop                                            // sll r0, r0, 0
  c->sb(t1, 6168, v1);                              // sb t1, 6168(v1)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 544, v1);                            // sqc2 vf9, 544(v1)
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->sqc2(vf10, 560, v1);                           // sqc2 vf10, 560(v1)
  goto block_84;                                    // branch always

  
block_141:
  // nop                                            // sll r0, r0, 0
  c->lb(t1, 6168, v1);                              // lb t1, 6168(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 544, v1);                            // lqc2 vf9, 544(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf10, 560, v1);                           // lqc2 vf10, 560(v1)
  // nop                                            // sll r0, r0, 0
  c->sb(t1, 6169, v1);                              // sb t1, 6169(v1)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 512, v1);                            // sqc2 vf9, 512(v1)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf10, 528, v1);                           // sqc2 vf10, 528(v1)
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->sqc2(vf23, 496, v1);                           // sqc2 vf23, 496(v1)
  goto block_84;                                    // branch always

  
block_142:
  // nop                                            // sll r0, r0, 0
  c->lb(t1, 6169, v1);                              // lb t1, 6169(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 512, v1);                            // lqc2 vf9, 512(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf10, 528, v1);                           // lqc2 vf10, 528(v1)
  // nop                                            // sll r0, r0, 0
  c->sb(t1, 6168, v1);                              // sb t1, 6168(v1)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 544, v1);                            // sqc2 vf9, 544(v1)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf10, 560, v1);                           // sqc2 vf10, 560(v1)
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->lqc2(vf23, 496, v1);                           // lqc2 vf23, 496(v1)
  goto block_84;                                    // branch always

  
block_143:
  c->daddiu(t2, t1, -3);                            // daddiu t2, t1, -3
  c->ori(a2, a2, 64);                               // ori a2, a2, 64
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L119
  c->daddiu(t1, t1, -2);                            // daddiu t1, t1, -2
  if (bc) {goto block_147;}                         // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L118
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_146;}                         // branch non-likely

  //beq r0, r0, L120                                // beq r0, r0, L120
  c->lqc2(vf20, 6096, v1);                          // lqc2 vf20, 6096(v1)
  goto block_148;                                   // branch always

  
block_146:
  //beq r0, r0, L120                                // beq r0, r0, L120
  c->lqc2(vf20, 6112, v1);                          // lqc2 vf20, 6112(v1)
  goto block_148;                                   // branch always

  
block_147:
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf20, 6128, v1);                          // lqc2 vf20, 6128(v1)
  
block_148:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->vadd(DEST::xyz, vf4, vf23, vf15);              // vadd.xyz vf4, vf23, vf15
  c->sll(t2, t1, 4);                                // sll t2, t1, 4
  //beq r0, r0, L123                                // beq r0, r0, L123
  c->vsub(DEST::xyzw, vf1, vf25, vf23);             // vsub.xyzw vf1, vf25, vf23
  goto block_152;                                   // branch always

  
block_149:
  // nop                                            // sll r0, r0, 0
  c->addiu(t2, r0, -65);                            // addiu t2, r0, -65
  c->and_(a2, a2, t2);                              // and a2, a2, t2
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyz, vf4, vf23, vf15);              // vadd.xyz vf4, vf23, vf15
  c->sll(t2, t1, 4);                                // sll t2, t1, 4
  c->vsub(DEST::xyzw, vf1, vf25, vf23);             // vsub.xyzw vf1, vf25, vf23
  c->daddiu(t3, t1, -10);                           // daddiu t3, t1, -10
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L122
  c->daddiu(t1, t1, -13);                           // daddiu t1, t1, -13
  if (bc) {goto block_151;}                         // branch non-likely

  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L123
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_152;}                         // branch non-likely

  
block_151:
  c->daddiu(a3, a3, 16);                            // daddiu a3, a3, 16
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->lqc2(vf23, 640, a3);                           // lqc2 vf23, 640(a3)
  goto block_84;                                    // branch always

  
block_152:
  c->addu(t1, t2, a1);                              // addu t1, t2, a1
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf5, -96, t1);                            // lqc2 vf5, -96(t1)
  c->mov128_gpr_vf(t1, vf1);                        // qmfc2.i t1, vf1
  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L126
  c->vadd(DEST::xyz, vf6, vf5, vf16);               // vadd.xyz vf6, vf5, vf16
  if (bc) {goto block_163;}                         // branch non-likely

  c->sra(t1, t1, 31);                               // sra t1, t1, 31
  c->vadd(DEST::xyz, vf7, vf5, vf17);               // vadd.xyz vf7, vf5, vf17
  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L99
  c->vadd(DEST::xyz, vf8, vf5, vf18);               // vadd.xyz vf8, vf5, vf18
  if (bc) {goto block_84;}                          // branch non-likely

  c->vadd(DEST::xyz, vf1, vf23, vf0);               // vadd.xyz vf1, vf23, vf0
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyz, vf2, vf23, vf13);              // vadd.xyz vf2, vf23, vf13
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyz, vf3, vf23, vf14);              // vadd.xyz vf3, vf23, vf14
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyz, vf4, vf23, vf15);              // vadd.xyz vf4, vf23, vf15
  // nop                                            // sll r0, r0, 0
  c->vmul(DEST::xyzw, vf19, vf5, vf13);             // vmul.xyzw vf19, vf5, vf13
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf1);       // vmaddax.xyzw acc, vf28, vf1
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf1);       // vmadday.xyzw acc, vf29, vf1
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf1, vf30, vf1);   // vmaddz.xyzw vf1, vf30, vf1
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf2);       // vmaddax.xyzw acc, vf28, vf2
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf2);       // vmadday.xyzw acc, vf29, vf2
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf2, vf30, vf2);   // vmaddz.xyzw vf2, vf30, vf2
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf3);       // vmaddax.xyzw acc, vf28, vf3
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf3);       // vmadday.xyzw acc, vf29, vf3
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf3, vf30, vf3);   // vmaddz.xyzw vf3, vf30, vf3
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf4);       // vmaddax.xyzw acc, vf28, vf4
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf4);       // vmadday.xyzw acc, vf29, vf4
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf4, vf30, vf4);   // vmaddz.xyzw vf4, vf30, vf4
  c->vdiv(vf25, BC::z, vf1, BC::w);                 // vdiv Q, vf25.z, vf1.w
  c->lq(t1, 64, v1);                                // lq t1, 64(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 80, v1);                                // lq t2, 80(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 0, a0);                                 // sq t1, 0(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 16, a0);                                // sq t2, 16(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf1, vf1);                    // vmulq.xyz vf1, vf1, Q
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  c->vdiv(vf25, BC::z, vf2, BC::w);                 // vdiv Q, vf25.z, vf2.w
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf1, vf1, vf27);              // vadd.xyzw vf1, vf1, vf27
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 544, v1);                            // lqc2 vf9, 544(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf10, 560, v1);                           // lqc2 vf10, 560(v1)
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf1, vf1);                   // vftoi4.xyw vf1, vf1
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf1, vf1);                     // vftoi0.z vf1, vf1
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf2, vf2);                    // vmulq.xyz vf2, vf2, Q
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  c->vdiv(vf25, BC::z, vf3, BC::w);                 // vdiv Q, vf25.z, vf3.w
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf10, 128, a0);                           // sqc2 vf10, 128(a0)
  c->vadd(DEST::xyzw, vf2, vf2, vf27);              // vadd.xyzw vf2, vf2, vf27
  c->sqc2(vf1, 80, a0);                             // sqc2 vf1, 80(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf2, vf2);                   // vftoi4.xyw vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf2, vf2);                     // vftoi0.z vf2, vf2
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf3, vf3);                    // vmulq.xyz vf3, vf3, Q
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf6, 96, a0);                             // sqc2 vf6, 96(a0)
  c->vdiv(vf25, BC::z, vf4, BC::w);                 // vdiv Q, vf25.z, vf4.w
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 112, a0);                            // sqc2 vf2, 112(a0)
  c->vadd(DEST::xyzw, vf3, vf3, vf27);              // vadd.xyzw vf3, vf3, vf27
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyw, vf3, vf3);                   // vftoi4.xyw vf3, vf3
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf3, vf3);                     // vftoi0.z vf3, vf3
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf4, vf4);                    // vmulq.xyz vf4, vf4, Q
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf7, 144, a0);                            // sqc2 vf7, 144(a0)
  c->vadd(DEST::xyzw, vf4, vf4, vf27);              // vadd.xyzw vf4, vf4, vf27
  c->sqc2(vf3, 160, a0);                            // sqc2 vf3, 160(a0)
  c->andi(t1, a2, 2);                               // andi t1, a2, 2
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L124
  c->andi(t1, a2, 64);                              // andi t1, a2, 64
  if (bc) {goto block_157;}                         // branch non-likely

  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L124
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_157;}                         // branch non-likely

  //beq r0, r0, L125                                // beq r0, r0, L125
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf19);     // vaddw.x vf23, vf23, vf19
  goto block_158;                                   // branch always

  
block_157:
  // nop                                            // sll r0, r0, 0
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf14);     // vaddw.x vf23, vf23, vf14
  
block_158:
  c->vftoi4(DEST::xyw, vf4, vf4);                   // vftoi4.xyw vf4, vf4
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf4, vf4);                     // vftoi0.z vf4, vf4
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 176, a0);                            // sqc2 vf8, 176(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf4, 192, a0);                            // sqc2 vf4, 192(a0)
  // nop                                            // sll r0, r0, 0
  c->lw(t2, 80, a0);                                // lw t2, 80(a0)
  // nop                                            // sll r0, r0, 0
  c->lw(t1, 84, a0);                                // lw t1, 84(a0)
  c->ori(t3, r0, 36864);                            // ori t3, r0, 36864
  c->dsubu(t2, t2, t3);                             // dsubu t2, t2, t3
  c->lw(t3, 192, a0);                               // lw t3, 192(a0)
  c->ori(t4, r0, 36096);                            // ori t4, r0, 36096
  c->dsubu(t1, t1, t4);                             // dsubu t1, t1, t4
  c->lw(t4, 196, a0);                               // lw t4, 196(a0)
  bc = ((s64)c->sgpr64(t2)) > 0;                    // bgtz t2, L99
  c->daddiu(t2, t3, -28672);                        // daddiu t2, t3, -28672
  if (bc) {goto block_84;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t1)) > 0;                    // bgtz t1, L99
  c->daddiu(t1, t4, -29440);                        // daddiu t1, t4, -29440
  if (bc) {goto block_84;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t2)) < 0;                    // bltz t2, L99
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_84;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L99
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_84;}                          // branch non-likely

  //beq r0, r0, L99                                 // beq r0, r0, L99
  c->daddiu(a0, a0, 208);                           // daddiu a0, a0, 208
  goto block_84;                                    // branch always

  
block_163:
  c->lw(v1, 6172, v1);                              // lw v1, 6172(v1)
  c->sw(a0, 4, v1);                                 // sw a0, 4(v1)
  c->vsub(DEST::xyzw, vf23, vf23, vf24);            // vsub.xyzw vf23, vf23, vf24
  c->mov128_gpr_vf(v0, vf23);                       // qmfc2.i v0, vf23
  c->ld(ra, 0, sp);                                 // ld ra, 0(sp)
  c->lq(gp, 48, sp);                                // lq gp, 48(sp)
  c->lq(s5, 32, sp);                                // lq s5, 32(sp)
  c->lq(s4, 16, sp);                                // lq s4, 16(sp)
  //jr ra                                           // jr ra
  c->daddiu(sp, sp, 64);                            // daddiu sp, sp, 64
  goto end_of_function;                             // return

  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
end_of_function:
  return c->gprs[v0].du64[0];
}

void link() {
  cache.font_work = intern_from_c(-1, 0, "*font-work*").c();
  cache.font12_table = intern_from_c(-1, 0, "*font12-table*").c();
  cache.font24_table = intern_from_c(-1, 0, "*font24-table*").c();
  cache.draw_string_init_justify = intern_from_c(-1, 0, "draw-string-init-justify").c();
  gLinkedFunctionTable.reg("draw-string3d-asm", execute, 512);
}

} // namespace draw_string3d_asm
} // namespace Mips2C
// add draw_string3d_asm::link to the link callback table for the object file.
// FWD DEC:
namespace draw_string3d_asm { extern void link(); }

// clang-format off
using ::jakx::intern_from_c;
namespace Mips2C::jakx {
namespace draw_string_asm_packed {
struct Cache {
  void* font_work; // *font-work*
  void* font12_table; // *font12-table*
  void* font24_table; // *font24-table*
  void* draw_string_init_justify; // draw-string-init-justify
} cache;

u64 execute(void* ctxt) {
  auto* c = (ExecutionContext*)ctxt;
  printf("M2C: %s\n", __PRETTY_FUNCTION__);
  bool bc = false;
  u32 call_addr = 0;
  c->daddiu(sp, sp, -64);                           // daddiu sp, sp, -64
  c->sd(ra, 0, sp);                                 // sd ra, 0(sp)
  c->sq(s4, 16, sp);                                // sq s4, 16(sp)
  c->sq(s5, 32, sp);                                // sq s5, 32(sp)
  c->sq(gp, 48, sp);                                // sq gp, 48(sp)
  c->mov64(s5, a0);                                 // or s5, a0, r0
  c->mov64(s4, a1);                                 // or s4, a1, r0
  c->mov64(gp, a2);                                 // or gp, a2, r0
  c->load_symbol2(t9, cache.draw_string_init_justify);// lw t9, draw-string-init-justify(s7)
  c->mov64(a0, s5);                                 // or a0, s5, r0
  c->mov64(a1, s4);                                 // or a1, s4, r0
  c->mov64(a2, gp);                                 // or a2, gp, r0
  call_addr = c->gprs[t9].du32[0];                  // function call:
  c->sll(v0, ra, 0);                                // sll v0, ra, 0
  c->jalr(call_addr);                               // jalr ra, t9
  c->load_symbol2(v1, cache.font_work);             // lw v1, *font-work*(s7)
  c->mov64(v1, v1);                                 // or v1, v1, r0
  c->sw(s4, 6172, v1);                              // sw s4, 6172(v1)
  c->lw(a0, 4, s4);                                 // lw a0, 4(s4)
  c->sw(s5, 6176, v1);                              // sw s5, 6176(v1)
  c->lqc2(vf28, 76, gp);                            // lqc2 vf28, 76(gp)
  c->lqc2(vf29, 92, gp);                            // lqc2 vf29, 92(gp)
  c->lqc2(vf30, 108, gp);                           // lqc2 vf30, 108(gp)
  c->lqc2(vf31, 124, gp);                           // lqc2 vf31, 124(gp)
  c->lqc2(vf16, 416, v1);                           // lqc2 vf16, 416(v1)
  c->lqc2(vf17, 432, v1);                           // lqc2 vf17, 432(v1)
  c->lqc2(vf18, 448, v1);                           // lqc2 vf18, 448(v1)
  c->lqc2(vf27, 4736, v1);                          // lqc2 vf27, 4736(v1)
  c->lqc2(vf26, 4752, v1);                          // lqc2 vf26, 4752(v1)
  c->lqc2(vf25, 156, gp);                           // lqc2 vf25, 156(gp)
  c->lqc2(vf23, 44, gp);                            // lqc2 vf23, 44(gp)
  c->lqc2(vf24, 44, gp);                            // lqc2 vf24, 44(gp)
  c->lqc2(vf1, 44, gp);                             // lqc2 vf1, 44(gp)
  c->lqc2(vf4, 44, gp);                             // lqc2 vf4, 44(gp)
  c->vadd_bc(DEST::x, BC::x, vf1, vf0, vf0);        // vaddx.x vf1, vf0, vf0
  c->vadd_bc(DEST::x, BC::x, vf4, vf0, vf0);        // vaddx.x vf4, vf0, vf0
  c->vadd(DEST::x, vf1, vf0, vf25);                 // vadd.x vf1, vf0, vf25
  c->vmul_bc(DEST::x, BC::w, vf4, vf25, vf16);      // vmulw.x vf4, vf25, vf16
  c->sqc2(vf1, 464, v1);                            // sqc2 vf1, 464(v1)
  c->sqc2(vf4, 480, v1);                            // sqc2 vf4, 480(v1)
  c->lw(a1, 12, gp);                                // lw a1, 12(gp)
  c->vmove(DEST::xyzw, vf1, vf0);                   // vmove.xyzw vf1, vf0
  c->vmove(DEST::xyzw, vf4, vf0);                   // vmove.xyzw vf4, vf0
  // nop                                            // sll r0, r0, 0
  c->andi(a1, a1, 32);                              // andi a1, a1, 32
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(a1) != 0;                          // bne a1, r0, L128
  c->load_symbol2(a1, cache.font12_table);          // lw a1, *font12-table*(s7)
  if (bc) {goto block_2;}                           // branch non-likely

  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lq(a2, 192, v1);                               // lq a2, 192(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(a3, 208, v1);                               // lq a3, 208(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t0, 224, v1);                               // lq t0, 224(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 240, v1);                               // lq t1, 240(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a2, 6080, v1);                              // sq a2, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a3, 6096, v1);                              // sq a3, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t0, 6112, v1);                              // sq t0, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 6128, v1);                              // sq t1, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 320, v1);                           // lqc2 vf13, 320(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 336, v1);                           // lqc2 vf14, 336(v1)
  //beq r0, r0, L129                                // beq r0, r0, L129
  c->lqc2(vf15, 352, v1);                           // lqc2 vf15, 352(v1)
  goto block_3;                                     // branch always

  
block_2:
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a1, cache.font24_table);          // lw a1, *font24-table*(s7)
  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lq(a2, 256, v1);                               // lq a2, 256(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(a3, 272, v1);                               // lq a3, 272(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t0, 288, v1);                               // lq t0, 288(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 304, v1);                               // lq t1, 304(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a2, 6080, v1);                              // sq a2, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a3, 6096, v1);                              // sq a3, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t0, 6112, v1);                              // sq t0, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 6128, v1);                              // sq t1, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 368, v1);                           // lqc2 vf13, 368(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 384, v1);                           // lqc2 vf14, 384(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 400, v1);                           // lqc2 vf15, 400(v1)
  
block_3:
  // nop                                            // sll r0, r0, 0
  c->lb(a3, 180, gp);                               // lb a3, 180(gp)
  c->sll(t0, a3, 4);                                // sll t0, a3, 4
  c->lwu(a2, 8, gp);                                // lwu a2, 8(gp)
  c->daddu(t0, t0, v1);                             // daddu t0, t0, v1
  c->sb(a3, 6168, v1);                              // sb a3, 6168(v1)
  // nop                                            // sll r0, r0, 0
  c->lwu(a3, 4896, t0);                             // lwu a3, 4896(t0)
  c->pextlb(a3, r0, a3);                            // pextlb a3, r0, a3
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pextlh(a3, r0, a3);                            // pextlh a3, r0, a3
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  // nop                                            // sll r0, r0, 0
  c->sq(a3, 544, v1);                               // sq a3, 544(v1)
  // nop                                            // sll r0, r0, 0
  c->sw(a2, 556, v1);                               // sw a2, 556(v1)
  // nop                                            // sll r0, r0, 0
  c->sw(a2, 620, v1);                               // sw a2, 620(v1)
  c->lw(a2, 12, gp);                                // lw a2, 12(gp)
  c->mov64(a3, v1);                                 // or a3, v1, r0
  c->lw(t0, 6176, v1);                              // lw t0, 6176(v1)
  c->lqc2(vf23, 640, a3);                           // lqc2 vf23, 640(a3)
  
block_4:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->lqc2(vf20, 6080, v1);                          // lqc2 vf20, 6080(v1)
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L159
  c->daddiu(t2, t1, -3);                            // daddiu t2, t1, -3
  if (bc) {goto block_91;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t2)) <= 0;                   // blez t2, L144
  c->daddiu(t2, t1, -126);                          // daddiu t2, t1, -126
  if (bc) {goto block_57;}                          // branch non-likely

  bc = c->sgpr64(t2) != 0;                          // bne t2, r0, L148
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_63;}                          // branch non-likely

  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->addiu(t2, r0, 0);                              // addiu t2, r0, 0
  c->addiu(t3, r0, 0);                              // addiu t3, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L159
  c->daddiu(t4, t1, -43);                           // daddiu t4, t1, -43
  if (bc) {goto block_91;}                          // branch non-likely

  c->movz(t2, t1, t4);                              // movz t2, t1, t4
  c->daddiu(t4, t1, -45);                           // daddiu t4, t1, -45
  c->movz(t2, t1, t4);                              // movz t2, t1, t4
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t2) != 0;                          // bne t2, r0, L131
  c->daddiu(t4, t1, -91);                           // daddiu t4, t1, -91
  if (bc) {goto block_18;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L130
  c->daddiu(t3, t1, -93);                           // daddiu t3, t1, -93
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L130
  c->daddiu(t3, t1, -121);                          // daddiu t3, t1, -121
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L142
  c->daddiu(t3, t1, -89);                           // daddiu t3, t1, -89
  if (bc) {goto block_55;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L142
  c->daddiu(t3, t1, -122);                          // daddiu t3, t1, -122
  if (bc) {goto block_55;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L143
  c->daddiu(t3, t1, -90);                           // daddiu t3, t1, -90
  if (bc) {goto block_56;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L143
  c->daddiu(t3, t1, -48);                           // daddiu t3, t1, -48
  if (bc) {goto block_56;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t3)) < 0;                    // bltz t3, L148
  c->daddiu(t3, t1, -57);                           // daddiu t3, t1, -57
  if (bc) {goto block_63;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t3)) > 0;                    // bgtz t3, L148
  c->daddiu(t3, t1, -126);                          // daddiu t3, t1, -126
  if (bc) {goto block_63;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L148
  c->daddiu(t3, t1, -48);                           // daddiu t3, t1, -48
  if (bc) {goto block_63;}                          // branch non-likely

  
block_18:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L159
  c->daddiu(t4, t1, -110);                          // daddiu t4, t1, -110
  if (bc) {goto block_91;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L132
  c->daddiu(t4, t1, -78);                           // daddiu t4, t1, -78
  if (bc) {goto block_38;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L132
  c->daddiu(t4, t1, -108);                          // daddiu t4, t1, -108
  if (bc) {goto block_38;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L130
  c->daddiu(t4, t1, -76);                           // daddiu t4, t1, -76
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L130
  c->daddiu(t4, t1, -119);                          // daddiu t4, t1, -119
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L134
  c->daddiu(t4, t1, -87);                           // daddiu t4, t1, -87
  if (bc) {goto block_41;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L134
  c->daddiu(t4, t1, -107);                          // daddiu t4, t1, -107
  if (bc) {goto block_41;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L135
  c->daddiu(t4, t1, -75);                           // daddiu t4, t1, -75
  if (bc) {goto block_43;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L135
  c->daddiu(t4, t1, -106);                          // daddiu t4, t1, -106
  if (bc) {goto block_43;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L130
  c->daddiu(t4, t1, -74);                           // daddiu t4, t1, -74
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L130
  c->daddiu(t4, t1, -104);                          // daddiu t4, t1, -104
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L136
  c->daddiu(t4, t1, -72);                           // daddiu t4, t1, -72
  if (bc) {goto block_45;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L136
  c->daddiu(t4, t1, -118);                          // daddiu t4, t1, -118
  if (bc) {goto block_45;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L139
  c->daddiu(t4, t1, -86);                           // daddiu t4, t1, -86
  if (bc) {goto block_50;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L139
  c->daddiu(t4, t1, -117);                          // daddiu t4, t1, -117
  if (bc) {goto block_50;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L130
  c->daddiu(t4, t1, -85);                           // daddiu t4, t1, -85
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L130
  c->daddiu(t4, t1, -48);                           // daddiu t4, t1, -48
  if (bc) {goto block_4;}                           // branch non-likely

  bc = ((s64)c->sgpr64(t4)) < 0;                    // bltz t4, L148
  c->daddiu(t5, t1, -57);                           // daddiu t5, t1, -57
  if (bc) {goto block_63;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t5)) > 0;                    // bgtz t5, L148
  c->sll(t5, t3, 2);                                // sll t5, t3, 2
  if (bc) {goto block_63;}                          // branch non-likely

  c->daddu(t1, t3, t5);                             // daddu t1, t3, t5
  // nop                                            // sll r0, r0, 0
  c->sll(t1, t1, 1);                                // sll t1, t1, 1
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L131                                // beq r0, r0, L131
  c->daddu(t3, t1, t4);                             // daddu t3, t1, t4
  goto block_18;                                    // branch always

  
block_38:
  bc = c->sgpr64(t3) != 0;                          // bne t3, r0, L133
  c->addiu(t1, r0, -33);                            // addiu t1, r0, -33
  if (bc) {goto block_40;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lq(a1, 192, v1);                               // lq a1, 192(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 208, v1);                               // lq t2, 208(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t3, 224, v1);                               // lq t3, 224(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t4, 240, v1);                               // lq t4, 240(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a1, 6080, v1);                              // sq a1, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 6096, v1);                              // sq t2, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t3, 6112, v1);                              // sq t3, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t4, 6128, v1);                              // sq t4, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a1, cache.font12_table);          // lw a1, *font12-table*(s7)
  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 320, v1);                           // lqc2 vf13, 320(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 336, v1);                           // lqc2 vf14, 336(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 352, v1);                           // lqc2 vf15, 352(v1)
  //beq r0, r0, L130                                // beq r0, r0, L130
  c->and_(a2, a2, t1);                              // and a2, a2, t1
  goto block_4;                                     // branch always

  
block_40:
  // nop                                            // sll r0, r0, 0
  c->lq(a1, 256, v1);                               // lq a1, 256(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 272, v1);                               // lq t1, 272(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 288, v1);                               // lq t2, 288(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t3, 304, v1);                               // lq t3, 304(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a1, 6080, v1);                              // sq a1, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 6096, v1);                              // sq t1, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 6112, v1);                              // sq t2, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t3, 6128, v1);                              // sq t3, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a1, cache.font24_table);          // lw a1, *font24-table*(s7)
  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 368, v1);                           // lqc2 vf13, 368(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 384, v1);                           // lqc2 vf14, 384(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 400, v1);                           // lqc2 vf15, 400(v1)
  //beq r0, r0, L130                                // beq r0, r0, L130
  c->ori(a2, a2, 32);                               // ori a2, a2, 32
  goto block_4;                                     // branch always

  
block_41:
  c->addiu(t1, r0, -2);                             // addiu t1, r0, -2
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L130
  c->and_(a2, a2, t1);                              // and a2, a2, t1
  if (bc) {goto block_4;}                           // branch non-likely

  //beq r0, r0, L130                                // beq r0, r0, L130
  c->ori(a2, a2, 1);                                // ori a2, a2, 1
  goto block_4;                                     // branch always

  
block_43:
  c->addiu(t1, r0, -3);                             // addiu t1, r0, -3
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L130
  c->and_(a2, a2, t1);                              // and a2, a2, t1
  if (bc) {goto block_4;}                           // branch non-likely

  //beq r0, r0, L130                                // beq r0, r0, L130
  c->ori(a2, a2, 2);                                // ori a2, a2, 2
  goto block_4;                                     // branch always

  
block_45:
  c->mov128_vf_gpr(vf1, t3);                        // qmtc2.i vf1, t3
  c->daddiu(t1, t2, -45);                           // daddiu t1, t2, -45
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L138
  c->vitof0(DEST::xyzw, vf1, vf1);                  // vitof0.xyzw vf1, vf1
  if (bc) {goto block_49;}                          // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L137
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_48;}                          // branch non-likely

  //beq r0, r0, L130                                // beq r0, r0, L130
  c->vadd_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vaddx.x vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_48:
  //beq r0, r0, L130                                // beq r0, r0, L130
  c->vsub_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vsubx.x vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_49:
  //beq r0, r0, L130                                // beq r0, r0, L130
  c->vadd_bc(DEST::x, BC::x, vf23, vf0, vf1);       // vaddx.x vf23, vf0, vf1
  goto block_4;                                     // branch always

  
block_50:
  c->mov128_vf_gpr(vf1, t3);                        // qmtc2.i vf1, t3
  c->daddiu(t1, t2, -45);                           // daddiu t1, t2, -45
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L141
  c->vitof0(DEST::xyzw, vf1, vf1);                  // vitof0.xyzw vf1, vf1
  if (bc) {goto block_54;}                          // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L140
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_53;}                          // branch non-likely

  //beq r0, r0, L130                                // beq r0, r0, L130
  c->vadd_bc(DEST::y, BC::x, vf23, vf23, vf1);      // vaddx.y vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_53:
  //beq r0, r0, L130                                // beq r0, r0, L130
  c->vsub_bc(DEST::y, BC::x, vf23, vf23, vf1);      // vsubx.y vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_54:
  //beq r0, r0, L130                                // beq r0, r0, L130
  c->vadd_bc(DEST::y, BC::x, vf23, vf0, vf1);       // vaddx.y vf23, vf0, vf1
  goto block_4;                                     // branch always

  
block_55:
  //beq r0, r0, L130                                // beq r0, r0, L130
  c->sqc2(vf23, 496, v1);                           // sqc2 vf23, 496(v1)
  goto block_4;                                     // branch always

  
block_56:
  //beq r0, r0, L130                                // beq r0, r0, L130
  c->lqc2(vf23, 496, v1);                           // lqc2 vf23, 496(v1)
  goto block_4;                                     // branch always

  
block_57:
  c->daddiu(t2, t1, -3);                            // daddiu t2, t1, -3
  c->ori(a2, a2, 64);                               // ori a2, a2, 64
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L146
  c->daddiu(t1, t1, -2);                            // daddiu t1, t1, -2
  if (bc) {goto block_61;}                          // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L145
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_60;}                          // branch non-likely

  //beq r0, r0, L147                                // beq r0, r0, L147
  c->lqc2(vf20, 6096, v1);                          // lqc2 vf20, 6096(v1)
  goto block_62;                                    // branch always

  
block_60:
  //beq r0, r0, L147                                // beq r0, r0, L147
  c->lqc2(vf20, 6112, v1);                          // lqc2 vf20, 6112(v1)
  goto block_62;                                    // branch always

  
block_61:
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf20, 6128, v1);                          // lqc2 vf20, 6128(v1)
  
block_62:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->vadd(DEST::xyz, vf4, vf23, vf15);              // vadd.xyz vf4, vf23, vf15
  c->sll(t2, t1, 4);                                // sll t2, t1, 4
  //beq r0, r0, L150                                // beq r0, r0, L150
  c->vsub(DEST::xyzw, vf1, vf25, vf23);             // vsub.xyzw vf1, vf25, vf23
  goto block_66;                                    // branch always

  
block_63:
  c->sll(t2, t1, 4);                                // sll t2, t1, 4
  c->addiu(t3, r0, -65);                            // addiu t3, r0, -65
  c->vadd(DEST::xyz, vf4, vf23, vf15);              // vadd.xyz vf4, vf23, vf15
  c->and_(a2, a2, t3);                              // and a2, a2, t3
  c->vsub(DEST::xyzw, vf1, vf25, vf23);             // vsub.xyzw vf1, vf25, vf23
  c->daddiu(t3, t1, -10);                           // daddiu t3, t1, -10
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L149
  c->daddiu(t1, t1, -13);                           // daddiu t1, t1, -13
  if (bc) {goto block_65;}                          // branch non-likely

  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L150
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_66;}                          // branch non-likely

  
block_65:
  // nop                                            // sll r0, r0, 0
  c->daddiu(a3, a3, 16);                            // daddiu a3, a3, 16
  //beq r0, r0, L130                                // beq r0, r0, L130
  c->lqc2(vf23, 640, a3);                           // lqc2 vf23, 640(a3)
  goto block_4;                                     // branch always

  
block_66:
  // nop                                            // sll r0, r0, 0
  c->addu(t1, t2, a1);                              // addu t1, t2, a1
  c->lqc2(vf5, -96, t1);                            // lqc2 vf5, -96(t1)
  c->mov128_gpr_vf(t1, vf1);                        // qmfc2.i t1, vf1
  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L159
  c->vadd(DEST::xyz, vf8, vf5, vf18);               // vadd.xyz vf8, vf5, vf18
  if (bc) {goto block_91;}                          // branch non-likely

  c->sra(t1, t1, 31);                               // sra t1, t1, 31
  // nop                                            // sll r0, r0, 0
  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L130
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_4;}                           // branch non-likely

  c->vadd(DEST::xyz, vf1, vf23, vf0);               // vadd.xyz vf1, vf23, vf0
  c->andi(t1, a2, 4097);                            // andi t1, a2, 4097
  c->vmul(DEST::xyzw, vf19, vf5, vf13);             // vmul.xyzw vf19, vf5, vf13
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L130
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_4;}                           // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf1);       // vmaddax.xyzw acc, vf28, vf1
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf1);       // vmadday.xyzw acc, vf29, vf1
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf1, vf30, vf1);   // vmaddz.xyzw vf1, vf30, vf1
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf4);       // vmaddax.xyzw acc, vf28, vf4
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf4);       // vmadday.xyzw acc, vf29, vf4
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf4, vf30, vf4);   // vmaddz.xyzw vf4, vf30, vf4
  c->vdiv(vf25, BC::z, vf1, BC::w);                 // vdiv Q, vf25.z, vf1.w
  c->lq(t2, 0, v1);                                 // lq t2, 0(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 16, v1);                                // lq t1, 16(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 608, v1);                            // lqc2 vf9, 608(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 0, a0);                                 // sq t2, 0(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 16, a0);                                // sq t1, 16(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  c->vmulq(DEST::xyz, vf1, vf1);                    // vmulq.xyz vf1, vf1, Q
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf5, vf5);                    // vmulq.xyz vf5, vf5, Q
  // nop                                            // sll r0, r0, 0
  c->vdiv(vf25, BC::z, vf4, BC::w);                 // vdiv Q, vf25.z, vf4.w
  c->andi(t2, a2, 2);                               // andi t2, a2, 2
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf2, vf1, vf26);              // vadd.xyzw vf2, vf1, vf26
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L151
  c->andi(t2, a2, 64);                              // andi t2, a2, 64
  if (bc) {goto block_72;}                          // branch non-likely

  bc = c->sgpr64(t2) != 0;                          // bne t2, r0, L151
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_72;}                          // branch non-likely

  //beq r0, r0, L152                                // beq r0, r0, L152
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf19);     // vaddw.x vf23, vf23, vf19
  goto block_73;                                    // branch always

  
block_72:
  // nop                                            // sll r0, r0, 0
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf14);     // vaddw.x vf23, vf23, vf14
  
block_73:
  c->vftoi4(DEST::xyzw, vf2, vf2);                  // vftoi4.xyzw vf2, vf2
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  c->vmulq(DEST::xyz, vf4, vf4);                    // vmulq.xyz vf4, vf4, Q
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf8, vf8);                    // vmulq.xyz vf8, vf8, Q
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf3, vf4, vf26);              // vadd.xyzw vf3, vf4, vf26
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 96, a0);                             // sqc2 vf8, 96(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyzw, vf3, vf3);                  // vftoi4.xyzw vf3, vf3
  c->andi(t2, a2, 256);                             // andi t2, a2, 256
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L153
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_75;}                          // branch non-likely

  c->vftoi0(DEST::z, vf2, vf23);                    // vftoi0.z vf2, vf23
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf3, vf23);                    // vftoi0.z vf3, vf23
  // nop                                            // sll r0, r0, 0
  
block_75:
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 80, a0);                             // sqc2 vf2, 80(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf3, 112, a0);                            // sqc2 vf3, 112(a0)
  // nop                                            // sll r0, r0, 0
  c->lw(t3, 80, a0);                                // lw t3, 80(a0)
  // nop                                            // sll r0, r0, 0
  c->lw(t2, 84, a0);                                // lw t2, 84(a0)
  c->ori(t4, r0, 36864);                            // ori t4, r0, 36864
  c->dsubu(t3, t3, t4);                             // dsubu t3, t3, t4
  c->lw(t4, 112, a0);                               // lw t4, 112(a0)
  c->ori(t5, r0, 36096);                            // ori t5, r0, 36096
  c->dsubu(t2, t2, t5);                             // dsubu t2, t2, t5
  c->lw(t5, 116, a0);                               // lw t5, 116(a0)
  bc = ((s64)c->sgpr64(t3)) > 0;                    // bgtz t3, L130
  c->daddiu(t3, t4, -28672);                        // daddiu t3, t4, -28672
  if (bc) {goto block_4;}                           // branch non-likely

  bc = ((s64)c->sgpr64(t2)) > 0;                    // bgtz t2, L130
  c->daddiu(t2, t5, -29440);                        // daddiu t2, t5, -29440
  if (bc) {goto block_4;}                           // branch non-likely

  bc = ((s64)c->sgpr64(t3)) < 0;                    // bltz t3, L130
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_4;}                           // branch non-likely

  bc = ((s64)c->sgpr64(t2)) < 0;                    // bltz t2, L130
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_4;}                           // branch non-likely

  c->andi(t2, a2, 1);                               // andi t2, a2, 1
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L154
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_81;}                          // branch non-likely

  c->daddiu(a0, a0, 128);                           // daddiu a0, a0, 128
  // nop                                            // sll r0, r0, 0
  
block_81:
  c->andi(t2, a2, 4096);                            // andi t2, a2, 4096
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L130
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_4;}                           // branch non-likely

  c->lqc2(vf26, 4784, v1);                          // lqc2 vf26, 4784(v1)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 0, v1);                                 // lq t2, 0(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 16, a0);                                // sq t1, 16(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 0, a0);                                 // sq t2, 0(a0)
  c->vadd(DEST::xyzw, vf2, vf1, vf26);              // vadd.xyzw vf2, vf1, vf26
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyzw, vf2, vf2);                  // vftoi4.xyzw vf2, vf2
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  c->vadd(DEST::xyzw, vf3, vf4, vf26);              // vadd.xyzw vf3, vf4, vf26
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 96, a0);                             // sqc2 vf8, 96(a0)
  c->vftoi4(DEST::xyzw, vf3, vf3);                  // vftoi4.xyzw vf3, vf3
  c->andi(t2, a2, 256);                             // andi t2, a2, 256
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L155
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_84;}                          // branch non-likely

  c->vftoi0(DEST::z, vf2, vf23);                    // vftoi0.z vf2, vf23
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf3, vf23);                    // vftoi0.z vf3, vf23
  // nop                                            // sll r0, r0, 0
  
block_84:
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 80, a0);                             // sqc2 vf2, 80(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf3, 112, a0);                            // sqc2 vf3, 112(a0)
  c->daddiu(a0, a0, 128);                           // daddiu a0, a0, 128
  c->lqc2(vf26, 4800, v1);                          // lqc2 vf26, 4800(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 0, v1);                                 // lq t2, 0(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 16, a0);                                // sq t1, 16(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 0, a0);                                 // sq t2, 0(a0)
  c->vadd(DEST::xyzw, vf2, vf1, vf26);              // vadd.xyzw vf2, vf1, vf26
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyzw, vf2, vf2);                  // vftoi4.xyzw vf2, vf2
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  c->vadd(DEST::xyzw, vf3, vf4, vf26);              // vadd.xyzw vf3, vf4, vf26
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 96, a0);                             // sqc2 vf8, 96(a0)
  c->vftoi4(DEST::xyzw, vf3, vf3);                  // vftoi4.xyzw vf3, vf3
  c->andi(t2, a2, 256);                             // andi t2, a2, 256
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L156
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_86;}                          // branch non-likely

  c->vftoi0(DEST::z, vf2, vf23);                    // vftoi0.z vf2, vf23
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf3, vf23);                    // vftoi0.z vf3, vf23
  // nop                                            // sll r0, r0, 0
  
block_86:
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 80, a0);                             // sqc2 vf2, 80(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf3, 112, a0);                            // sqc2 vf3, 112(a0)
  c->daddiu(a0, a0, 128);                           // daddiu a0, a0, 128
  c->lqc2(vf26, 4816, v1);                          // lqc2 vf26, 4816(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 0, v1);                                 // lq t2, 0(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 16, a0);                                // sq t1, 16(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 0, a0);                                 // sq t2, 0(a0)
  c->vadd(DEST::xyzw, vf2, vf1, vf26);              // vadd.xyzw vf2, vf1, vf26
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyzw, vf2, vf2);                  // vftoi4.xyzw vf2, vf2
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  c->vadd(DEST::xyzw, vf3, vf4, vf26);              // vadd.xyzw vf3, vf4, vf26
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 96, a0);                             // sqc2 vf8, 96(a0)
  c->vftoi4(DEST::xyzw, vf3, vf3);                  // vftoi4.xyzw vf3, vf3
  c->andi(t2, a2, 256);                             // andi t2, a2, 256
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L157
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_88;}                          // branch non-likely

  c->vftoi0(DEST::z, vf2, vf23);                    // vftoi0.z vf2, vf23
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf3, vf23);                    // vftoi0.z vf3, vf23
  // nop                                            // sll r0, r0, 0
  
block_88:
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 80, a0);                             // sqc2 vf2, 80(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf3, 112, a0);                            // sqc2 vf3, 112(a0)
  c->daddiu(a0, a0, 128);                           // daddiu a0, a0, 128
  c->lqc2(vf26, 4768, v1);                          // lqc2 vf26, 4768(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 0, v1);                                 // lq t2, 0(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 16, a0);                                // sq t1, 16(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 0, a0);                                 // sq t2, 0(a0)
  c->vadd(DEST::xyzw, vf2, vf1, vf26);              // vadd.xyzw vf2, vf1, vf26
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyzw, vf2, vf2);                  // vftoi4.xyzw vf2, vf2
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  c->vadd(DEST::xyzw, vf3, vf4, vf26);              // vadd.xyzw vf3, vf4, vf26
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 96, a0);                             // sqc2 vf8, 96(a0)
  c->vftoi4(DEST::xyzw, vf3, vf3);                  // vftoi4.xyzw vf3, vf3
  c->andi(t1, a2, 256);                             // andi t1, a2, 256
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L158
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_90;}                          // branch non-likely

  c->vftoi0(DEST::z, vf2, vf23);                    // vftoi0.z vf2, vf23
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf3, vf23);                    // vftoi0.z vf3, vf23
  // nop                                            // sll r0, r0, 0
  
block_90:
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 80, a0);                             // sqc2 vf2, 80(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf3, 112, a0);                            // sqc2 vf3, 112(a0)
  c->lqc2(vf26, 4752, v1);                          // lqc2 vf26, 4752(v1)
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L130                                // beq r0, r0, L130
  c->daddiu(a0, a0, 128);                           // daddiu a0, a0, 128
  goto block_4;                                     // branch always

  
block_91:
  c->lw(a2, 12, gp);                                // lw a2, 12(gp)
  c->mov64(a3, v1);                                 // or a3, v1, r0
  c->lw(t0, 6176, v1);                              // lw t0, 6176(v1)
  c->lqc2(vf23, 640, a3);                           // lqc2 vf23, 640(a3)
  
block_92:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->lqc2(vf20, 6080, v1);                          // lqc2 vf20, 6080(v1)
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L187
  c->daddiu(t2, t1, -3);                            // daddiu t2, t1, -3
  if (bc) {goto block_171;}                         // branch non-likely

  bc = ((s64)c->sgpr64(t2)) <= 0;                   // blez t2, L177
  c->daddiu(t2, t1, -126);                          // daddiu t2, t1, -126
  if (bc) {goto block_149;}                         // branch non-likely

  bc = c->sgpr64(t2) != 0;                          // bne t2, r0, L181
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_155;}                         // branch non-likely

  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->addiu(t2, r0, 0);                              // addiu t2, r0, 0
  c->addiu(t3, r0, 0);                              // addiu t3, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L187
  c->daddiu(t4, t1, -43);                           // daddiu t4, t1, -43
  if (bc) {goto block_171;}                         // branch non-likely

  c->movz(t2, t1, t4);                              // movz t2, t1, t4
  c->daddiu(t4, t1, -45);                           // daddiu t4, t1, -45
  c->movz(t2, t1, t4);                              // movz t2, t1, t4
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t2) != 0;                          // bne t2, r0, L161
  c->daddiu(t4, t1, -91);                           // daddiu t4, t1, -91
  if (bc) {goto block_106;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L173
  c->daddiu(t3, t1, -93);                           // daddiu t3, t1, -93
  if (bc) {goto block_145;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L174
  c->daddiu(t3, t1, -121);                          // daddiu t3, t1, -121
  if (bc) {goto block_146;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L175
  c->daddiu(t3, t1, -89);                           // daddiu t3, t1, -89
  if (bc) {goto block_147;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L175
  c->daddiu(t3, t1, -122);                          // daddiu t3, t1, -122
  if (bc) {goto block_147;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L176
  c->daddiu(t3, t1, -90);                           // daddiu t3, t1, -90
  if (bc) {goto block_148;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L176
  c->daddiu(t3, t1, -48);                           // daddiu t3, t1, -48
  if (bc) {goto block_148;}                         // branch non-likely

  bc = ((s64)c->sgpr64(t3)) < 0;                    // bltz t3, L181
  c->daddiu(t3, t1, -57);                           // daddiu t3, t1, -57
  if (bc) {goto block_155;}                         // branch non-likely

  bc = ((s64)c->sgpr64(t3)) > 0;                    // bgtz t3, L181
  c->daddiu(t3, t1, -126);                          // daddiu t3, t1, -126
  if (bc) {goto block_155;}                         // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L181
  c->daddiu(t3, t1, -48);                           // daddiu t3, t1, -48
  if (bc) {goto block_155;}                         // branch non-likely

  
block_106:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L187
  c->daddiu(t4, t1, -110);                          // daddiu t4, t1, -110
  if (bc) {goto block_171;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L162
  c->daddiu(t4, t1, -78);                           // daddiu t4, t1, -78
  if (bc) {goto block_126;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L162
  c->daddiu(t4, t1, -108);                          // daddiu t4, t1, -108
  if (bc) {goto block_126;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L164
  c->daddiu(t4, t1, -76);                           // daddiu t4, t1, -76
  if (bc) {goto block_129;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L164
  c->daddiu(t4, t1, -119);                          // daddiu t4, t1, -119
  if (bc) {goto block_129;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L160
  c->daddiu(t4, t1, -87);                           // daddiu t4, t1, -87
  if (bc) {goto block_92;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L160
  c->daddiu(t4, t1, -107);                          // daddiu t4, t1, -107
  if (bc) {goto block_92;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L166
  c->daddiu(t4, t1, -75);                           // daddiu t4, t1, -75
  if (bc) {goto block_133;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L166
  c->daddiu(t4, t1, -106);                          // daddiu t4, t1, -106
  if (bc) {goto block_133;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L160
  c->daddiu(t4, t1, -74);                           // daddiu t4, t1, -74
  if (bc) {goto block_92;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L160
  c->daddiu(t4, t1, -104);                          // daddiu t4, t1, -104
  if (bc) {goto block_92;}                          // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L167
  c->daddiu(t4, t1, -72);                           // daddiu t4, t1, -72
  if (bc) {goto block_135;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L167
  c->daddiu(t4, t1, -118);                          // daddiu t4, t1, -118
  if (bc) {goto block_135;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L170
  c->daddiu(t4, t1, -86);                           // daddiu t4, t1, -86
  if (bc) {goto block_140;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L170
  c->daddiu(t4, t1, -117);                          // daddiu t4, t1, -117
  if (bc) {goto block_140;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L165
  c->daddiu(t4, t1, -85);                           // daddiu t4, t1, -85
  if (bc) {goto block_131;}                         // branch non-likely

  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L165
  c->daddiu(t4, t1, -48);                           // daddiu t4, t1, -48
  if (bc) {goto block_131;}                         // branch non-likely

  bc = ((s64)c->sgpr64(t4)) < 0;                    // bltz t4, L181
  c->daddiu(t5, t1, -57);                           // daddiu t5, t1, -57
  if (bc) {goto block_155;}                         // branch non-likely

  bc = ((s64)c->sgpr64(t5)) > 0;                    // bgtz t5, L181
  c->sll(t5, t3, 2);                                // sll t5, t3, 2
  if (bc) {goto block_155;}                         // branch non-likely

  c->daddu(t1, t3, t5);                             // daddu t1, t3, t5
  // nop                                            // sll r0, r0, 0
  c->sll(t1, t1, 1);                                // sll t1, t1, 1
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L161                                // beq r0, r0, L161
  c->daddu(t3, t1, t4);                             // daddu t3, t1, t4
  goto block_106;                                   // branch always

  
block_126:
  bc = c->sgpr64(t3) != 0;                          // bne t3, r0, L163
  c->addiu(t1, r0, -33);                            // addiu t1, r0, -33
  if (bc) {goto block_128;}                         // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lq(a1, 192, v1);                               // lq a1, 192(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 208, v1);                               // lq t2, 208(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t3, 224, v1);                               // lq t3, 224(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t4, 240, v1);                               // lq t4, 240(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a1, 6080, v1);                              // sq a1, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 6096, v1);                              // sq t2, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t3, 6112, v1);                              // sq t3, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t4, 6128, v1);                              // sq t4, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a1, cache.font12_table);          // lw a1, *font12-table*(s7)
  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 320, v1);                           // lqc2 vf13, 320(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 336, v1);                           // lqc2 vf14, 336(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 352, v1);                           // lqc2 vf15, 352(v1)
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->and_(a2, a2, t1);                              // and a2, a2, t1
  goto block_92;                                    // branch always

  
block_128:
  // nop                                            // sll r0, r0, 0
  c->lq(a1, 256, v1);                               // lq a1, 256(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 272, v1);                               // lq t1, 272(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 288, v1);                               // lq t2, 288(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t3, 304, v1);                               // lq t3, 304(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a1, 6080, v1);                              // sq a1, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 6096, v1);                              // sq t1, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 6112, v1);                              // sq t2, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t3, 6128, v1);                              // sq t3, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a1, cache.font24_table);          // lw a1, *font24-table*(s7)
  c->mov64(a1, a1);                                 // or a1, a1, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 368, v1);                           // lqc2 vf13, 368(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 384, v1);                           // lqc2 vf14, 384(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 400, v1);                           // lqc2 vf15, 400(v1)
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->ori(a2, a2, 32);                               // ori a2, a2, 32
  goto block_92;                                    // branch always

  
block_129:
  c->andi(t1, a2, 128);                             // andi t1, a2, 128
  c->sll(t2, t3, 4);                                // sll t2, t3, 4
  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L160
  c->sb(t3, 6168, v1);                              // sb t3, 6168(v1)
  if (bc) {goto block_92;}                          // branch non-likely

  c->daddu(t1, t2, v1);                             // daddu t1, t2, v1
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->lwu(t1, 4896, t1);                             // lwu t1, 4896(t1)
  c->pextlb(t2, r0, t1);                            // pextlb t2, r0, t1
  c->lwu(t1, 8, gp);                                // lwu t1, 8(gp)
  c->pextlh(t2, r0, t2);                            // pextlh t2, r0, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 544, v1);                               // sq t2, 544(v1)
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->sw(t1, 556, v1);                               // sw t1, 556(v1)
  goto block_92;                                    // branch always

  
block_131:
  c->andi(t1, a2, 128);                             // andi t1, a2, 128
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L160
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_92;}                          // branch non-likely

  c->pextlb(t2, r0, t3);                            // pextlb t2, r0, t3
  c->lw(t1, 8, gp);                                 // lw t1, 8(gp)
  c->pextlh(t2, r0, t2);                            // pextlh t2, r0, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 544, v1);                               // sq t2, 544(v1)
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->sw(t1, 556, v1);                               // sw t1, 556(v1)
  goto block_92;                                    // branch always

  
block_133:
  c->addiu(t1, r0, -3);                             // addiu t1, r0, -3
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L160
  c->and_(a2, a2, t1);                              // and a2, a2, t1
  if (bc) {goto block_92;}                          // branch non-likely

  //beq r0, r0, L160                                // beq r0, r0, L160
  c->ori(a2, a2, 2);                                // ori a2, a2, 2
  goto block_92;                                    // branch always

  
block_135:
  c->mov128_vf_gpr(vf1, t3);                        // qmtc2.i vf1, t3
  c->daddiu(t1, t2, -45);                           // daddiu t1, t2, -45
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L169
  c->vitof0(DEST::xyzw, vf1, vf1);                  // vitof0.xyzw vf1, vf1
  if (bc) {goto block_139;}                         // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L168
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_138;}                         // branch non-likely

  //beq r0, r0, L160                                // beq r0, r0, L160
  c->vadd_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vaddx.x vf23, vf23, vf1
  goto block_92;                                    // branch always

  
block_138:
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->vsub_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vsubx.x vf23, vf23, vf1
  goto block_92;                                    // branch always

  
block_139:
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->vadd_bc(DEST::x, BC::x, vf23, vf0, vf1);       // vaddx.x vf23, vf0, vf1
  goto block_92;                                    // branch always

  
block_140:
  c->mov128_vf_gpr(vf1, t3);                        // qmtc2.i vf1, t3
  c->daddiu(t1, t2, -45);                           // daddiu t1, t2, -45
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L172
  c->vitof0(DEST::xyzw, vf1, vf1);                  // vitof0.xyzw vf1, vf1
  if (bc) {goto block_144;}                         // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L171
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_143;}                         // branch non-likely

  //beq r0, r0, L160                                // beq r0, r0, L160
  c->vadd_bc(DEST::y, BC::x, vf23, vf23, vf1);      // vaddx.y vf23, vf23, vf1
  goto block_92;                                    // branch always

  
block_143:
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->vsub_bc(DEST::y, BC::x, vf23, vf23, vf1);      // vsubx.y vf23, vf23, vf1
  goto block_92;                                    // branch always

  
block_144:
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->vadd_bc(DEST::y, BC::x, vf23, vf0, vf1);       // vaddx.y vf23, vf0, vf1
  goto block_92;                                    // branch always

  
block_145:
  // nop                                            // sll r0, r0, 0
  c->lb(t1, 6168, v1);                              // lb t1, 6168(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 544, v1);                            // lqc2 vf9, 544(v1)
  // nop                                            // sll r0, r0, 0
  c->sb(t1, 6169, v1);                              // sb t1, 6169(v1)
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->sqc2(vf9, 512, v1);                            // sqc2 vf9, 512(v1)
  goto block_92;                                    // branch always

  
block_146:
  // nop                                            // sll r0, r0, 0
  c->lb(t1, 6169, v1);                              // lb t1, 6169(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 512, v1);                            // lqc2 vf9, 512(v1)
  // nop                                            // sll r0, r0, 0
  c->sb(t1, 6168, v1);                              // sb t1, 6168(v1)
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->sqc2(vf9, 544, v1);                            // sqc2 vf9, 544(v1)
  goto block_92;                                    // branch always

  
block_147:
  // nop                                            // sll r0, r0, 0
  c->lb(t1, 6168, v1);                              // lb t1, 6168(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 544, v1);                            // lqc2 vf9, 544(v1)
  // nop                                            // sll r0, r0, 0
  c->sb(t1, 6169, v1);                              // sb t1, 6169(v1)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 512, v1);                            // sqc2 vf9, 512(v1)
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->sqc2(vf23, 496, v1);                           // sqc2 vf23, 496(v1)
  goto block_92;                                    // branch always

  
block_148:
  // nop                                            // sll r0, r0, 0
  c->lb(t1, 6169, v1);                              // lb t1, 6169(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 512, v1);                            // lqc2 vf9, 512(v1)
  // nop                                            // sll r0, r0, 0
  c->sb(t1, 6168, v1);                              // sb t1, 6168(v1)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 544, v1);                            // sqc2 vf9, 544(v1)
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->lqc2(vf23, 496, v1);                           // lqc2 vf23, 496(v1)
  goto block_92;                                    // branch always

  
block_149:
  c->daddiu(t2, t1, -3);                            // daddiu t2, t1, -3
  c->ori(a2, a2, 64);                               // ori a2, a2, 64
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L179
  c->daddiu(t1, t1, -2);                            // daddiu t1, t1, -2
  if (bc) {goto block_153;}                         // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L178
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_152;}                         // branch non-likely

  //beq r0, r0, L180                                // beq r0, r0, L180
  c->lqc2(vf20, 6096, v1);                          // lqc2 vf20, 6096(v1)
  goto block_154;                                   // branch always

  
block_152:
  //beq r0, r0, L180                                // beq r0, r0, L180
  c->lqc2(vf20, 6112, v1);                          // lqc2 vf20, 6112(v1)
  goto block_154;                                   // branch always

  
block_153:
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf20, 6128, v1);                          // lqc2 vf20, 6128(v1)
  
block_154:
  c->lbu(t1, 4, t0);                                // lbu t1, 4(t0)
  c->daddiu(t0, t0, 1);                             // daddiu t0, t0, 1
  c->vadd(DEST::xyz, vf4, vf23, vf15);              // vadd.xyz vf4, vf23, vf15
  c->sll(t2, t1, 4);                                // sll t2, t1, 4
  //beq r0, r0, L183                                // beq r0, r0, L183
  c->vsub(DEST::xyzw, vf1, vf25, vf23);             // vsub.xyzw vf1, vf25, vf23
  goto block_158;                                   // branch always

  
block_155:
  // nop                                            // sll r0, r0, 0
  c->addiu(t2, r0, -65);                            // addiu t2, r0, -65
  c->and_(a2, a2, t2);                              // and a2, a2, t2
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyz, vf4, vf23, vf15);              // vadd.xyz vf4, vf23, vf15
  c->sll(t2, t1, 4);                                // sll t2, t1, 4
  c->vsub(DEST::xyzw, vf1, vf25, vf23);             // vsub.xyzw vf1, vf25, vf23
  c->daddiu(t3, t1, -10);                           // daddiu t3, t1, -10
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L182
  c->daddiu(t1, t1, -13);                           // daddiu t1, t1, -13
  if (bc) {goto block_157;}                         // branch non-likely

  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L183
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_158;}                         // branch non-likely

  
block_157:
  c->daddiu(a3, a3, 16);                            // daddiu a3, a3, 16
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L160                                // beq r0, r0, L160
  c->lqc2(vf23, 640, a3);                           // lqc2 vf23, 640(a3)
  goto block_92;                                    // branch always

  
block_158:
  c->addu(t1, t2, a1);                              // addu t1, t2, a1
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf5, -96, t1);                            // lqc2 vf5, -96(t1)
  c->mov128_gpr_vf(t1, vf1);                        // qmfc2.i t1, vf1
  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L187
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_171;}                         // branch non-likely

  c->sra(t1, t1, 31);                               // sra t1, t1, 31
  // nop                                            // sll r0, r0, 0
  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L160
  c->vadd(DEST::xyz, vf8, vf5, vf18);               // vadd.xyz vf8, vf5, vf18
  if (bc) {goto block_92;}                          // branch non-likely

  c->vadd(DEST::xyz, vf1, vf23, vf0);               // vadd.xyz vf1, vf23, vf0
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyz, vf4, vf23, vf15);              // vadd.xyz vf4, vf23, vf15
  // nop                                            // sll r0, r0, 0
  c->vmul(DEST::xyzw, vf19, vf5, vf13);             // vmul.xyzw vf19, vf5, vf13
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf1);       // vmaddax.xyzw acc, vf28, vf1
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf1);       // vmadday.xyzw acc, vf29, vf1
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf1, vf30, vf1);   // vmaddz.xyzw vf1, vf30, vf1
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf31, vf0);        // vmulaw.xyzw acc, vf31, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf28, vf4);       // vmaddax.xyzw acc, vf28, vf4
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf29, vf4);       // vmadday.xyzw acc, vf29, vf4
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf4, vf30, vf4);   // vmaddz.xyzw vf4, vf30, vf4
  c->vdiv(vf25, BC::z, vf1, BC::w);                 // vdiv Q, vf25.z, vf1.w
  c->lq(t1, 0, v1);                                 // lq t1, 0(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 16, v1);                                // lq t2, 16(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf9, 544, v1);                            // lqc2 vf9, 544(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 0, a0);                                 // sq t1, 0(a0)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 16, a0);                                // sq t2, 16(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf20, 32, a0);                            // sqc2 vf20, 32(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 48, a0);                             // sqc2 vf9, 48(a0)
  c->vmulq(DEST::xyz, vf1, vf1);                    // vmulq.xyz vf1, vf1, Q
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf5, vf5);                    // vmulq.xyz vf5, vf5, Q
  // nop                                            // sll r0, r0, 0
  c->vdiv(vf25, BC::z, vf4, BC::w);                 // vdiv Q, vf25.z, vf4.w
  c->andi(t1, a2, 2);                               // andi t1, a2, 2
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf1, vf1, vf27);              // vadd.xyzw vf1, vf1, vf27
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L184
  c->andi(t1, a2, 64);                              // andi t1, a2, 64
  if (bc) {goto block_163;}                         // branch non-likely

  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L184
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_163;}                         // branch non-likely

  //beq r0, r0, L185                                // beq r0, r0, L185
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf19);     // vaddw.x vf23, vf23, vf19
  goto block_164;                                   // branch always

  
block_163:
  // nop                                            // sll r0, r0, 0
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf14);     // vaddw.x vf23, vf23, vf14
  
block_164:
  c->vftoi4(DEST::xyzw, vf1, vf1);                  // vftoi4.xyzw vf1, vf1
  c->sqc2(vf5, 64, a0);                             // sqc2 vf5, 64(a0)
  c->vmulq(DEST::xyz, vf4, vf4);                    // vmulq.xyz vf4, vf4, Q
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xyz, vf8, vf8);                    // vmulq.xyz vf8, vf8, Q
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vadd(DEST::xyzw, vf4, vf4, vf27);              // vadd.xyzw vf4, vf4, vf27
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf8, 96, a0);                             // sqc2 vf8, 96(a0)
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->vftoi4(DEST::xyzw, vf4, vf4);                  // vftoi4.xyzw vf4, vf4
  c->andi(t1, a2, 256);                             // andi t1, a2, 256
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L186
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_166;}                         // branch non-likely

  c->vftoi0(DEST::z, vf1, vf23);                    // vftoi0.z vf1, vf23
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::z, vf4, vf23);                    // vftoi0.z vf4, vf23
  // nop                                            // sll r0, r0, 0
  
block_166:
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf1, 80, a0);                             // sqc2 vf1, 80(a0)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf4, 112, a0);                            // sqc2 vf4, 112(a0)
  // nop                                            // sll r0, r0, 0
  c->lw(t2, 80, a0);                                // lw t2, 80(a0)
  // nop                                            // sll r0, r0, 0
  c->lw(t1, 84, a0);                                // lw t1, 84(a0)
  c->ori(t3, r0, 36864);                            // ori t3, r0, 36864
  c->dsubu(t2, t2, t3);                             // dsubu t2, t2, t3
  c->lw(t3, 112, a0);                               // lw t3, 112(a0)
  c->ori(t4, r0, 36096);                            // ori t4, r0, 36096
  c->dsubu(t1, t1, t4);                             // dsubu t1, t1, t4
  c->lw(t4, 116, a0);                               // lw t4, 116(a0)
  bc = ((s64)c->sgpr64(t2)) > 0;                    // bgtz t2, L160
  c->daddiu(t2, t3, -28672);                        // daddiu t2, t3, -28672
  if (bc) {goto block_92;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t1)) > 0;                    // bgtz t1, L160
  c->daddiu(t1, t4, -29440);                        // daddiu t1, t4, -29440
  if (bc) {goto block_92;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t2)) < 0;                    // bltz t2, L160
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_92;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t1)) < 0;                    // bltz t1, L160
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_92;}                          // branch non-likely

  //beq r0, r0, L160                                // beq r0, r0, L160
  c->daddiu(a0, a0, 128);                           // daddiu a0, a0, 128
  goto block_92;                                    // branch always

  
block_171:
  c->lw(v1, 6172, v1);                              // lw v1, 6172(v1)
  c->sw(a0, 4, v1);                                 // sw a0, 4(v1)
  c->vsub(DEST::xyzw, vf23, vf23, vf24);            // vsub.xyzw vf23, vf23, vf24
  c->mov128_gpr_vf(v0, vf23);                       // qmfc2.i v0, vf23
  c->ld(ra, 0, sp);                                 // ld ra, 0(sp)
  c->lq(gp, 48, sp);                                // lq gp, 48(sp)
  c->lq(s5, 32, sp);                                // lq s5, 32(sp)
  c->lq(s4, 16, sp);                                // lq s4, 16(sp)
  //jr ra                                           // jr ra
  c->daddiu(sp, sp, 64);                            // daddiu sp, sp, 64
  goto end_of_function;                             // return

  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
end_of_function:
  return c->gprs[v0].du64[0];
}

void link() {
  cache.font_work = intern_from_c(-1, 0, "*font-work*").c();
  cache.font12_table = intern_from_c(-1, 0, "*font12-table*").c();
  cache.font24_table = intern_from_c(-1, 0, "*font24-table*").c();
  cache.draw_string_init_justify = intern_from_c(-1, 0, "draw-string-init-justify").c();
  gLinkedFunctionTable.reg("draw-string-asm-packed", execute, 512);
}

} // namespace draw_string_asm_packed
} // namespace Mips2C

namespace Mips2C::jakx {
namespace draw_string_init_justify {
struct Cache {
  void* font_work; // *font-work*
  void* font12_table; // *font12-table*
  void* font24_table; // *font24-table*
} cache;

u64 execute(void* ctxt) {
  auto* c = (ExecutionContext*)ctxt;
  printf("M2C: %s\n", __PRETTY_FUNCTION__);
  bool bc = false;
  u32 call_addr = 0;
  c->load_symbol2(v1, cache.font_work);             // lw v1, *font-work*(s7)
  c->mov64(v1, v1);                                 // or v1, v1, r0
  c->sw(a1, 6172, v1);                              // sw a1, 6172(v1)
  c->lw(a1, 4, a1);                                 // lw a1, 4(a1)
  c->sw(a0, 6176, v1);                              // sw a0, 6176(v1)
  c->lqc2(vf28, 76, a2);                            // lqc2 vf28, 76(a2)
  c->lqc2(vf29, 92, a2);                            // lqc2 vf29, 92(a2)
  c->lqc2(vf30, 108, a2);                           // lqc2 vf30, 108(a2)
  c->lqc2(vf31, 124, a2);                           // lqc2 vf31, 124(a2)
  c->lqc2(vf16, 416, v1);                           // lqc2 vf16, 416(v1)
  c->lqc2(vf17, 432, v1);                           // lqc2 vf17, 432(v1)
  c->lqc2(vf18, 448, v1);                           // lqc2 vf18, 448(v1)
  c->lqc2(vf27, 4736, v1);                          // lqc2 vf27, 4736(v1)
  c->lqc2(vf26, 4752, v1);                          // lqc2 vf26, 4752(v1)
  c->lqc2(vf25, 156, a2);                           // lqc2 vf25, 156(a2)
  c->lqc2(vf23, 44, a2);                            // lqc2 vf23, 44(a2)
  c->lqc2(vf24, 44, a2);                            // lqc2 vf24, 44(a2)
  c->lqc2(vf1, 44, a2);                             // lqc2 vf1, 44(a2)
  c->lqc2(vf2, 44, a2);                             // lqc2 vf2, 44(a2)
  c->vadd_bc(DEST::x, BC::x, vf1, vf0, vf0);        // vaddx.x vf1, vf0, vf0
  c->vadd_bc(DEST::x, BC::x, vf2, vf0, vf0);        // vaddx.x vf2, vf0, vf0
  c->vadd(DEST::x, vf1, vf0, vf25);                 // vadd.x vf1, vf0, vf25
  c->vmul_bc(DEST::x, BC::w, vf2, vf25, vf16);      // vmulw.x vf2, vf25, vf16
  c->sqc2(vf1, 464, v1);                            // sqc2 vf1, 464(v1)
  c->sqc2(vf2, 480, v1);                            // sqc2 vf2, 480(v1)
  c->lw(a1, 12, a2);                                // lw a1, 12(a2)
  c->vmove(DEST::xyzw, vf1, vf0);                   // vmove.xyzw vf1, vf0
  c->vmove(DEST::xyzw, vf2, vf0);                   // vmove.xyzw vf2, vf0
  c->vmove(DEST::xyzw, vf3, vf0);                   // vmove.xyzw vf3, vf0
  c->vmove(DEST::xyzw, vf4, vf0);                   // vmove.xyzw vf4, vf0
  // nop                                            // sll r0, r0, 0
  c->andi(a2, a1, 32);                              // andi a2, a1, 32
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(a2) != 0;                          // bne a2, r0, L286
  c->load_symbol2(a2, cache.font12_table);          // lw a2, *font12-table*(s7)
  if (bc) {goto block_2;}                           // branch non-likely

  c->mov64(a2, a2);                                 // or a2, a2, r0
  // nop                                            // sll r0, r0, 0
  c->lq(a3, 192, v1);                               // lq a3, 192(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t0, 208, v1);                               // lq t0, 208(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 224, v1);                               // lq t1, 224(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 240, v1);                               // lq t2, 240(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a3, 6080, v1);                              // sq a3, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t0, 6096, v1);                              // sq t0, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 6112, v1);                              // sq t1, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 6128, v1);                              // sq t2, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 320, v1);                           // lqc2 vf13, 320(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 336, v1);                           // lqc2 vf14, 336(v1)
  //beq r0, r0, L287                                // beq r0, r0, L287
  c->lqc2(vf15, 352, v1);                           // lqc2 vf15, 352(v1)
  goto block_3;                                     // branch always

  
block_2:
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a2, cache.font24_table);          // lw a2, *font24-table*(s7)
  c->mov64(a2, a2);                                 // or a2, a2, r0
  // nop                                            // sll r0, r0, 0
  c->lq(a3, 256, v1);                               // lq a3, 256(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t0, 272, v1);                               // lq t0, 272(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t1, 288, v1);                               // lq t1, 288(v1)
  // nop                                            // sll r0, r0, 0
  c->lq(t2, 304, v1);                               // lq t2, 304(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(a3, 6080, v1);                              // sq a3, 6080(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t0, 6096, v1);                              // sq t0, 6096(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t1, 6112, v1);                              // sq t1, 6112(v1)
  // nop                                            // sll r0, r0, 0
  c->sq(t2, 6128, v1);                              // sq t2, 6128(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 368, v1);                           // lqc2 vf13, 368(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 384, v1);                           // lqc2 vf14, 384(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 400, v1);                           // lqc2 vf15, 400(v1)
  
block_3:
  c->mov64(a3, v1);                                 // or a3, v1, r0
  // nop                                            // sll r0, r0, 0
  
block_4:
  c->lbu(t0, 4, a0);                                // lbu t0, 4(a0)
  c->daddiu(a0, a0, 1);                             // daddiu a0, a0, 1
  c->lqc2(vf20, 6080, v1);                          // lqc2 vf20, 6080(v1)
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t0) == 0;                          // beq t0, r0, L311
  c->daddiu(t1, t0, -3);                            // daddiu t1, t0, -3
  if (bc) {goto block_73;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t1)) <= 0;                   // blez t1, L303
  c->daddiu(t1, t0, -126);                          // daddiu t1, t0, -126
  if (bc) {goto block_59;}                          // branch non-likely

  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L304
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_60;}                          // branch non-likely

  c->lbu(t0, 4, a0);                                // lbu t0, 4(a0)
  c->daddiu(a0, a0, 1);                             // daddiu a0, a0, 1
  c->addiu(t1, r0, 0);                              // addiu t1, r0, 0
  c->addiu(t2, r0, 0);                              // addiu t2, r0, 0
  bc = c->sgpr64(t0) == 0;                          // beq t0, r0, L311
  c->daddiu(t3, t0, -43);                           // daddiu t3, t0, -43
  if (bc) {goto block_73;}                          // branch non-likely

  c->movz(t1, t0, t3);                              // movz t1, t0, t3
  c->daddiu(t3, t0, -45);                           // daddiu t3, t0, -45
  c->movz(t1, t0, t3);                              // movz t1, t0, t3
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t1) != 0;                          // bne t1, r0, L289
  c->daddiu(t3, t0, -91);                           // daddiu t3, t0, -91
  if (bc) {goto block_18;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L288
  c->daddiu(t2, t0, -93);                           // daddiu t2, t0, -93
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L288
  c->daddiu(t2, t0, -121);                          // daddiu t2, t0, -121
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L301
  c->daddiu(t2, t0, -89);                           // daddiu t2, t0, -89
  if (bc) {goto block_57;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L301
  c->daddiu(t2, t0, -122);                          // daddiu t2, t0, -122
  if (bc) {goto block_57;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L302
  c->daddiu(t2, t0, -90);                           // daddiu t2, t0, -90
  if (bc) {goto block_58;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L302
  c->daddiu(t2, t0, -48);                           // daddiu t2, t0, -48
  if (bc) {goto block_58;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t2)) < 0;                    // bltz t2, L304
  c->daddiu(t2, t0, -57);                           // daddiu t2, t0, -57
  if (bc) {goto block_60;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t2)) > 0;                    // bgtz t2, L304
  c->daddiu(t2, t0, -126);                          // daddiu t2, t0, -126
  if (bc) {goto block_60;}                          // branch non-likely

  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L304
  c->daddiu(t2, t0, -48);                           // daddiu t2, t0, -48
  if (bc) {goto block_60;}                          // branch non-likely

  
block_18:
  c->lbu(t0, 4, a0);                                // lbu t0, 4(a0)
  c->daddiu(a0, a0, 1);                             // daddiu a0, a0, 1
  bc = c->sgpr64(t0) == 0;                          // beq t0, r0, L311
  c->daddiu(t3, t0, -110);                          // daddiu t3, t0, -110
  if (bc) {goto block_73;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L290
  c->daddiu(t3, t0, -78);                           // daddiu t3, t0, -78
  if (bc) {goto block_38;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L290
  c->daddiu(t3, t0, -108);                          // daddiu t3, t0, -108
  if (bc) {goto block_38;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L288
  c->daddiu(t3, t0, -76);                           // daddiu t3, t0, -76
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L288
  c->daddiu(t3, t0, -119);                          // daddiu t3, t0, -119
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L288
  c->daddiu(t3, t0, -87);                           // daddiu t3, t0, -87
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L288
  c->daddiu(t3, t0, -107);                          // daddiu t3, t0, -107
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L292
  c->daddiu(t3, t0, -75);                           // daddiu t3, t0, -75
  if (bc) {goto block_41;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L292
  c->daddiu(t3, t0, -106);                          // daddiu t3, t0, -106
  if (bc) {goto block_41;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L293
  c->daddiu(t3, t0, -74);                           // daddiu t3, t0, -74
  if (bc) {goto block_43;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L293
  c->daddiu(t3, t0, -104);                          // daddiu t3, t0, -104
  if (bc) {goto block_43;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L295
  c->daddiu(t3, t0, -72);                           // daddiu t3, t0, -72
  if (bc) {goto block_47;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L295
  c->daddiu(t3, t0, -118);                          // daddiu t3, t0, -118
  if (bc) {goto block_47;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L298
  c->daddiu(t3, t0, -86);                           // daddiu t3, t0, -86
  if (bc) {goto block_52;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L298
  c->daddiu(t3, t0, -117);                          // daddiu t3, t0, -117
  if (bc) {goto block_52;}                          // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L288
  c->daddiu(t3, t0, -85);                           // daddiu t3, t0, -85
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L288
  c->daddiu(t3, t0, -48);                           // daddiu t3, t0, -48
  if (bc) {goto block_4;}                           // branch non-likely

  bc = ((s64)c->sgpr64(t3)) < 0;                    // bltz t3, L304
  c->daddiu(t4, t0, -57);                           // daddiu t4, t0, -57
  if (bc) {goto block_60;}                          // branch non-likely

  bc = ((s64)c->sgpr64(t4)) > 0;                    // bgtz t4, L304
  c->sll(t4, t2, 2);                                // sll t4, t2, 2
  if (bc) {goto block_60;}                          // branch non-likely

  c->daddu(t0, t2, t4);                             // daddu t0, t2, t4
  // nop                                            // sll r0, r0, 0
  c->sll(t0, t0, 1);                                // sll t0, t0, 1
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L289                                // beq r0, r0, L289
  c->daddu(t2, t0, t3);                             // daddu t2, t0, t3
  goto block_18;                                    // branch always

  
block_38:
  bc = c->sgpr64(t2) != 0;                          // bne t2, r0, L291
  c->addiu(t0, r0, -33);                            // addiu t0, r0, -33
  if (bc) {goto block_40;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a2, cache.font12_table);          // lw a2, *font12-table*(s7)
  c->mov64(a2, a2);                                 // or a2, a2, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 320, v1);                           // lqc2 vf13, 320(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 336, v1);                           // lqc2 vf14, 336(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 352, v1);                           // lqc2 vf15, 352(v1)
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->and_(a1, a1, t0);                              // and a1, a1, t0
  goto block_4;                                     // branch always

  
block_40:
  // nop                                            // sll r0, r0, 0
  c->load_symbol2(a2, cache.font24_table);          // lw a2, *font24-table*(s7)
  c->mov64(a2, a2);                                 // or a2, a2, r0
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf13, 368, v1);                           // lqc2 vf13, 368(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf14, 384, v1);                           // lqc2 vf14, 384(v1)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf15, 400, v1);                           // lqc2 vf15, 400(v1)
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->ori(a1, a1, 32);                               // ori a1, a1, 32
  goto block_4;                                     // branch always

  
block_41:
  c->addiu(t0, r0, -3);                             // addiu t0, r0, -3
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L288
  c->and_(a1, a1, t0);                              // and a1, a1, t0
  if (bc) {goto block_4;}                           // branch non-likely

  //beq r0, r0, L288                                // beq r0, r0, L288
  c->ori(a1, a1, 2);                                // ori a1, a1, 2
  goto block_4;                                     // branch always

  
block_43:
  c->addiu(t0, r0, -21);                            // addiu t0, r0, -21
  c->daddiu(t1, t2, -2);                            // daddiu t1, t2, -2
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L288
  c->and_(a1, a1, t0);                              // and a1, a1, t0
  if (bc) {goto block_4;}                           // branch non-likely

  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L294
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_46;}                          // branch non-likely

  //beq r0, r0, L288                                // beq r0, r0, L288
  c->ori(a1, a1, 16);                               // ori a1, a1, 16
  goto block_4;                                     // branch always

  
block_46:
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->ori(a1, a1, 4);                                // ori a1, a1, 4
  goto block_4;                                     // branch always

  
block_47:
  c->mov128_vf_gpr(vf1, t2);                        // qmtc2.i vf1, t2
  c->daddiu(t0, t1, -45);                           // daddiu t0, t1, -45
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L297
  c->vitof0(DEST::xyzw, vf1, vf1);                  // vitof0.xyzw vf1, vf1
  if (bc) {goto block_51;}                          // branch non-likely

  bc = c->sgpr64(t0) == 0;                          // beq t0, r0, L296
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_50;}                          // branch non-likely

  //beq r0, r0, L288                                // beq r0, r0, L288
  c->vadd_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vaddx.x vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_50:
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->vsub_bc(DEST::x, BC::x, vf23, vf23, vf1);      // vsubx.x vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_51:
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->vadd_bc(DEST::x, BC::x, vf23, vf0, vf1);       // vaddx.x vf23, vf0, vf1
  goto block_4;                                     // branch always

  
block_52:
  c->mov128_vf_gpr(vf1, t2);                        // qmtc2.i vf1, t2
  c->daddiu(t0, t1, -45);                           // daddiu t0, t1, -45
  bc = c->sgpr64(t1) == 0;                          // beq t1, r0, L300
  c->vitof0(DEST::xyzw, vf1, vf1);                  // vitof0.xyzw vf1, vf1
  if (bc) {goto block_56;}                          // branch non-likely

  bc = c->sgpr64(t0) == 0;                          // beq t0, r0, L299
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_55;}                          // branch non-likely

  //beq r0, r0, L288                                // beq r0, r0, L288
  c->vadd_bc(DEST::y, BC::x, vf23, vf23, vf1);      // vaddx.y vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_55:
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->vsub_bc(DEST::y, BC::x, vf23, vf23, vf1);      // vsubx.y vf23, vf23, vf1
  goto block_4;                                     // branch always

  
block_56:
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->vadd_bc(DEST::y, BC::x, vf23, vf0, vf1);       // vaddx.y vf23, vf0, vf1
  goto block_4;                                     // branch always

  
block_57:
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->sqc2(vf23, 496, v1);                           // sqc2 vf23, 496(v1)
  goto block_4;                                     // branch always

  
block_58:
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->lqc2(vf23, 496, v1);                           // lqc2 vf23, 496(v1)
  goto block_4;                                     // branch always

  
block_59:
  // nop                                            // sll r0, r0, 0
  c->ori(a1, a1, 64);                               // ori a1, a1, 64
  c->lbu(t0, 4, a0);                                // lbu t0, 4(a0)
  c->daddiu(a0, a0, 1);                             // daddiu a0, a0, 1
  // nop                                            // sll r0, r0, 0
  c->sll(t1, t0, 4);                                // sll t1, t0, 4
  //beq r0, r0, L308                                // beq r0, r0, L308
  c->vsub(DEST::xyzw, vf1, vf25, vf23);             // vsub.xyzw vf1, vf25, vf23
  goto block_67;                                    // branch always

  
block_60:
  // nop                                            // sll r0, r0, 0
  c->addiu(t1, r0, -65);                            // addiu t1, r0, -65
  c->and_(a1, a1, t1);                              // and a1, a1, t1
  c->sll(t1, t0, 4);                                // sll t1, t0, 4
  c->vsub(DEST::xyzw, vf1, vf25, vf23);             // vsub.xyzw vf1, vf25, vf23
  c->daddiu(t2, t0, -10);                           // daddiu t2, t0, -10
  bc = c->sgpr64(t2) == 0;                          // beq t2, r0, L305
  c->daddiu(t0, t0, -13);                           // daddiu t0, t0, -13
  if (bc) {goto block_62;}                          // branch non-likely

  bc = c->sgpr64(t0) != 0;                          // bne t0, r0, L308
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_67;}                          // branch non-likely

  
block_62:
  c->vsub(DEST::xyzw, vf1, vf23, vf24);             // vsub.xyzw vf1, vf23, vf24
  c->andi(t0, a1, 16);                              // andi t0, a1, 16
  bc = c->sgpr64(t0) != 0;                          // bne t0, r0, L306
  c->andi(t0, a1, 4);                               // andi t0, a1, 4
  if (bc) {goto block_65;}                          // branch non-likely

  bc = c->sgpr64(t0) != 0;                          // bne t0, r0, L307
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_66;}                          // branch non-likely

  c->vadd_bc(DEST::x, BC::x, vf23, vf0, vf24);      // vaddx.x vf23, vf0, vf24
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf23, 640, a3);                           // sqc2 vf23, 640(a3)
  c->vadd_bc(DEST::y, BC::w, vf23, vf23, vf15);     // vaddw.y vf23, vf23, vf15
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->daddiu(a3, a3, 16);                            // daddiu a3, a3, 16
  goto block_4;                                     // branch always

  
block_65:
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf2, 464, v1);                            // lqc2 vf2, 464(v1)
  c->vsub(DEST::x, vf23, vf2, vf1);                 // vsub.x vf23, vf2, vf1
  c->sqc2(vf23, 640, a3);                           // sqc2 vf23, 640(a3)
  c->vadd_bc(DEST::x, BC::x, vf23, vf0, vf24);      // vaddx.x vf23, vf0, vf24
  c->vadd_bc(DEST::y, BC::w, vf23, vf23, vf15);     // vaddw.y vf23, vf23, vf15
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->daddiu(a3, a3, 16);                            // daddiu a3, a3, 16
  goto block_4;                                     // branch always

  
block_66:
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf2, 480, v1);                            // lqc2 vf2, 480(v1)
  c->vmul_bc(DEST::x, BC::w, vf1, vf1, vf16);       // vmulw.x vf1, vf1, vf16
  // nop                                            // sll r0, r0, 0
  c->vsub(DEST::x, vf23, vf2, vf1);                 // vsub.x vf23, vf2, vf1
  c->sqc2(vf23, 640, a3);                           // sqc2 vf23, 640(a3)
  c->vadd_bc(DEST::x, BC::x, vf23, vf0, vf24);      // vaddx.x vf23, vf0, vf24
  c->vadd_bc(DEST::y, BC::w, vf23, vf23, vf15);     // vaddw.y vf23, vf23, vf15
  //beq r0, r0, L288                                // beq r0, r0, L288
  c->daddiu(a3, a3, 16);                            // daddiu a3, a3, 16
  goto block_4;                                     // branch always

  
block_67:
  c->addu(t0, t1, a2);                              // addu t0, t1, a2
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf5, -96, t0);                            // lqc2 vf5, -96(t0)
  c->mov128_gpr_vf(t0, vf1);                        // qmfc2.i t0, vf1
  bc = ((s64)c->sgpr64(t0)) < 0;                    // bltz t0, L311
  c->sra(t0, t0, 31);                               // sra t0, t0, 31
  if (bc) {goto block_73;}                          // branch non-likely

  c->vmul(DEST::xyzw, vf19, vf5, vf13);             // vmul.xyzw vf19, vf5, vf13
  c->andi(t0, a1, 2);                               // andi t0, a1, 2
  bc = c->sgpr64(t0) == 0;                          // beq t0, r0, L309
  c->andi(t0, a1, 64);                              // andi t0, a1, 64
  if (bc) {goto block_71;}                          // branch non-likely

  bc = c->sgpr64(t0) != 0;                          // bne t0, r0, L309
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_71;}                          // branch non-likely

  //beq r0, r0, L310                                // beq r0, r0, L310
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf19);     // vaddw.x vf23, vf23, vf19
  goto block_72;                                    // branch always

  
block_71:
  // nop                                            // sll r0, r0, 0
  c->vadd_bc(DEST::x, BC::w, vf23, vf23, vf14);     // vaddw.x vf23, vf23, vf14
  
block_72:
  //beq r0, r0, L288                                // beq r0, r0, L288
  // nop                                            // sll r0, r0, 0
  goto block_4;                                     // branch always

  
block_73:
  c->vsub(DEST::xyzw, vf1, vf23, vf24);             // vsub.xyzw vf1, vf23, vf24
  c->andi(a0, a1, 16);                              // andi a0, a1, 16
  bc = c->sgpr64(a0) != 0;                          // bne a0, r0, L312
  c->andi(a0, a1, 4);                               // andi a0, a1, 4
  if (bc) {goto block_76;}                          // branch non-likely

  bc = c->sgpr64(a0) != 0;                          // bne a0, r0, L313
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_77;}                          // branch non-likely

  c->vadd_bc(DEST::x, BC::x, vf23, vf0, vf24);      // vaddx.x vf23, vf0, vf24
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L314                                // beq r0, r0, L314
  c->sqc2(vf23, 640, a3);                           // sqc2 vf23, 640(a3)
  goto block_78;                                    // branch always

  
block_76:
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf2, 464, v1);                            // lqc2 vf2, 464(v1)
  c->vsub(DEST::x, vf23, vf2, vf1);                 // vsub.x vf23, vf2, vf1
  // nop                                            // sll r0, r0, 0
  //beq r0, r0, L314                                // beq r0, r0, L314
  c->sqc2(vf23, 640, a3);                           // sqc2 vf23, 640(a3)
  goto block_78;                                    // branch always

  
block_77:
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf2, 480, v1);                            // lqc2 vf2, 480(v1)
  c->vmul_bc(DEST::x, BC::w, vf1, vf1, vf16);       // vmulw.x vf1, vf1, vf16
  // nop                                            // sll r0, r0, 0
  c->vsub(DEST::x, vf23, vf2, vf1);                 // vsub.x vf23, vf2, vf1
  c->sqc2(vf23, 640, a3);                           // sqc2 vf23, 640(a3)
  
block_78:
  c->vsub(DEST::xyzw, vf23, vf23, vf24);            // vsub.xyzw vf23, vf23, vf24
  c->mov128_gpr_vf(v0, vf23);                       // qmfc2.i v0, vf23
  //jr ra                                           // jr ra
  c->daddu(sp, sp, r0);                             // daddu sp, sp, r0
  goto end_of_function;                             // return

  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
end_of_function:
  return c->gprs[v0].du64[0];
}

void link() {
  cache.font_work = intern_from_c(-1, 0, "*font-work*").c();
  cache.font12_table = intern_from_c(-1, 0, "*font12-table*").c();
  cache.font24_table = intern_from_c(-1, 0, "*font24-table*").c();
  gLinkedFunctionTable.reg("draw-string-init-justify", execute, 256);
}

} // namespace draw_string_init_justify
} // namespace Mips2C
