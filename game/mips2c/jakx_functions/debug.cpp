//--------------------------MIPS2C---------------------
// clang-format off
#include "game/mips2c/mips2c_private.h"
#include "game/kernel/jakx/kscheme.h"
using ::jakx::intern_from_c;
namespace Mips2C::jakx {
namespace debug_line_clip {
struct Cache {
  void* view_get_active_math_camera; // view-get-active-math-camera
} cache;

void qfsrv_same_mtsab_4(ExecutionContext* c, int reg) {
  u32 temp[4];
  auto& val = c->gprs[reg];
  temp[0] = val.du32[1];
  temp[1] = val.du32[2];
  temp[2] = val.du32[3];
  temp[3] = val.du32[0];
  val.du32[0] = temp[0];
  val.du32[1] = temp[1];
  val.du32[2] = temp[2];
  val.du32[3] = temp[3];
}

u64 execute(void* ctxt) {
  auto* c = (ExecutionContext*)ctxt;
  bool bc = false;
  u32 call_addr = 0;
  bool cop1_bc = false;
  c->daddiu(sp, sp, -80);                           // daddiu sp, sp, -80
  c->sd(ra, 0, sp);                                 // sd ra, 0(sp)
  c->sq(s3, 16, sp);                                // sq s3, 16(sp)
  c->sq(s4, 32, sp);                                // sq s4, 32(sp)
  c->sq(s5, 48, sp);                                // sq s5, 48(sp)
  c->sq(gp, 64, sp);                                // sq gp, 64(sp)
  c->mov64(gp, a0);                                 // or gp, a0, r0
  c->mov64(s5, a1);                                 // or s5, a1, r0
  c->mov64(s4, a2);                                 // or s4, a2, r0
  c->mov64(s3, a3);                                 // or s3, a3, r0
  c->load_symbol2(t9, cache.view_get_active_math_camera);// lw t9, view-get-active-math-camera(s7)
  call_addr = c->gprs[t9].du32[0];                  // function call:
  c->sll(v0, ra, 0);                                // sll v0, ra, 0
  c->jalr(call_addr);                               // jalr ra, t9
  c->mov64(v1, v0);                                 // or v1, v0, r0
  c->lqc2(vf9, 0, s4);                              // lqc2 vf9, 0(s4)
  c->lqc2(vf10, 0, s3);                             // lqc2 vf10, 0(s3)
  c->lqc2(vf16, 940, v1);                           // lqc2 vf16, 940(v1)
  c->lqc2(vf17, 956, v1);                           // lqc2 vf17, 956(v1)
  c->lqc2(vf18, 972, v1);                           // lqc2 vf18, 972(v1)
  c->lqc2(vf19, 988, v1);                           // lqc2 vf19, 988(v1)
  c->vmula_bc(DEST::xyzw, BC::x, vf16, vf9);        // vmulax.xyzw acc, vf16, vf9
  c->vmadda_bc(DEST::xyzw, BC::y, vf17, vf9);       // vmadday.xyzw acc, vf17, vf9
  c->vmadda_bc(DEST::xyzw, BC::z, vf18, vf9);       // vmaddaz.xyzw acc, vf18, vf9
  c->vmsub_bc(DEST::xyzw, BC::w, vf11, vf19, vf0);  // vmsubw.xyzw vf11, vf19, vf0
  c->vmula_bc(DEST::xyzw, BC::x, vf16, vf10);       // vmulax.xyzw acc, vf16, vf10
  c->vmadda_bc(DEST::xyzw, BC::y, vf17, vf10);      // vmadday.xyzw acc, vf17, vf10
  c->vmadda_bc(DEST::xyzw, BC::z, vf18, vf10);      // vmaddaz.xyzw acc, vf18, vf10
  c->vmsub_bc(DEST::xyzw, BC::w, vf12, vf19, vf0);  // vmsubw.xyzw vf12, vf19, vf0
  c->mov128_gpr_vf(v1, vf11);                       // qmfc2.i v1, vf11
  c->pcgtw(v1, r0, v1);                             // pcgtw v1, r0, v1
  c->ppach(a0, r0, v1);                             // ppach a0, r0, v1
  c->mov128_gpr_vf(v1, vf12);                       // qmfc2.i v1, vf12
  c->pcgtw(v1, r0, v1);                             // pcgtw v1, r0, v1
  c->ppach(a1, r0, v1);                             // ppach a1, r0, v1
  c->and_(v1, a0, a1);                              // and v1, a0, a1
  bc = c->sgpr64(v1) != 0;                          // bne v1, r0, L258
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_10;}                          // branch non-likely

  c->vmove(DEST::xyzw, vf13, vf9);                  // vmove.xyzw vf13, vf9
  c->vmove(DEST::xyzw, vf14, vf10);                 // vmove.xyzw vf14, vf10
  c->or_(a0, a0, a1);                               // or a0, a0, a1
  bc = c->sgpr64(a0) == 0;                          // beq a0, r0, L257
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_9;}                           // branch non-likely

  c->mov128_gpr_vf(a0, vf11);                       // qmfc2.i a0, vf11
  c->mov128_gpr_vf(a1, vf12);                       // qmfc2.i a1, vf12
  c->mtc1(f0, r0);                                  // mtc1 f0, r0
  c->mtc1(f1, r0);                                  // mtc1 f1, r0
  c->lui(a2, 16256);                                // lui a2, 16256
  c->mtc1(f2, a2);                                  // mtc1 f2, a2
  c->addiu(a2, r0, 3);                              // addiu a2, r0, 3
  // mtsab r0, 4

block_3:
  c->mtc1(f3, a0);                                  // mtc1 f3, a0
  c->mtc1(f4, a1);                                  // mtc1 f4, a1
  c->subs(f5, f3, f4);                              // sub.s f5, f3, f4
  cop1_bc = c->fprs[f3] < c->fprs[f0];              // c.lt.s f3, f0
  bc = !cop1_bc;                                    // bc1f L255
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_5;}                           // branch non-likely

  c->divs(f6, f3, f5);                              // div.s f6, f3, f5
  c->maxs(f1, f6, f1);                              // max.s f1, f6, f1

block_5:
  cop1_bc = c->fprs[f4] < c->fprs[f0];              // c.lt.s f4, f0
  bc = !cop1_bc;                                    // bc1f L256
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_7;}                           // branch non-likely

  c->divs(f3, f3, f5);                              // div.s f3, f3, f5
  c->mins(f2, f3, f2);                              // min.s f2, f3, f2

block_7:
  // qfsrv a0, a0, a0
  qfsrv_same_mtsab_4(c, a0);
  // qfsrv a1, a1, a1
  qfsrv_same_mtsab_4(c, a1);
  bc = c->sgpr64(a2) != 0;                          // bne a2, r0, L254
  c->daddiu(a2, a2, -1);                            // daddiu a2, a2, -1
  if (bc) {goto block_3;}                           // branch non-likely

  c->mfc1(a0, f1);                                  // mfc1 a0, f1
  c->mfc1(a1, f2);                                  // mfc1 a1, f2
  c->mov128_vf_gpr(vf20, a0);                       // qmtc2.i vf20, a0
  c->mov128_vf_gpr(vf21, a1);                       // qmtc2.i vf21, a1
  c->vsub(DEST::xyzw, vf15, vf10, vf9);             // vsub.xyzw vf15, vf10, vf9
  c->vmul_bc(DEST::xyzw, BC::x, vf13, vf15, vf20);  // vmulx.xyzw vf13, vf15, vf20
  c->vadd(DEST::xyzw, vf13, vf9, vf13);             // vadd.xyzw vf13, vf9, vf13
  c->vmul_bc(DEST::xyzw, BC::x, vf14, vf15, vf21);  // vmulx.xyzw vf14, vf15, vf21
  c->vadd(DEST::xyzw, vf14, vf9, vf14);             // vadd.xyzw vf14, vf9, vf14

block_9:
  c->sqc2(vf13, 0, gp);                             // sqc2 vf13, 0(gp)
  c->sqc2(vf14, 0, s5);                             // sqc2 vf14, 0(s5)

block_10:
  c->daddiu(v0, s7, 4);                             // daddiu v0, s7, 4
  c->movn(v0, s7, v1);                              // movn v0, s7, v1
  c->ld(ra, 0, sp);                                 // ld ra, 0(sp)
  c->lq(gp, 64, sp);                                // lq gp, 64(sp)
  c->lq(s5, 48, sp);                                // lq s5, 48(sp)
  c->lq(s4, 32, sp);                                // lq s4, 32(sp)
  c->lq(s3, 16, sp);                                // lq s3, 16(sp)
  //jr ra                                           // jr ra
  c->daddiu(sp, sp, 80);                            // daddiu sp, sp, 80
  goto end_of_function;                             // return

  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
end_of_function:
  return c->gprs[v0].du64[0];
}

void link() {
  cache.view_get_active_math_camera = intern_from_c(-1, 0, "view-get-active-math-camera").c();
  gLinkedFunctionTable.reg("debug-line-clip?", execute, 128);
}

} // namespace debug_line_clip
} // namespace Mips2C
