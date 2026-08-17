
//--------------------------MIPS2C---------------------
// clang-format off
#include "game/mips2c/mips2c_private.h"
#include "game/kernel/jakx/kscheme.h"
using ::jakx::intern_from_c;
namespace Mips2C::jakx {
namespace collide_probe_node {
struct Cache {
  void* collide_probe_stack; // *collide-probe-stack*
} cache;

u64 execute(void* ctxt) {
  auto* c = (ExecutionContext*)ctxt;
  bool bc = false;
  u32 call_addr = 0;
  c->daddiu(sp, sp, -96);                           // daddiu sp, sp, -96
  c->sd(ra, 0, sp);                                 // sd ra, 0(sp)
  c->sq(s2, 16, sp);                                // sq s2, 16(sp)
  c->sq(s3, 32, sp);                                // sq s3, 32(sp)
  c->sq(s4, 48, sp);                                // sq s4, 48(sp)
  c->sq(s5, 64, sp);                                // sq s5, 64(sp)
  c->sq(gp, 80, sp);                                // sq gp, 80(sp)
  c->load_symbol2(v1, cache.collide_probe_stack);   // lw v1, *collide-probe-stack*(s7)
  c->sw(a0, 0, v1);                                 // sw a0, 0(v1)
  c->daddiu(a0, v1, 8);                             // daddiu a0, v1, 8
  c->sh(a1, 4, v1);                                 // sh a1, 4(v1)
  c->addiu(a1, r0, 1);                              // addiu a1, r0, 1
  c->sh(a1, 6, v1);                                 // sh a1, 6(v1)
  c->lq(a1, 368, a3);                               // lq a1, 368(a3)
  c->lq(t0, 384, a3);                               // lq t0, 384(a3)
  c->lqc2(vf1, 288, a3);                            // lqc2 vf1, 288(a3)
  c->lqc2(vf2, 304, a3);                            // lqc2 vf2, 304(a3)
  c->lqc2(vf3, 320, a3);                            // lqc2 vf3, 320(a3)
  c->lqc2(vf4, 336, a3);                            // lqc2 vf4, 336(a3)
  c->lw(a3, 0, a2);                                 // lw a3, 0(a2)
  c->sll(t1, a3, 3);                                // sll t1, a3, 3
  c->addu(t1, t1, a2);                              // addu t1, t1, a2
  c->addiu(t1, t1, 16);                             // addiu t1, t1, 16

block_1:
  bc = c->sgpr64(a0) == c->sgpr64(v1);              // beq a0, v1, L71
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_32;}                          // branch non-likely

  c->daddiu(a0, a0, -8);                            // daddiu a0, a0, -8
  c->lw(t2, 0, a0);                                 // lw t2, 0(a0)
  c->lhu(t3, 4, a0);                                // lhu t3, 4(a0)
  c->lhu(t4, 6, a0);                                // lhu t4, 6(a0)

block_3:
  c->lqc2(vf5, 12, t2);                             // lqc2 vf5, 12(t2)
  c->lqc2(vf6, 44, t2);                             // lqc2 vf6, 44(t2)
  c->daddiu(t5, t3, -4);                            // daddiu t5, t3, -4
  c->lqc2(vf7, 76, t2);                             // lqc2 vf7, 76(t2)
  bc = ((s64)c->sgpr64(t5)) < 0;                    // bltz t5, L61
  c->lqc2(vf8, 108, t2);                            // lqc2 vf8, 108(t2)
  if (bc) {goto block_5;}                           // branch non-likely

  // Unknown instr: pref r0, 140, t2

block_5:
  // Unknown instr: vcallms 0
  //  nop                        |  mulaw.xyzw ACC, vf04, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf4, vf0);
  //  nop                        |  maddax.xyzw ACC, vf01, vf05
  c->vmadda_bc(DEST::xyzw, BC::x, vf1, vf5);
  //  nop                        |  madday.xyzw ACC, vf02, vf05
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf5);
  //  nop                        |  maddz.xyz vf05, vf03, vf05
  c->vmadd_bc(DEST::xyz, BC::z, vf5, vf3, vf5);
  //  nop                        |  mulaw.xyzw ACC, vf04, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf4, vf0);
  //  nop                        |  maddax.xyzw ACC, vf01, vf06
  c->vmadda_bc(DEST::xyzw, BC::x, vf1, vf6);
  //  nop                        |  madday.xyzw ACC, vf02, vf06
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf6);
  //  nop                        |  maddz.xyz vf06, vf03, vf06
  c->vmadd_bc(DEST::xyz, BC::z, vf6, vf3, vf6);
  //  nop                        |  mulaw.xyzw ACC, vf04, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf4, vf0);
  //  nop                        |  maddax.xyzw ACC, vf01, vf07
  c->vmadda_bc(DEST::xyzw, BC::x, vf1, vf7);
  //  nop                        |  madday.xyzw ACC, vf02, vf07
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf7);
  //  nop                        |  maddz.xyz vf07, vf03, vf07
  c->vmadd_bc(DEST::xyz, BC::z, vf7, vf3, vf7);
  //  nop                        |  mulaw.xyzw ACC, vf04, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf4, vf0);
  //  nop                        |  maddax.xyzw ACC, vf01, vf08
  c->vmadda_bc(DEST::xyzw, BC::x, vf1, vf8);
  //  nop                        |  madday.xyzw ACC, vf02, vf08
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf8);
  //  nop                        |  maddz.xyz vf08, vf03, vf08
  c->vmadd_bc(DEST::xyz, BC::z, vf8, vf3, vf8);
  //  nop                        |  subw.xyz vf09, vf05, vf05
  c->vsub_bc(DEST::xyz, BC::w, vf9, vf5, vf5);
  //  nop                        |  subw.xyz vf11, vf06, vf06
  c->vsub_bc(DEST::xyz, BC::w, vf11, vf6, vf6);
  //  nop                        |  subw.xyz vf13, vf07, vf07
  c->vsub_bc(DEST::xyz, BC::w, vf13, vf7, vf7);
  //  nop                        |  subw.xyz vf15, vf08, vf08
  c->vsub_bc(DEST::xyz, BC::w, vf15, vf8, vf8);
  //  nop                        |  addw.xyz vf10, vf05, vf05
  c->vadd_bc(DEST::xyz, BC::w, vf10, vf5, vf5);
  //  nop                        |  addw.xyz vf12, vf06, vf06
  c->vadd_bc(DEST::xyz, BC::w, vf12, vf6, vf6);
  //  nop                        |  addw.xyz vf14, vf07, vf07
  c->vadd_bc(DEST::xyz, BC::w, vf14, vf7, vf7);
  //  nop                        |  addw.xyz vf16, vf08, vf08
  c->vadd_bc(DEST::xyz, BC::w, vf16, vf8, vf8);
  //  nop                        |  ftoi0.xyzw vf09, vf09
  c->vftoi0(DEST::xyzw, vf9, vf9);
  //  nop                        |  ftoi0.xyzw vf11, vf11
  c->vftoi0(DEST::xyzw, vf11, vf11);
  //  nop                        |  ftoi0.xyzw vf13, vf13
  c->vftoi0(DEST::xyzw, vf13, vf13);
  //  nop                        |  ftoi0.xyzw vf15, vf15
  c->vftoi0(DEST::xyzw, vf15, vf15);
  //  nop                        |  ftoi0.xyzw vf10, vf10
  c->vftoi0(DEST::xyzw, vf10, vf10);
  //  nop                        |  ftoi0.xyzw vf12, vf12
  c->vftoi0(DEST::xyzw, vf12, vf12);
  //  nop                        |  ftoi0.xyzw vf14, vf14 :e
  c->vftoi0(DEST::xyzw, vf14, vf14);
  //  nop                        |  ftoi0.xyzw vf16, vf16
  c->vftoi0(DEST::xyzw, vf16, vf16);
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(t5, vf9);                        // qmfc2.i t5, vf9
  c->mov128_gpr_vf(t6, vf10);                       // qmfc2.i t6, vf10
  c->mov128_gpr_vf(s2, vf11);                       // qmfc2.i s2, vf11
  c->mov128_gpr_vf(s3, vf12);                       // qmfc2.i s3, vf12
  c->mov128_gpr_vf(s4, vf13);                       // qmfc2.i s4, vf13
  c->mov128_gpr_vf(s5, vf14);                       // qmfc2.i s5, vf14
  c->mov128_gpr_vf(ra, vf15);                       // qmfc2.i ra, vf15
  c->mov128_gpr_vf(t9, vf16);                       // qmfc2.i t9, vf16
  // nop                                            // sll r0, r0, 0
  c->pcgtw(t5, t5, t0);                             // pcgtw t5, t5, t0
  c->lw(t8, 4, t2);                                 // lw t8, 4(t2)
  c->pcgtw(t7, a1, t6);                             // pcgtw t7, a1, t6
  c->lw(t6, 36, t2);                                // lw t6, 36(t2)
  c->por(t5, t5, t7);                               // por t5, t5, t7
  c->lw(t7, 68, t2);                                // lw t7, 68(t2)
  c->ppach(gp, r0, t5);                             // ppach gp, r0, t5
  c->lw(t5, 100, t2);                               // lw t5, 100(t2)
  c->pcgtw(s2, s2, t0);                             // pcgtw s2, s2, t0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(s3, a1, s3);                             // pcgtw s3, a1, s3
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(s3, s2, s3);                               // por s3, s2, s3
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(s3, r0, s3);                             // ppach s3, r0, s3
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(s4, s4, t0);                             // pcgtw s4, s4, t0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(s5, a1, s5);                             // pcgtw s5, a1, s5
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(s5, s4, s5);                               // por s5, s4, s5
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(s5, r0, s5);                             // ppach s5, r0, s5
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(ra, ra, t0);                             // pcgtw ra, ra, t0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(t9, a1, t9);                             // pcgtw t9, a1, t9
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(t9, ra, t9);                               // por t9, ra, t9
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(t9, r0, t9);                             // ppach t9, r0, t9
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->dsll(s4, gp, 16);                              // dsll s4, gp, 16
  c->dsll(gp, s3, 16);                              // dsll gp, s3, 16
  c->dsll(ra, s5, 16);                              // dsll ra, s5, 16
  c->dsll(t9, t9, 16);                              // dsll t9, t9, 16
  bc = c->sgpr64(t4) == 0;                          // beq t4, r0, L66
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_19;}                          // branch non-likely

  bc = c->sgpr64(s4) != 0;                          // bne s4, r0, L62
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_8;}                           // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lbu(s5, 2, t2);                                // lbu s5, 2(t2)
  // nop                                            // sll r0, r0, 0
  c->lbu(s4, 3, t2);                                // lbu s4, 3(t2)
  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t8, -8, a0);                                // sw t8, -8(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(s5, -4, a0);                                // sh s5, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(s4, -2, a0);                                // sh s4, -2(a0)

block_8:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L65
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_17;}                          // branch non-likely

  bc = c->sgpr64(gp) != 0;                          // bne gp, r0, L63
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_11;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lbu(t8, 34, t2);                               // lbu t8, 34(t2)
  // nop                                            // sll r0, r0, 0
  c->lbu(gp, 35, t2);                               // lbu gp, 35(t2)
  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t6, -8, a0);                                // sw t6, -8(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t8, -4, a0);                                // sh t8, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(gp, -2, a0);                                // sh gp, -2(a0)

block_11:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L65
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_17;}                          // branch non-likely

  bc = c->sgpr64(ra) != 0;                          // bne ra, r0, L64
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_14;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lbu(t6, 66, t2);                               // lbu t6, 66(t2)
  // nop                                            // sll r0, r0, 0
  c->lbu(t8, 67, t2);                               // lbu t8, 67(t2)
  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t7, -8, a0);                                // sw t7, -8(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t6, -4, a0);                                // sh t6, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t8, -2, a0);                                // sh t8, -2(a0)

block_14:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L65
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_17;}                          // branch non-likely

  bc = c->sgpr64(t9) != 0;                          // bne t9, r0, L65
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_17;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lbu(t6, 98, t2);                               // lbu t6, 98(t2)
  // nop                                            // sll r0, r0, 0
  c->lbu(t7, 99, t2);                               // lbu t7, 99(t2)
  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t5, -8, a0);                                // sw t5, -8(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t6, -4, a0);                                // sh t6, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t7, -2, a0);                                // sh t7, -2(a0)

block_17:
  bc = ((s64)c->sgpr64(t3)) > 0;                    // bgtz t3, L60
  c->daddiu(t2, t2, 128);                           // daddiu t2, t2, 128
  if (bc) {goto block_3;}                           // branch non-likely

  //beq r0, r0, L59                                 // beq r0, r0, L59
  // nop                                            // sll r0, r0, 0
  goto block_1;                                     // branch always


block_19:
  bc = c->sgpr64(s4) != 0;                          // bne s4, r0, L67
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_21;}                          // branch non-likely

  c->daddiu(a3, a3, 1);                             // daddiu a3, a3, 1
  c->sw(t8, 0, t1);                                 // sw t8, 0(t1)
  c->daddiu(t1, t1, 8);                             // daddiu t1, t1, 8
  c->sw(s7, -4, t1);                                // sw s7, -4(t1)

block_21:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L70
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_30;}                          // branch non-likely

  bc = c->sgpr64(gp) != 0;                          // bne gp, r0, L68
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_24;}                          // branch non-likely

  c->daddiu(a3, a3, 1);                             // daddiu a3, a3, 1
  c->sw(t6, 0, t1);                                 // sw t6, 0(t1)
  c->daddiu(t1, t1, 8);                             // daddiu t1, t1, 8
  c->sw(s7, -4, t1);                                // sw s7, -4(t1)

block_24:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L70
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_30;}                          // branch non-likely

  bc = c->sgpr64(ra) != 0;                          // bne ra, r0, L69
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_27;}                          // branch non-likely

  c->daddiu(a3, a3, 1);                             // daddiu a3, a3, 1
  c->sw(t7, 0, t1);                                 // sw t7, 0(t1)
  c->daddiu(t1, t1, 8);                             // daddiu t1, t1, 8
  c->sw(s7, -4, t1);                                // sw s7, -4(t1)

block_27:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L70
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_30;}                          // branch non-likely

  bc = c->sgpr64(t9) != 0;                          // bne t9, r0, L70
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_30;}                          // branch non-likely

  c->daddiu(a3, a3, 1);                             // daddiu a3, a3, 1
  c->sw(t5, 0, t1);                                 // sw t5, 0(t1)
  c->daddiu(t1, t1, 8);                             // daddiu t1, t1, 8
  c->sw(s7, -4, t1);                                // sw s7, -4(t1)

block_30:
  bc = ((s64)c->sgpr64(t3)) > 0;                    // bgtz t3, L60
  c->daddiu(t2, t2, 128);                           // daddiu t2, t2, 128
  if (bc) {goto block_3;}                           // branch non-likely

  //beq r0, r0, L59                                 // beq r0, r0, L59
  // nop                                            // sll r0, r0, 0
  goto block_1;                                     // branch always


block_32:
  // nop                                            // sll r0, r0, 0
  c->sw(a3, 0, a2);                                 // sw a3, 0(a2)
  c->gprs[v0].du64[0] = 0;                          // or v0, r0, r0
  c->ld(ra, 0, sp);                                 // ld ra, 0(sp)
  c->lq(gp, 80, sp);                                // lq gp, 80(sp)
  c->lq(s5, 64, sp);                                // lq s5, 64(sp)
  c->lq(s4, 48, sp);                                // lq s4, 48(sp)
  c->lq(s3, 32, sp);                                // lq s3, 32(sp)
  c->lq(s2, 16, sp);                                // lq s2, 16(sp)
  //jr ra                                           // jr ra
  c->daddiu(sp, sp, 96);                            // daddiu sp, sp, 96
  goto end_of_function;                             // return

  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  end_of_function:
  return c->gprs[v0].du64[0];
}

void link() {
  cache.collide_probe_stack = intern_from_c(-1, 0, "*collide-probe-stack*").c();
  gLinkedFunctionTable.reg("collide-probe-node", execute, 256);
}

} // namespace collide_probe_node
} // namespace Mips2C
// add collide_probe_node::link to the link callback table for the object file.
// FWD DEC:
namespace collide_probe_node { extern void link(); }

//--------------------------MIPS2C---------------------
// clang-format off
#include "game/mips2c/mips2c_private.h"
#include "game/kernel/jakx/kscheme.h"
using ::jakx::intern_from_c;
namespace Mips2C::jakx {
namespace collide_probe_instance_tie {
struct Cache {
  void* collide_probe_stack; // *collide-probe-stack*
} cache;

u64 execute(void* ctxt) {
  auto* c = (ExecutionContext*)ctxt;
  bool bc = false;
  u32 call_addr = 0;
  c->daddiu(sp, sp, -128);                          // daddiu sp, sp, -128
  c->sd(ra, 0, sp);                                 // sd ra, 0(sp)
  c->sq(s0, 16, sp);                                // sq s0, 16(sp)
  c->sq(s1, 32, sp);                                // sq s1, 32(sp)
  c->sq(s2, 48, sp);                                // sq s2, 48(sp)
  c->sq(s3, 64, sp);                                // sq s3, 64(sp)
  c->sq(s4, 80, sp);                                // sq s4, 80(sp)
  c->sq(s5, 96, sp);                                // sq s5, 96(sp)
  c->sq(gp, 112, sp);                               // sq gp, 112(sp)
  c->load_symbol2(v1, cache.collide_probe_stack);   // lw v1, *collide-probe-stack*(s7)
  c->sw(a0, 0, v1);                                 // sw a0, 0(v1)
  c->daddiu(a0, v1, 8);                             // daddiu a0, v1, 8
  c->sh(a1, 4, v1);                                 // sh a1, 4(v1)
  c->sh(a3, 6, v1);                                 // sh a3, 6(v1)
  c->addiu(a1, r0, 0);                              // addiu a1, r0, 0
  c->addiu(a3, r0, 0);                              // addiu a3, r0, 0
  c->lq(t1, 368, t0);                               // lq t1, 368(t0)
  c->lq(t2, 384, t0);                               // lq t2, 384(t0)
  c->lqc2(vf1, 288, t0);                            // lqc2 vf1, 288(t0)
  c->lqc2(vf2, 304, t0);                            // lqc2 vf2, 304(t0)
  c->lqc2(vf3, 320, t0);                            // lqc2 vf3, 320(t0)
  c->lqc2(vf4, 336, t0);                            // lqc2 vf4, 336(t0)

block_1:
  bc = c->sgpr64(a0) == c->sgpr64(v1);              // beq a0, v1, L55
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_59;}                          // branch non-likely

  c->daddiu(a0, a0, -8);                            // daddiu a0, a0, -8
  c->lw(t4, 0, a0);                                 // lw t4, 0(a0)
  c->lh(t0, 6, a0);                                 // lh t0, 6(a0)
  c->lhu(t3, 4, a0);                                // lhu t3, 4(a0)
  bc = ((s64)c->sgpr64(t0)) < 0;                    // bltz t0, L45
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_36;}                          // branch non-likely


block_3:
  bc = c->sgpr64(t0) == 0;                          // beq t0, r0, L34
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_7;}                           // branch non-likely

  c->lqc2(vf5, 12, t4);                             // lqc2 vf5, 12(t4)
  c->daddiu(a3, a3, 1);                             // daddiu a3, a3, 1
  c->lqc2(vf6, 44, t4);                             // lqc2 vf6, 44(t4)
  c->daddiu(t5, t3, -4);                            // daddiu t5, t3, -4
  c->lqc2(vf7, 76, t4);                             // lqc2 vf7, 76(t4)
  bc = ((s64)c->sgpr64(t5)) < 0;                    // bltz t5, L33
  c->lqc2(vf8, 108, t4);                            // lqc2 vf8, 108(t4)
  if (bc) {goto block_6;}                           // branch non-likely

  // Unknown instr: pref r0, 140, t4

block_6:
  //beq r0, r0, L35                                 // beq r0, r0, L35
  // nop                                            // sll r0, r0, 0
  goto block_9;                                     // branch always


block_7:
  c->lqc2(vf5, 12, t4);                             // lqc2 vf5, 12(t4)
  c->daddiu(a1, a1, 1);                             // daddiu a1, a1, 1
  c->lqc2(vf6, 76, t4);                             // lqc2 vf6, 76(t4)
  c->daddiu(t5, t3, -4);                            // daddiu t5, t3, -4
  c->lqc2(vf7, 140, t4);                            // lqc2 vf7, 140(t4)
  bc = ((s64)c->sgpr64(t5)) < 0;                    // bltz t5, L35
  c->lqc2(vf8, 204, t4);                            // lqc2 vf8, 204(t4)
  if (bc) {goto block_9;}                           // branch non-likely

  // Unknown instr: pref r0, 268, t4
  // Unknown instr: pref r0, 396, t4

block_9:
  // Unknown instr: vcallms 0
  // fmt::print("vcallms 0\n");
  //  nop                        |  mulaw.xyzw ACC, vf04, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf4, vf0);
  //  nop                        |  maddax.xyzw ACC, vf01, vf05
  c->vmadda_bc(DEST::xyzw, BC::x, vf1, vf5);
  //  nop                        |  madday.xyzw ACC, vf02, vf05
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf5);
  //  nop                        |  maddz.xyz vf05, vf03, vf05
  c->vmadd_bc(DEST::xyz, BC::z, vf5, vf3, vf5);
  //  nop                        |  mulaw.xyzw ACC, vf04, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf4, vf0);
  //  nop                        |  maddax.xyzw ACC, vf01, vf06
  c->vmadda_bc(DEST::xyzw, BC::x, vf1, vf6);
  //  nop                        |  madday.xyzw ACC, vf02, vf06
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf6);
  //  nop                        |  maddz.xyz vf06, vf03, vf06
  c->vmadd_bc(DEST::xyz, BC::z, vf6, vf3, vf6);
  //  nop                        |  mulaw.xyzw ACC, vf04, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf4, vf0);
  //  nop                        |  maddax.xyzw ACC, vf01, vf07
  c->vmadda_bc(DEST::xyzw, BC::x, vf1, vf7);
  //  nop                        |  madday.xyzw ACC, vf02, vf07
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf7);
  //  nop                        |  maddz.xyz vf07, vf03, vf07
  c->vmadd_bc(DEST::xyz, BC::z, vf7, vf3, vf7);
  //  nop                        |  mulaw.xyzw ACC, vf04, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf4, vf0);
  //  nop                        |  maddax.xyzw ACC, vf01, vf08
  c->vmadda_bc(DEST::xyzw, BC::x, vf1, vf8);
  //  nop                        |  madday.xyzw ACC, vf02, vf08
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf8);
  //  nop                        |  maddz.xyz vf08, vf03, vf08
  c->vmadd_bc(DEST::xyz, BC::z, vf8, vf3, vf8);
  //  nop                        |  subw.xyz vf09, vf05, vf05
  c->vsub_bc(DEST::xyz, BC::w, vf9, vf5, vf5);
  //  nop                        |  subw.xyz vf11, vf06, vf06
  c->vsub_bc(DEST::xyz, BC::w, vf11, vf6, vf6);
  //  nop                        |  subw.xyz vf13, vf07, vf07
  c->vsub_bc(DEST::xyz, BC::w, vf13, vf7, vf7);
  //  nop                        |  subw.xyz vf15, vf08, vf08
  c->vsub_bc(DEST::xyz, BC::w, vf15, vf8, vf8);
  //  nop                        |  addw.xyz vf10, vf05, vf05
  c->vadd_bc(DEST::xyz, BC::w, vf10, vf5, vf5);
  //  nop                        |  addw.xyz vf12, vf06, vf06
  c->vadd_bc(DEST::xyz, BC::w, vf12, vf6, vf6);
  //  nop                        |  addw.xyz vf14, vf07, vf07
  c->vadd_bc(DEST::xyz, BC::w, vf14, vf7, vf7);
  //  nop                        |  addw.xyz vf16, vf08, vf08
  c->vadd_bc(DEST::xyz, BC::w, vf16, vf8, vf8);
  //  nop                        |  ftoi0.xyzw vf09, vf09
  c->vftoi0(DEST::xyzw, vf9, vf9);
  //  nop                        |  ftoi0.xyzw vf11, vf11
  c->vftoi0(DEST::xyzw, vf11, vf11);
  //  nop                        |  ftoi0.xyzw vf13, vf13
  c->vftoi0(DEST::xyzw, vf13, vf13);
  //  nop                        |  ftoi0.xyzw vf15, vf15
  c->vftoi0(DEST::xyzw, vf15, vf15);
  //  nop                        |  ftoi0.xyzw vf10, vf10
  c->vftoi0(DEST::xyzw, vf10, vf10);
  //  nop                        |  ftoi0.xyzw vf12, vf12
  c->vftoi0(DEST::xyzw, vf12, vf12);
  //  nop                        |  ftoi0.xyzw vf14, vf14 :e
  c->vftoi0(DEST::xyzw, vf14, vf14);
  //  nop                        |  ftoi0.xyzw vf16, vf16
  c->vftoi0(DEST::xyzw, vf16, vf16);
  c->mov128_gpr_vf(s5, vf9);                        // qmfc2.i s5, vf9
  c->mov128_gpr_vf(t7, vf10);                       // qmfc2.i t7, vf10
  c->mov128_gpr_vf(gp, vf11);                       // qmfc2.i gp, vf11
  c->mov128_gpr_vf(ra, vf12);                       // qmfc2.i ra, vf12
  c->mov128_gpr_vf(t9, vf13);                       // qmfc2.i t9, vf13
  c->mov128_gpr_vf(t8, vf14);                       // qmfc2.i t8, vf14
  c->mov128_gpr_vf(t6, vf15);                       // qmfc2.i t6, vf15
  c->mov128_gpr_vf(t5, vf16);                       // qmfc2.i t5, vf16
  // nop                                            // sll r0, r0, 0
  c->pcgtw(s5, s5, t2);                             // pcgtw s5, s5, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(t7, t1, t7);                             // pcgtw t7, t1, t7
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(t7, s5, t7);                               // por t7, s5, t7
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(t7, r0, t7);                             // ppach t7, r0, t7
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(gp, gp, t2);                             // pcgtw gp, gp, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(ra, t1, ra);                             // pcgtw ra, t1, ra
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(ra, gp, ra);                               // por ra, gp, ra
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(ra, r0, ra);                             // ppach ra, r0, ra
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(t9, t9, t2);                             // pcgtw t9, t9, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(t8, t1, t8);                             // pcgtw t8, t1, t8
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(t8, t9, t8);                               // por t8, t9, t8
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(t8, r0, t8);                             // ppach t8, r0, t8
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(t6, t6, t2);                             // pcgtw t6, t6, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(t5, t1, t5);                             // pcgtw t5, t1, t5
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(t5, t6, t5);                               // por t5, t6, t5
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(t5, r0, t5);                             // ppach t5, r0, t5
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->dsll(t9, t7, 16);                              // dsll t9, t7, 16
  c->dsll(t7, ra, 16);                              // dsll t7, ra, 16
  c->dsll(t6, t8, 16);                              // dsll t6, t8, 16
  c->dsll(t5, t5, 16);                              // dsll t5, t5, 16
  bc = c->sgpr64(t0) == 0;                          // beq t0, r0, L40
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_23;}                          // branch non-likely

  bc = c->sgpr64(t9) != 0;                          // bne t9, r0, L36
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_12;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lw(t8, 4, t4);                                 // lw t8, 4(t4)
  // nop                                            // sll r0, r0, 0
  c->lbu(t9, 2, t4);                                // lbu t9, 2(t4)
  // nop                                            // sll r0, r0, 0
  c->lbu(ra, 3, t4);                                // lbu ra, 3(t4)
  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t8, -8, a0);                                // sw t8, -8(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t9, -4, a0);                                // sh t9, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(ra, -2, a0);                                // sh ra, -2(a0)

block_12:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L39
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_21;}                          // branch non-likely

  bc = c->sgpr64(t7) != 0;                          // bne t7, r0, L37
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_15;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lw(t7, 36, t4);                                // lw t7, 36(t4)
  // nop                                            // sll r0, r0, 0
  c->lbu(t8, 34, t4);                               // lbu t8, 34(t4)
  // nop                                            // sll r0, r0, 0
  c->lbu(t9, 35, t4);                               // lbu t9, 35(t4)
  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t7, -8, a0);                                // sw t7, -8(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t8, -4, a0);                                // sh t8, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t9, -2, a0);                                // sh t9, -2(a0)

block_15:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L39
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_21;}                          // branch non-likely

  bc = c->sgpr64(t6) != 0;                          // bne t6, r0, L38
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_18;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lw(t6, 68, t4);                                // lw t6, 68(t4)
  // nop                                            // sll r0, r0, 0
  c->lbu(t7, 66, t4);                               // lbu t7, 66(t4)
  // nop                                            // sll r0, r0, 0
  c->lbu(t8, 67, t4);                               // lbu t8, 67(t4)
  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t6, -8, a0);                                // sw t6, -8(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t7, -4, a0);                                // sh t7, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t8, -2, a0);                                // sh t8, -2(a0)

block_18:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L39
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_21;}                          // branch non-likely

  bc = c->sgpr64(t5) != 0;                          // bne t5, r0, L39
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_21;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lw(t5, 100, t4);                               // lw t5, 100(t4)
  // nop                                            // sll r0, r0, 0
  c->lbu(t6, 98, t4);                               // lbu t6, 98(t4)
  // nop                                            // sll r0, r0, 0
  c->lbu(t7, 99, t4);                               // lbu t7, 99(t4)
  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t5, -8, a0);                                // sw t5, -8(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t6, -4, a0);                                // sh t6, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t7, -2, a0);                                // sh t7, -2(a0)

block_21:
  bc = ((s64)c->sgpr64(t3)) > 0;                    // bgtz t3, L32
  c->daddiu(t4, t4, 128);                           // daddiu t4, t4, 128
  if (bc) {goto block_3;}                           // branch non-likely

  //beq r0, r0, L31                                 // beq r0, r0, L31
  // nop                                            // sll r0, r0, 0
  goto block_1;                                     // branch always


block_23:
  bc = c->sgpr64(t9) != 0;                          // bne t9, r0, L41
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_25;}                          // branch non-likely

  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t4, -8, a0);                                // sw t4, -8(a0)
  c->daddiu(t8, r0, -1);                            // daddiu t8, r0, -1
  c->sh(r0, -4, a0);                                // sh r0, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t8, -2, a0);                                // sh t8, -2(a0)

block_25:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L44
  c->daddiu(t4, t4, 64);                            // daddiu t4, t4, 64
  if (bc) {goto block_34;}                          // branch non-likely

  bc = c->sgpr64(t7) != 0;                          // bne t7, r0, L42
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_28;}                          // branch non-likely

  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t4, -8, a0);                                // sw t4, -8(a0)
  c->daddiu(t7, r0, -1);                            // daddiu t7, r0, -1
  c->sh(r0, -4, a0);                                // sh r0, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t7, -2, a0);                                // sh t7, -2(a0)

block_28:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L44
  c->daddiu(t4, t4, 64);                            // daddiu t4, t4, 64
  if (bc) {goto block_34;}                          // branch non-likely

  bc = c->sgpr64(t6) != 0;                          // bne t6, r0, L43
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_31;}                          // branch non-likely

  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t4, -8, a0);                                // sw t4, -8(a0)
  c->daddiu(t6, r0, -1);                            // daddiu t6, r0, -1
  c->sh(r0, -4, a0);                                // sh r0, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t6, -2, a0);                                // sh t6, -2(a0)

block_31:
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L44
  c->daddiu(t4, t4, 64);                            // daddiu t4, t4, 64
  if (bc) {goto block_34;}                          // branch non-likely

  bc = c->sgpr64(t5) != 0;                          // bne t5, r0, L44
  c->daddiu(t3, t3, -1);                            // daddiu t3, t3, -1
  if (bc) {goto block_34;}                          // branch non-likely

  c->daddiu(a0, a0, 8);                             // daddiu a0, a0, 8
  c->sw(t4, -8, a0);                                // sw t4, -8(a0)
  c->daddiu(t5, r0, -1);                            // daddiu t5, r0, -1
  c->sh(r0, -4, a0);                                // sh r0, -4(a0)
  // nop                                            // sll r0, r0, 0
  c->sh(t5, -2, a0);                                // sh t5, -2(a0)

block_34:
  bc = ((s64)c->sgpr64(t3)) > 0;                    // bgtz t3, L32
  c->daddiu(t4, t4, 64);                            // daddiu t4, t4, 64
  if (bc) {goto block_3;}                           // branch non-likely

  //beq r0, r0, L31                                 // beq r0, r0, L31
  // nop                                            // sll r0, r0, 0
  goto block_1;                                     // branch always


block_36:
  c->mov64(t0, t4);                                 // or t0, t4, r0
  c->lhu(t4, 42, t4);                               // lhu t4, 42(t4)
  // nop                                            // sll r0, r0, 0
  c->lw(t3, 8, t0);                                 // lw t3, 8(t0)
  bc = c->sgpr64(t4) != 0;                          // bne t4, r0, L54
  c->lhu(t5, 34, t0);                               // lhu t5, 34(t0)
  if (bc) {goto block_58;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lw(t3, 172, t3);                               // lw t3, 172(t3)
  // nop                                            // sll r0, r0, 0
  c->lq(t4, 44, t0);                                // lq t4, 44(t0)
  bc = c->sgpr64(t3) == 0;                          // beq t3, r0, L54
  c->lq(t6, 28, t0);                                // lq t6, 28(t0)
  if (bc) {goto block_58;}                          // branch non-likely

  c->pextlw(t5, t5, t5);                            // pextlw t5, t5, t5
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcpyld(t5, t5, t5);                            // pcpyld t5, t5, t5
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcpyud(t8, t4, r0);                            // pcpyud t8, t4, r0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcpyud(t7, t6, r0);                            // pcpyud t7, t6, r0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pextlh(t8, t8, r0);                            // pextlh t8, t8, r0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->psraw(t8, t8, 10);                             // psraw t8, t8, 10
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pextlh(t9, t6, r0);                            // pextlh t9, t6, r0
  c->lhu(t6, 0, t3);                                // lhu t6, 0(t3)
  c->psraw(t9, t9, 16);                             // psraw t9, t9, 16
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pextlh(t7, t7, r0);                            // pextlh t7, t7, r0
  c->mov128_vf_gpr(vf20, t8);                       // qmtc2.i vf20, t8
  c->psraw(t7, t7, 16);                             // psraw t7, t7, 16
  c->mov128_vf_gpr(vf17, t9);                       // qmtc2.i vf17, t9
  c->pextlh(t4, t4, r0);                            // pextlh t4, t4, r0
  c->mov128_vf_gpr(vf18, t7);                       // qmtc2.i vf18, t7
  c->psraw(t4, t4, 16);                             // psraw t4, t4, 16
  c->lqc2(vf23, 12, t0);                            // lqc2 vf23, 12(t0)
  c->mov128_vf_gpr(vf19, t4);                       // qmtc2.i vf19, t4
  c->lw(t4, 0, a2);                                 // lw t4, 0(a2)
  c->mov128_vf_gpr(vf21, t5);                       // qmtc2.i vf21, t5
  c->daddiu(gp, t6, -4);                            // daddiu gp, t6, -4
  // Unknown instr: vcallms 32
  //  nop                        |  itof0.xyzw vf16, vf16
  c->vftoi0(DEST::xyzw, vf16, vf16);
  //  nop                        |  itof0.xyzw vf20, vf20
  c->vitof0(DEST::xyzw, vf20, vf20);
  //  nop                        |  itof12.xyzw vf17, vf17
  c->vitof12(DEST::xyzw, vf17, vf17);
  //  nop                        |  itof12.xyzw vf18, vf18
  c->vitof12(DEST::xyzw, vf18, vf18);
  //  nop                        |  itof12.xyzw vf19, vf19
  c->vitof12(DEST::xyzw, vf19, vf19);
  //  nop                        |  add.xyz vf20, vf20, vf23
  c->vadd(DEST::xyz, vf20, vf20, vf23);
  //  nop                        |  itof12.xyzw vf21, vf21
  c->vitof12(DEST::xyzw, vf21, vf21);
  //  nop                        |  mulax.xyzw ACC, vf01, vf20
  c->vmula_bc(DEST::xyzw, BC::x, vf1, vf20);
  //  nop                        |  madday.xyzw ACC, vf02, vf20
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf20);
  //  nop                        |  maddaz.xyzw ACC, vf03, vf20
  c->vmadda_bc(DEST::xyzw, BC::z, vf3, vf20);
  //  nop                        |  maddw.xyz vf20, vf04, vf00
  c->vmadd_bc(DEST::xyz, BC::w, vf20, vf4, vf0);
  //  nop                        |  mulax.xyzw ACC, vf01, vf17
  c->vmula_bc(DEST::xyzw, BC::x, vf1, vf17);
  //  nop                        |  madday.xyzw ACC, vf02, vf17
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf17);
  //  nop                        |  maddaz.xyzw ACC, vf03, vf17
  c->vmadda_bc(DEST::xyzw, BC::z, vf3, vf17);
  //  nop                        |  maddx.xyz vf17, vf04, vf00
  c->vmadd_bc(DEST::xyz, BC::x, vf17, vf4, vf0);
  //  nop                        |  mulax.xyzw ACC, vf01, vf18
  c->vmula_bc(DEST::xyzw, BC::x, vf1, vf18);
  //  nop                        |  madday.xyzw ACC, vf02, vf18
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf18);
  //  nop                        |  maddaz.xyzw ACC, vf03, vf18
  c->vmadda_bc(DEST::xyzw, BC::z, vf3, vf18);
  //  nop                        |  maddx.xyz vf18, vf04, vf00
  c->vmadd_bc(DEST::xyz, BC::x, vf18, vf4, vf0);
  //  nop                        |  mulax.xyzw ACC, vf01, vf19
  c->vmula_bc(DEST::xyzw, BC::x, vf1, vf19);
  //  nop                        |  madday.xyzw ACC, vf02, vf19
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf19);
  //  nop                        |  maddaz.xyzw ACC, vf03, vf19 :e
  c->vmadda_bc(DEST::xyzw, BC::z, vf3, vf19);
  //  nop                        |  maddx.xyz vf19, vf04, vf00
  c->vmadd_bc(DEST::xyz, BC::x, vf19, vf4, vf0);
  c->sll(t5, t4, 3);                                // sll t5, t4, 3
  c->addu(t5, t5, a2);                              // addu t5, t5, a2
  c->addu(t6, t6, r0);                              // addu t6, t6, r0
  c->addiu(t5, t5, 16);                             // addiu t5, t5, 16
  c->lw(t7, 12, t3);                                // lw t7, 12(t3)
  // nop                                            // sll r0, r0, 0
  c->lw(ra, 16, t3);                                // lw ra, 16(t3)
  // nop                                            // sll r0, r0, 0
  c->lw(t8, 20, t3);                                // lw t8, 20(t3)
  // nop                                            // sll r0, r0, 0
  c->lw(t9, 24, t3);                                // lw t9, 24(t3)
  bc = ((s64)c->sgpr64(gp)) < 0;                    // bltz gp, L52
  c->lq(gp, 12, t7);                                // lq gp, 12(t7)
  if (bc) {goto block_54;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lq(s5, 12, ra);                                // lq s5, 12(ra)
  // nop                                            // sll r0, r0, 0
  c->lq(s4, 12, t8);                                // lq s4, 12(t8)
  // nop                                            // sll r0, r0, 0
  c->lq(s3, 12, t9);                                // lq s3, 12(t9)
  // nop                                            // sll r0, r0, 0
  c->mov128_vf_gpr(vf5, gp);                        // qmtc2.ni vf5, gp
  // nop                                            // sll r0, r0, 0
  c->mov128_vf_gpr(vf6, s5);                        // qmtc2.ni vf6, s5
  // nop                                            // sll r0, r0, 0
  c->mov128_vf_gpr(vf7, s4);                        // qmtc2.ni vf7, s4
  // nop                                            // sll r0, r0, 0
  c->mov128_vf_gpr(vf8, s3);                        // qmtc2.ni vf8, s3

block_40:
  // Unknown instr: vcallms 54
  // fmt::print("vcallms54\n");
  //  nop                        |  mulaw.xyzw ACC, vf20, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf20, vf0);
  //  nop                        |  maddax.xyzw ACC, vf17, vf05
  c->vmadda_bc(DEST::xyzw, BC::x, vf17, vf5);
  //  nop                        |  madday.xyzw ACC, vf18, vf05
  c->vmadda_bc(DEST::xyzw, BC::y, vf18, vf5);
  //  nop                        |  maddz.xyzw vf09, vf19, vf05
  c->vmadd_bc(DEST::xyzw, BC::z, vf9, vf19, vf5);
  //  nop                        |  mulaw.xyzw ACC, vf20, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf20, vf0);
  //  nop                        |  maddax.xyzw ACC, vf17, vf06
  c->vmadda_bc(DEST::xyzw, BC::x, vf17, vf6);
  //  nop                        |  madday.xyzw ACC, vf18, vf06
  c->vmadda_bc(DEST::xyzw, BC::y, vf18, vf6);
  //  nop                        |  maddz.xyzw vf11, vf19, vf06
  c->vmadd_bc(DEST::xyzw, BC::z, vf11, vf19, vf6);
  //  nop                        |  mulaw.xyzw ACC, vf20, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf20, vf0);
  //  nop                        |  maddax.xyzw ACC, vf17, vf07
  c->vmadda_bc(DEST::xyzw, BC::x, vf17, vf7);
  //  nop                        |  madday.xyzw ACC, vf18, vf07
  c->vmadda_bc(DEST::xyzw, BC::y, vf18, vf7);
  //  nop                        |  maddz.xyzw vf13, vf19, vf07
  c->vmadd_bc(DEST::xyzw, BC::z, vf13, vf19, vf7);
  //  nop                        |  mulaw.xyzw ACC, vf20, vf00
  c->vmula_bc(DEST::xyzw, BC::w, vf20, vf0);
  //  nop                        |  maddax.xyzw ACC, vf17, vf08
  c->vmadda_bc(DEST::xyzw, BC::x, vf17, vf8);
  //  nop                        |  madday.xyzw ACC, vf18, vf08
  c->vmadda_bc(DEST::xyzw, BC::y, vf18, vf8);
  //  nop                        |  maddz.xyzw vf15, vf19, vf08
  c->vmadd_bc(DEST::xyzw, BC::z, vf15, vf19, vf8);
  //  nop                        |  mulw.x vf22, vf21, vf05
  c->vmul_bc(DEST::x, BC::w, vf22, vf21, vf5);
  //  nop                        |  mulw.y vf22, vf21, vf06
  c->vmul_bc(DEST::y, BC::w, vf22, vf21, vf6);
  //  nop                        |  mulw.z vf22, vf21, vf07
  c->vmul_bc(DEST::z, BC::w, vf22, vf21, vf7);
  //  nop                        |  mulw.w vf22, vf21, vf08
  c->vmul_bc(DEST::w, BC::w, vf22, vf21, vf8);
  //  nop                        |  addx.xyz vf10, vf09, vf22
  c->vadd_bc(DEST::xyz, BC::x, vf10, vf9, vf22);
  //  nop                        |  subx.xyz vf09, vf09, vf22
  c->vsub_bc(DEST::xyz, BC::x, vf9, vf9, vf22);
  //  nop                        |  addy.xyz vf12, vf11, vf22
  c->vadd_bc(DEST::xyz, BC::y, vf12, vf11, vf22);
  //  nop                        |  suby.xyz vf11, vf11, vf22
  c->vsub_bc(DEST::xyz, BC::y, vf11, vf11, vf22);
  //  nop                        |  addz.xyz vf14, vf13, vf22
  c->vadd_bc(DEST::xyz, BC::z, vf14, vf13, vf22);
  //  nop                        |  subz.xyz vf13, vf13, vf22
  c->vsub_bc(DEST::xyz, BC::z, vf13, vf13, vf22);
  //  nop                        |  addw.xyz vf16, vf15, vf22
  c->vadd_bc(DEST::xyz, BC::w, vf16, vf15, vf22);
  //  nop                        |  subw.xyz vf15, vf15, vf22
  c->vsub_bc(DEST::xyz, BC::w, vf15, vf15, vf22);
  //  nop                        |  ftoi0.xyzw vf10, vf10
  c->vftoi0(DEST::xyzw, vf10, vf10);
  //  nop                        |  ftoi0.xyzw vf09, vf09
  c->vftoi0(DEST::xyzw, vf9, vf9);
  //  nop                        |  ftoi0.xyzw vf12, vf12
  c->vftoi0(DEST::xyzw, vf12, vf12);
  //  nop                        |  ftoi0.xyzw vf11, vf11
  c->vftoi0(DEST::xyzw, vf11, vf11);
  //  nop                        |  ftoi0.xyzw vf14, vf14
  c->vftoi0(DEST::xyzw, vf14, vf14);
  //  nop                        |  ftoi0.xyzw vf13, vf13
  c->vftoi0(DEST::xyzw, vf13, vf13);
  //  nop                        |  ftoi0.xyzw vf16, vf16 :e
  c->vftoi0(DEST::xyzw, vf16, vf16);
  //  nop                        |  ftoi0.xyzw vf15, vf15
  c->vftoi0(DEST::xyzw, vf15, vf15);

  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // vnop
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(v0, vf10);                       // qmfc2.i v0, vf10
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(s4, vf9);                        // qmfc2.i s4, vf9
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(s0, vf12);                       // qmfc2.i s0, vf12
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(s1, vf11);                       // qmfc2.i s1, vf11
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(s2, vf14);                       // qmfc2.i s2, vf14
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(s3, vf13);                       // qmfc2.i s3, vf13
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(s5, vf16);                       // qmfc2.i s5, vf16
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(gp, vf15);                       // qmfc2.i gp, vf15
  // nop                                            // sll r0, r0, 0
  c->pcgtw(v0, t1, v0);                             // pcgtw v0, t1, v0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(s4, s4, t2);                             // pcgtw s4, s4, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(s4, s4, v0);                               // por s4, s4, v0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(s4, r0, s4);                             // ppach s4, r0, s4
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(s0, t1, s0);                             // pcgtw s0, t1, s0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(s1, s1, t2);                             // pcgtw s1, s1, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(s1, s1, s0);                               // por s1, s1, s0
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(s1, r0, s1);                             // ppach s1, r0, s1
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(s2, t1, s2);                             // pcgtw s2, t1, s2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(s3, s3, t2);                             // pcgtw s3, s3, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(s3, s3, s2);                               // por s3, s3, s2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(s3, r0, s3);                             // ppach s3, r0, s3
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(s5, t1, s5);                             // pcgtw s5, t1, s5
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(gp, gp, t2);                             // pcgtw gp, gp, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(gp, gp, s5);                               // por gp, gp, s5
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(gp, r0, gp);                             // ppach gp, r0, gp
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->dsll(s5, s4, 16);                              // dsll s5, s4, 16
  c->dsll(s4, s1, 16);                              // dsll s4, s1, 16
  c->dsll(s3, s3, 16);                              // dsll s3, s3, 16
  c->dsll(gp, gp, 16);                              // dsll gp, gp, 16
  bc = c->sgpr64(s5) != 0;                          // bne s5, r0, L47
  c->daddiu(t6, t6, -1);                            // daddiu t6, t6, -1
  if (bc) {goto block_42;}                          // branch non-likely

  c->daddiu(t4, t4, 1);                             // daddiu t4, t4, 1
  c->sw(t0, 4, t5);                                 // sw t0, 4(t5)
  c->daddiu(t5, t5, 8);                             // daddiu t5, t5, 8
  c->sw(t7, -8, t5);                                // sw t7, -8(t5)

block_42:
  bc = c->sgpr64(t6) == 0;                          // beq t6, r0, L51
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_53;}                          // branch non-likely

  bc = c->sgpr64(s4) != 0;                          // bne s4, r0, L48
  c->daddiu(t6, t6, -1);                            // daddiu t6, t6, -1
  if (bc) {goto block_45;}                          // branch non-likely

  c->daddiu(t4, t4, 1);                             // daddiu t4, t4, 1
  c->sw(t0, 4, t5);                                 // sw t0, 4(t5)
  c->daddiu(t5, t5, 8);                             // daddiu t5, t5, 8
  c->sw(ra, -8, t5);                                // sw ra, -8(t5)

block_45:
  bc = c->sgpr64(t6) == 0;                          // beq t6, r0, L51
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_53;}                          // branch non-likely

  bc = c->sgpr64(s3) != 0;                          // bne s3, r0, L49
  c->daddiu(t6, t6, -1);                            // daddiu t6, t6, -1
  if (bc) {goto block_48;}                          // branch non-likely

  c->daddiu(t4, t4, 1);                             // daddiu t4, t4, 1
  c->sw(t0, 4, t5);                                 // sw t0, 4(t5)
  c->daddiu(t5, t5, 8);                             // daddiu t5, t5, 8
  c->sw(t8, -8, t5);                                // sw t8, -8(t5)

block_48:
  bc = c->sgpr64(t6) == 0;                          // beq t6, r0, L51
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_53;}                          // branch non-likely

  bc = c->sgpr64(gp) != 0;                          // bne gp, r0, L50
  c->daddiu(t6, t6, -1);                            // daddiu t6, t6, -1
  if (bc) {goto block_51;}                          // branch non-likely

  c->daddiu(t4, t4, 1);                             // daddiu t4, t4, 1
  c->sw(t0, 4, t5);                                 // sw t0, 4(t5)
  c->daddiu(t5, t5, 8);                             // daddiu t5, t5, 8
  c->sw(t9, -8, t5);                                // sw t9, -8(t5)

block_51:
  bc = c->sgpr64(t6) == 0;                          // beq t6, r0, L51
  c->daddiu(t3, t3, 16);                            // daddiu t3, t3, 16
  if (bc) {goto block_53;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->lw(t7, 12, t3);                                // lw t7, 12(t3)
  // nop                                            // sll r0, r0, 0
  c->lw(ra, 16, t3);                                // lw ra, 16(t3)
  // nop                                            // sll r0, r0, 0
  c->lw(t8, 20, t3);                                // lw t8, 20(t3)
  // nop                                            // sll r0, r0, 0
  c->lw(t9, 24, t3);                                // lw t9, 24(t3)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf5, 12, t7);                             // lqc2 vf5, 12(t7)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf6, 12, ra);                             // lqc2 vf6, 12(ra)
  // nop                                            // sll r0, r0, 0
  c->lqc2(vf7, 12, t8);                             // lqc2 vf7, 12(t8)
  //beq r0, r0, L46                                 // beq r0, r0, L46
  c->lqc2(vf8, 12, t9);                             // lqc2 vf8, 12(t9)
  goto block_40;                                    // branch always


block_53:
  // nop                                            // sll r0, r0, 0
  c->sw(t4, 0, a2);                                 // sw t4, 0(a2)
  //beq r0, r0, L31                                 // beq r0, r0, L31
  // nop                                            // sll r0, r0, 0
  goto block_1;                                     // branch always


block_54:
  c->mov128_vf_gpr(vf5, gp);                        // qmtc2.i vf5, gp
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf20, vf0);        // vmulaw.xyzw acc, vf20, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf17, vf5);       // vmaddax.xyzw acc, vf17, vf5
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf18, vf5);       // vmadday.xyzw acc, vf18, vf5
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyzw, BC::z, vf24, vf19, vf5);  // vmaddz.xyzw vf24, vf19, vf5
  // nop                                            // sll r0, r0, 0
  c->vmul_bc(DEST::x, BC::w, vf22, vf21, vf5);      // vmulw.x vf22, vf21, vf5
  // nop                                            // sll r0, r0, 0
  c->vadd_bc(DEST::xyz, BC::x, vf25, vf24, vf22);   // vaddx.xyz vf25, vf24, vf22
  // nop                                            // sll r0, r0, 0
  c->vsub_bc(DEST::xyz, BC::x, vf24, vf24, vf22);   // vsubx.xyz vf24, vf24, vf22
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::xyzw, vf25, vf25);                // vftoi0.xyzw vf25, vf25
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::xyzw, vf24, vf24);                // vftoi0.xyzw vf24, vf24
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(t9, vf25);                       // qmfc2.i t9, vf25
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(t8, vf24);                       // qmfc2.i t8, vf24
  // nop                                            // sll r0, r0, 0
  c->pcgtw(t9, t1, t9);                             // pcgtw t9, t1, t9
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->pcgtw(t8, t8, t2);                             // pcgtw t8, t8, t2
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->por(t8, t8, t9);                               // por t8, t8, t9
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->ppach(t8, r0, t8);                             // ppach t8, r0, t8
  c->mfc1(r0, f31);                                 // mfc1 r0, f31
  c->dsll(t8, t8, 16);                              // dsll t8, t8, 16
  c->daddiu(t3, t3, 4);                             // daddiu t3, t3, 4
  bc = c->sgpr64(t8) != 0;                          // bne t8, r0, L53
  c->daddiu(t6, t6, -1);                            // daddiu t6, t6, -1
  if (bc) {goto block_56;}                          // branch non-likely

  c->daddiu(t4, t4, 1);                             // daddiu t4, t4, 1
  c->sw(t0, 4, t5);                                 // sw t0, 4(t5)
  c->daddiu(t5, t5, 8);                             // daddiu t5, t5, 8
  c->sw(t7, -8, t5);                                // sw t7, -8(t5)

block_56:
  // nop                                            // sll r0, r0, 0
  c->lw(t7, 12, t3);                                // lw t7, 12(t3)
  bc = ((s64)c->sgpr64(t6)) > 0;                    // bgtz t6, L52
  c->lq(gp, 12, t7);                                // lq gp, 12(t7)
  if (bc) {goto block_54;}                          // branch non-likely

  // nop                                            // sll r0, r0, 0
  c->sw(t4, 0, a2);                                 // sw t4, 0(a2)

block_58:
  //beq r0, r0, L31                                 // beq r0, r0, L31
  // nop                                            // sll r0, r0, 0
  goto block_1;                                     // branch always


block_59:
  c->gprs[v0].du64[0] = 0;                          // or v0, r0, r0
  c->ld(ra, 0, sp);                                 // ld ra, 0(sp)
  c->lq(gp, 112, sp);                               // lq gp, 112(sp)
  c->lq(s5, 96, sp);                                // lq s5, 96(sp)
  c->lq(s4, 80, sp);                                // lq s4, 80(sp)
  c->lq(s3, 64, sp);                                // lq s3, 64(sp)
  c->lq(s2, 48, sp);                                // lq s2, 48(sp)
  c->lq(s1, 32, sp);                                // lq s1, 32(sp)
  c->lq(s0, 16, sp);                                // lq s0, 16(sp)
  //jr ra                                           // jr ra
  c->daddiu(sp, sp, 128);                           // daddiu sp, sp, 128
  goto end_of_function;                             // return

  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  end_of_function:
  return c->gprs[v0].du64[0];
}

void link() {
  cache.collide_probe_stack = intern_from_c(-1, 0, "*collide-probe-stack*").c();
  gLinkedFunctionTable.reg("collide-probe-instance-tie", execute, 512);
}

} // namespace collide_probe_instance_tie
} // namespace Mips2C
// add collide_probe_instance_tie::link to the link callback table for the object file.
// FWD DEC:
namespace collide_probe_instance_tie { extern void link(); }
