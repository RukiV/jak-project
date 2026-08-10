
//--------------------------MIPS2C---------------------
// clang-format off
#include "game/mips2c/mips2c_private.h"
#include "game/kernel/jakx/kscheme.h"
using ::jakx::intern_from_c;
namespace Mips2C::jakx {
namespace method_9_collide_cache_prim {
struct Cache {
  void* moving_sphere_triangle_intersect; // moving-sphere-triangle-intersect
} cache;

u64 execute(void* ctxt) {
  auto* c = (ExecutionContext*)ctxt;
  bool bc = false;
  u32 call_addr = 0;
  bool cop1_bc = false;
  c->daddiu(sp, sp, -672);                          // daddiu sp, sp, -672
  c->sd(ra, 0, sp);                                 // sd ra, 0(sp)
  c->sq(s1, 576, sp);                               // sq s1, 576(sp)
  c->sq(s2, 592, sp);                               // sq s2, 592(sp)
  c->sq(s3, 608, sp);                               // sq s3, 608(sp)
  c->sq(s4, 624, sp);                               // sq s4, 624(sp)
  c->sq(s5, 640, sp);                               // sq s5, 640(sp)
  c->sq(gp, 656, sp);                               // sq gp, 656(sp)
  c->mov64(gp, a1);                                 // or gp, a1, r0
  c->mov64(s5, a2);                                 // or s5, a2, r0
  c->mov64(s4, a3);                                 // or s4, a3, r0
  c->daddiu(s3, sp, 16);                            // daddiu s3, sp, 16
  // nop                                            // sll r0, r0, 0
  c->mtc1(f0, r0);                                  // mtc1 f0, r0
  c->sw(t1, 8, s3);                                 // sw t1, 8(s3)
  c->mtc1(f1, t0);                                  // mtc1 f1, t0
  c->lhu(v1, 40, a0);                               // lhu v1, 40(a0)
  // nop                                            // sll r0, r0, 0
  c->lwu(a1, 32, a0);                               // lwu a1, 32(a0)
  cop1_bc = c->fprs[f0] <= c->fprs[f1];             // c.le.s f0, f1
  c->dsll(v1, v1, 6);                               // dsll v1, v1, 6
  bc = cop1_bc;                                     // bc1t L37
  c->lw(a1, 124, a1);                               // lw a1, 124(a1)
  if (bc) {goto block_2;}                           // branch non-likely

  c->lui(a2, 16384);                                // lui a2, 16384
  c->mtc1(f1, a2);                                  // mtc1 f1, a2

block_2:
  c->daddu(s2, a1, v1);                             // daddu s2, a1, v1
  c->swc1(f1, 0, s3);                               // swc1 f1, 0(s3)
  // nop                                            // sll r0, r0, 0
  c->lhu(s1, 42, a0);                               // lhu s1, 42(a0)
  // nop                                            // sll r0, r0, 0
  c->swc1(f1, 4, s3);                               // swc1 f1, 4(s3)

block_3:
  bc = c->sgpr64(s1) == 0;                          // beq s1, r0, L40
  c->daddiu(s1, s1, -1);                            // daddiu s1, s1, -1
  if (bc) {goto block_14;}                          // branch non-likely

  c->load_symbol2(t9, cache.moving_sphere_triangle_intersect);// lw t9, moving-sphere-triangle-intersect(s7)
  c->mov64(a0, s5);                                 // or a0, s5, r0
  c->mov64(a1, s4);                                 // or a1, s4, r0
  c->lwc1(f0, 12, s5);                              // lwc1 f0, 12(s5)
  c->mfc1(a2, f0);                                  // mfc1 a2, f0
  c->daddu(a3, r0, s2);                             // daddu a3, r0, s2
  c->daddiu(t0, s3, 64);                            // daddiu t0, s3, 64
  c->daddiu(t1, s3, 80);                            // daddiu t1, s3, 80
  call_addr = c->gprs[t9].du32[0];                  // function call:
  c->sll(v0, ra, 0);                                // sll v0, ra, 0
  c->jalr(call_addr);                               // jalr ra, t9
  c->mov64(v1, v0);                                 // or v1, v0, r0
  // nop                                            // sll r0, r0, 0
  c->mtc1(f0, r0);                                  // mtc1 f0, r0
  c->lqc2(vf1, 0, s4);                              // lqc2 vf1, 0(s4)
  c->mtc1(f2, v1);                                  // mtc1 f2, v1
  c->lwc1(f1, 0, s3);                               // lwc1 f1, 0(s3)
  cop1_bc = c->fprs[f2] < c->fprs[f0];              // c.lt.s f2, f0
  c->lqc2(vf2, 80, s3);                             // lqc2 vf2, 80(s3)
  if (cop1_bc) {                                    // bc1tl L38
    c->daddiu(s2, s2, 64);                          // daddiu s2, s2, 64
    goto block_3;
  }

// block_6:
  cop1_bc = c->fprs[f1] <= c->fprs[f2];             // c.le.s f1, f2
  c->lqc2(vf3, 64, s3);                             // lqc2 vf3, 64(s3)
  if (cop1_bc) {                                    // bc1tl L38
    c->daddiu(s2, s2, 64);                          // daddiu s2, s2, 64
    goto block_3;
  }

// block_8:
  c->vmul(DEST::xyzw, vf5, vf1, vf2);               // vmul.xyzw vf5, vf1, vf2
  c->lqc2(vf4, 0, s5);                              // lqc2 vf4, 0(s5)
  // nop                                            // sll r0, r0, 0
  c->lwu(v1, 8, s3);                                // lwu v1, 8(s3)
  c->vsub(DEST::xyzw, vf7, vf4, vf3);               // vsub.xyzw vf7, vf4, vf3
  c->andi(v1, v1, 1);                               // andi v1, v1, 1
  bc = c->sgpr64(v1) == 0;                          // beq v1, r0, L39
  c->vadd_bc(DEST::x, BC::y, vf5, vf5, vf5);        // vaddy.x vf5, vf5, vf5
  if (bc) {goto block_13;}                          // branch non-likely

  c->vmul(DEST::xyzw, vf6, vf7, vf2);               // vmul.xyzw vf6, vf7, vf2
  // nop                                            // sll r0, r0, 0
  c->vadd_bc(DEST::x, BC::z, vf5, vf5, vf5);        // vaddz.x vf5, vf5, vf5
  // nop                                            // sll r0, r0, 0
  c->vadd_bc(DEST::x, BC::y, vf6, vf6, vf6);        // vaddy.x vf6, vf6, vf6
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(v1, vf5);                        // qmfc2.i v1, vf5
  // nop                                            // sll r0, r0, 0
  c->mtc1(f3, v1);                                  // mtc1 f3, v1
  // nop                                            // sll r0, r0, 0
  cop1_bc = c->fprs[f0] <= c->fprs[f3];             // c.le.s f0, f3
  // nop                                            // sll r0, r0, 0
  if (cop1_bc) {                                    // bc1tl L38
    c->daddiu(s2, s2, 64);                          // daddiu s2, s2, 64
    goto block_3;
  }

// block_11:
  c->vadd_bc(DEST::x, BC::z, vf6, vf6, vf6);        // vaddz.x vf6, vf6, vf6
  // nop                                            // sll r0, r0, 0
  c->mov128_gpr_vf(v1, vf6);                        // qmfc2.i v1, vf6
  // nop                                            // sll r0, r0, 0
  c->mtc1(f4, v1);                                  // mtc1 f4, v1
  // nop                                            // sll r0, r0, 0
  cop1_bc = c->fprs[f4] < c->fprs[f0];              // c.lt.s f4, f0
  // nop                                            // sll r0, r0, 0
  if (cop1_bc) {                                    // bc1tl L38
    c->daddiu(s2, s2, 64);                          // daddiu s2, s2, 64
    goto block_3;
  }

block_13:
  c->lqc2(vf8, 0, s2);                              // lqc2 vf8, 0(s2)
  c->lqc2(vf9, 16, s2);                             // lqc2 vf9, 16(s2)
  c->lqc2(vf10, 32, s2);                            // lqc2 vf10, 32(s2)
  c->lwu(v1, 48, s2);                               // lwu v1, 48(s2)
  c->lw(a0, 52, s2);                                // lw a0, 52(s2)
  c->swc1(f2, 0, s3);                               // swc1 f2, 0(s3)
  c->sqc2(vf3, 48, gp);                             // sqc2 vf3, 48(gp)
  c->sqc2(vf2, 64, gp);                             // sqc2 vf2, 64(gp)
  c->sqc2(vf8, 0, gp);                              // sqc2 vf8, 0(gp)
  c->sqc2(vf9, 16, gp);                             // sqc2 vf9, 16(gp)
  c->sqc2(vf10, 32, gp);                            // sqc2 vf10, 32(gp)
  c->sw(v1, 80, gp);                                // sw v1, 80(gp)
  c->sw(a0, 84, gp);                                // sw a0, 84(gp)
  //beq r0, r0, L38                                 // beq r0, r0, L38
  c->daddiu(s2, s2, 64);                            // daddiu s2, s2, 64
  goto block_3;                                     // branch always


block_14:
  c->lwc1(f1, 0, s3);                               // lwc1 f1, 0(s3)
  c->lwc1(f5, 4, s3);                               // lwc1 f5, 4(s3)
  cop1_bc = c->fprs[f1] == c->fprs[f5];             // c.eq.s f1, f5
  if (!cop1_bc) {                                   // bc1fl L41
    c->mfc1(v0, f1);                                // mfc1 v0, f1
    goto block_17;
  }

// block_16:
  c->lui(v0, -13122);                               // lui v0, -13122
  c->ori(v0, v0, 48160);                            // ori v0, v0, 48160

block_17:
  c->ld(ra, 0, sp);                                 // ld ra, 0(sp)
  c->lq(gp, 656, sp);                               // lq gp, 656(sp)
  c->lq(s5, 640, sp);                               // lq s5, 640(sp)
  c->lq(s4, 624, sp);                               // lq s4, 624(sp)
  c->lq(s3, 608, sp);                               // lq s3, 608(sp)
  c->lq(s2, 592, sp);                               // lq s2, 592(sp)
  c->lq(s1, 576, sp);                               // lq s1, 576(sp)
  //jr ra                                           // jr ra
  c->daddiu(sp, sp, 672);                           // daddiu sp, sp, 672
  goto end_of_function;                             // return

  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
end_of_function:
  return c->gprs[v0].du64[0];
}

void link() {
  cache.moving_sphere_triangle_intersect = intern_from_c(-1, 0, "moving-sphere-triangle-intersect").c();
  gLinkedFunctionTable.reg("(method 9 collide-cache-prim)", execute, 1024);
}

} // namespace method_9_collide_cache_prim
} // namespace Mips2C
// add method_9_collide_cache_prim::link to the link callback table for the object file.
// FWD DEC:

//--------------------------MIPS2C---------------------
// clang-format off
#include "game/mips2c/mips2c_private.h"
#include "game/kernel/jakx/kscheme.h"
using ::jakx::intern_from_c;
namespace Mips2C::jakx {
namespace method_10_collide_cache_prim {
struct Cache {
  void* moving_sphere_sphere_intersect; // moving-sphere-sphere-intersect
} cache;

u64 execute(void* ctxt) {
  auto* c = (ExecutionContext*)ctxt;
  bool bc = false;
  u32 call_addr = 0;
  bool cop1_bc = false;
  c->daddiu(sp, sp, -128);                          // daddiu sp, sp, -128
  c->sd(ra, 0, sp);                                 // sd ra, 0(sp)
  c->sd(fp, 8, sp);                                 // sd fp, 8(sp)
  c->mov64(fp, t9);                                 // or fp, t9, r0
  c->sq(s1, 32, sp);                                // sq s1, 32(sp)
  c->sq(s2, 48, sp);                                // sq s2, 48(sp)
  c->sq(s3, 64, sp);                                // sq s3, 64(sp)
  c->sq(s4, 80, sp);                                // sq s4, 80(sp)
  c->sq(s5, 96, sp);                                // sq s5, 96(sp)
  c->sq(gp, 112, sp);                               // sq gp, 112(sp)
  c->mov64(s3, a0);                                 // or s3, a0, r0
  c->mov64(gp, a1);                                 // or gp, a1, r0
  c->mov64(s5, a3);                                 // or s5, a3, r0
  c->mov64(s2, t0);                                 // or s2, t0, r0
  c->mov64(s4, t1);                                 // or s4, t1, r0
  c->daddiu(s1, sp, 16);                            // daddiu s1, sp, 16
  c->load_symbol2(t9, cache.moving_sphere_sphere_intersect);// lw t9, moving-sphere-sphere-intersect(s7)
  c->mov64(a0, a2);                                 // or a0, a2, r0
  c->mov64(a1, s5);                                 // or a1, s5, r0
  c->daddu(a2, r0, s3);                             // daddu a2, r0, s3
  c->mov64(a3, s1);                                 // or a3, s1, r0
  call_addr = c->gprs[t9].du32[0];                  // function call:
  c->sll(v0, ra, 0);                                // sll v0, ra, 0
  c->jalr(call_addr);                               // jalr ra, t9
  c->mtc1(f3, v0);                                  // mtc1 f3, v0
  c->lui(v1, -13122);                               // lui v1, -13122
  c->ori(v1, v1, 48160);                            // ori v1, v1, 48160
  c->mtc1(f4, v1);                                  // mtc1 f4, v1
  c->mtc1(f0, r0);                                  // mtc1 f0, r0
  c->lqc2(vf4, 0, s1);                              // lqc2 vf4, 0(s1)
  c->movs(f1, f3);                                  // mov.s f1, f3
  c->lqc2(vf5, 0, s3);                              // lqc2 vf5, 0(s3)
  cop1_bc = c->fprs[f1] < c->fprs[f0];              // c.lt.s f1, f0
  c->vmove(DEST::xyzw, vf1, vf0);                   // vmove.xyzw vf1, vf0
  bc = cop1_bc;                                     // bc1t L35
  c->mtc1(f2, s2);                                  // mtc1 f2, s2
  if (bc) {goto block_11;}                          // branch non-likely

  cop1_bc = c->fprs[f2] < c->fprs[f0];              // c.lt.s f2, f0
  c->vsub(DEST::xyz, vf1, vf4, vf5);                // vsub.xyz vf1, vf4, vf5
  bc = cop1_bc;                                     // bc1t L32
  c->lwu(v1, 36, s3);                               // lwu v1, 36(s3)
  if (bc) {goto block_4;}                           // branch non-likely

  cop1_bc = c->fprs[f2] <= c->fprs[f1];             // c.le.s f2, f1
  // nop                                            // sll r0, r0, 0
  if (cop1_bc) {                                    // bc1tl L35
    c->movs(f3, f4);                                // mov.s f3, f4
    goto block_11;
  }

block_4:
  c->andi(a0, s4, 1);                               // andi a0, s4, 1
  // nop                                            // sll r0, r0, 0
  bc = c->sgpr64(a0) == 0;                          // beq a0, r0, L33
  c->lqc2(vf15, 0, s5);                             // lqc2 vf15, 0(s5)
  if (bc) {goto block_7;}                           // branch non-likely

  c->vmul(DEST::xyzw, vf16, vf15, vf1);             // vmul.xyzw vf16, vf15, vf1
  c->vadd_bc(DEST::y, BC::x, vf16, vf16, vf16);     // vaddx.y vf16, vf16, vf16
  c->vadd_bc(DEST::y, BC::z, vf16, vf16, vf16);     // vaddz.y vf16, vf16, vf16
  c->mov128_gpr_vf(a0, vf16);                       // qmfc2.i a0, vf16
  if (((s64)c->sgpr64(a0)) >= 0) {                  // bgezl a0, L35
    c->movs(f3, f4);                                // mov.s f3, f4
    goto block_11;
  }

block_7:
  // daddiu a0, fp, L217                               // daddiu a0, fp, L217
  //   .word 0x0
  //   .word 0x45800000
  //   .word 0x0
  //   .word 0x3f800000
  //   .word 0x0
  //   .word 0xc5800000
  //   .word 0x45800000
  //   .word 0x3f800000
  //   .word 0x0
  //   .word 0xc5800000
  //   .word 0xc5800000
  //   .word 0x3f800000
  // nop                                            // sll r0, r0, 0
  c->vmul(DEST::xyzw, vf12, vf1, vf1);              // vmul.xyzw vf12, vf1, vf1
  c->sqc2(vf4, 48, gp);                             // sqc2 vf4, 48(gp)
  c->vmula_bc(DEST::w, BC::x, vf0, vf12);           // vmulax.w acc, vf0, vf12
  // c->lqc2(vf9, 0, a0);                              // lqc2 vf9, 0(a0)
  c->vfs[9].vf.set_u32s(0, 0x45800000, 0, 0x3f800000);
  c->vmadda_bc(DEST::w, BC::y, vf0, vf12);          // vmadday.w acc, vf0, vf12
  // c->lqc2(vf10, 16, a0);                            // lqc2 vf10, 16(a0)
  c->vfs[10].vf.set_u32s(0, 0xc5800000, 0x45800000, 0x3f800000);
  c->vmadd_bc(DEST::w, BC::z, vf12, vf0, vf12);     // vmaddz.w vf12, vf0, vf12
  // c->lqc2(vf11, 32, a0);                            // lqc2 vf11, 32(a0)
  c->vfs[11].vf.set_u32s(0, 0xc5800000, 0xc5800000, 0x3f800000);
  c->vrsqrt(vf0, BC::w, vf12, BC::w);               // vrsqrt Q, vf0.w, vf12.w
  // nop                                            // sll r0, r0, 0
  c->vwaitq();                                      // vwaitq
  c->lwu(a0, 60, v1);                               // lwu a0, 60(v1)
  c->vmulq(DEST::xyz, vf1, vf1);                    // vmulq.xyz vf1, vf1, Q
  // nop                                            // sll r0, r0, 0
  c->vmul(DEST::xyzw, vf14, vf1, vf1);              // vmul.xyzw vf14, vf1, vf1
  c->sqc2(vf1, 64, gp);                             // sqc2 vf1, 64(gp)
  c->vabs(DEST::xyzw, vf13, vf1);                   // vabs.xyzw vf13, vf1
  c->sw(a0, 80, gp);                                // sw a0, 80(gp)
  c->vmove(DEST::xyzw, vf2, vf0);                   // vmove.xyzw vf2, vf0
  c->sw(v1, 84, gp);                                // sw v1, 84(gp)
  c->vadd_bc(DEST::x, BC::y, vf14, vf14, vf14);     // vaddy.x vf14, vf14, vf14
  c->mov128_gpr_vf(v1, vf13);                       // qmfc2.i v1, vf13
  if (((s64)c->sgpr64(v1)) == ((s64)0)) {           // beql v1, r0, L34
    c->vadd_bc(DEST::x, BC::z, vf2, vf0, vf1);      // vaddz.x vf2, vf0, vf1
    goto block_10;
  }

// block_9:
  c->vsub_bc(DEST::x, BC::y, vf2, vf0, vf1);        // vsuby.x vf2, vf0, vf1
  // nop                                            // sll r0, r0, 0
  c->vrsqrt(vf0, BC::w, vf14, BC::x);               // vrsqrt Q, vf0.w, vf14.x
  // nop                                            // sll r0, r0, 0
  c->vadd_bc(DEST::y, BC::x, vf2, vf0, vf1);        // vaddx.y vf2, vf0, vf1
  // nop                                            // sll r0, r0, 0
  c->vwaitq();                                      // vwaitq
  // nop                                            // sll r0, r0, 0
  c->vmulq(DEST::xy, vf2, vf2);                     // vmulq.xy vf2, vf2, Q
  // nop                                            // sll r0, r0, 0

block_10:
  c->vopmula(vf1, vf2);                             // vopmula.xyz acc, vf1, vf2
  // nop                                            // sll r0, r0, 0
  c->vopmsub(vf3, vf2, vf1);                        // vopmsub.xyz vf3, vf2, vf1
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf4, vf0);         // vmulaw.xyzw acc, vf4, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf1, vf9);        // vmaddax.xyzw acc, vf1, vf9
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf9);        // vmadday.xyzw acc, vf2, vf9
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyz, BC::z, vf9, vf3, vf9);     // vmaddz.xyz vf9, vf3, vf9
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf4, vf0);         // vmulaw.xyzw acc, vf4, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf1, vf10);       // vmaddax.xyzw acc, vf1, vf10
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf10);       // vmadday.xyzw acc, vf2, vf10
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyz, BC::z, vf10, vf3, vf10);   // vmaddz.xyz vf10, vf3, vf10
  // nop                                            // sll r0, r0, 0
  c->vmula_bc(DEST::xyzw, BC::w, vf4, vf0);         // vmulaw.xyzw acc, vf4, vf0
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::x, vf1, vf11);       // vmaddax.xyzw acc, vf1, vf11
  // nop                                            // sll r0, r0, 0
  c->vmadda_bc(DEST::xyzw, BC::y, vf2, vf11);       // vmadday.xyzw acc, vf2, vf11
  // nop                                            // sll r0, r0, 0
  c->vmadd_bc(DEST::xyz, BC::z, vf11, vf3, vf11);   // vmaddz.xyz vf11, vf3, vf11
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf9, 0, gp);                              // sqc2 vf9, 0(gp)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf10, 16, gp);                            // sqc2 vf10, 16(gp)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf11, 32, gp);                            // sqc2 vf11, 32(gp)
  c->gprs[v1].du64[0] = 0;                          // or v1, r0, r0

block_11:
  c->mfc1(v0, f3);                                  // mfc1 v0, f3
  c->ld(ra, 0, sp);                                 // ld ra, 0(sp)
  c->ld(fp, 8, sp);                                 // ld fp, 8(sp)
  c->lq(gp, 112, sp);                               // lq gp, 112(sp)
  c->lq(s5, 96, sp);                                // lq s5, 96(sp)
  c->lq(s4, 80, sp);                                // lq s4, 80(sp)
  c->lq(s3, 64, sp);                                // lq s3, 64(sp)
  c->lq(s2, 48, sp);                                // lq s2, 48(sp)
  c->lq(s1, 32, sp);                                // lq s1, 32(sp)
  //jr ra                                           // jr ra
  c->daddiu(sp, sp, 128);                           // daddiu sp, sp, 128
  goto end_of_function;                             // return

  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
end_of_function:
  return c->gprs[v0].du64[0];
}

void link() {
  cache.moving_sphere_sphere_intersect = intern_from_c(-1, 0, "moving-sphere-sphere-intersect").c();
  gLinkedFunctionTable.reg("(method 10 collide-cache-prim)", execute, 256);
}

} // namespace method_10_collide_cache_prim
} // namespace Mips2C
// add method_10_collide_cache_prim::link to the link callback table for the object file.
// FWD DEC:

//--------------------------MIPS2C---------------------
// clang-format off
#include "game/mips2c/mips2c_private.h"
#include "game/kernel/jakx/kscheme.h"
using ::jakx::intern_from_c;
namespace Mips2C::jakx {
namespace method_17_collide_cache {
struct Cache {
  void* fake_scratchpad_data; // *fake-scratchpad-data*
  void* collide_puss_work; // collide-puss-work
  void* format; // format
} cache;

u64 execute(void* ctxt) {
  auto* c = (ExecutionContext*)ctxt;
  bool bc = false;
  u32 call_addr = 0;
  c->daddiu(sp, sp, -96);                           // daddiu sp, sp, -96
  c->sd(ra, 0, sp);                                 // sd ra, 0(sp)
  c->sd(fp, 8, sp);                                 // sd fp, 8(sp)
  c->mov64(fp, t9);                                 // or fp, t9, r0
  c->sq(s2, 16, sp);                                // sq s2, 16(sp)
  c->sq(s3, 32, sp);                                // sq s3, 32(sp)
  c->sq(s4, 48, sp);                                // sq s4, 48(sp)
  c->sq(s5, 64, sp);                                // sq s5, 64(sp)
  c->sq(gp, 80, sp);                                // sq gp, 80(sp)
  c->mov64(gp, a1);                                 // or gp, a1, r0
  get_fake_spad_addr2(s5, cache.fake_scratchpad_data, 0, c);// lui s5, 28672
  c->addiu(a3, r0, 64);                             // addiu a3, r0, 64
  c->lwu(a2, 116, gp);                              // lwu a2, 116(gp)
  c->daddiu(v1, s5, 96);                            // daddiu v1, s5, 96
  c->lwu(a1, 112, gp);                              // lwu a1, 112(gp)
  c->dsubu(a3, a2, a3);                             // dsubu a3, a2, a3
  // nop                                            // sll r0, r0, 0
  bc = ((s64)c->sgpr64(a3)) > 0;                    // bgtz a3, L26
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_20;}                          // branch non-likely

  bc = c->sgpr64(a2) == 0;                          // beq a2, r0, L19
  c->lqc2(vf1, 0, a1);                              // lqc2 vf1, 0(a1)
  if (bc) {goto block_5;}                           // branch non-likely

  c->daddiu(a2, a2, -1);                            // daddiu a2, a2, -1
  c->daddiu(a1, a1, 16);                            // daddiu a1, a1, 16
  c->vsub_bc(DEST::xyz, BC::w, vf2, vf1, vf1);      // vsubw.xyz vf2, vf1, vf1
  c->sqc2(vf1, 0, v1);                              // sqc2 vf1, 0(v1)
  c->vadd_bc(DEST::xyz, BC::w, vf3, vf1, vf1);      // vaddw.xyz vf3, vf1, vf1
  c->daddiu(v1, v1, 48);                            // daddiu v1, v1, 48
  c->vftoi0(DEST::xyzw, vf4, vf2);                  // vftoi0.xyzw vf4, vf2
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::xyzw, vf5, vf3);                  // vftoi0.xyzw vf5, vf3
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf4, -32, v1);                            // sqc2 vf4, -32(v1)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf5, -16, v1);                            // sqc2 vf5, -16(v1)

block_3:
  bc = c->sgpr64(a2) == 0;                          // beq a2, r0, L19
  c->lqc2(vf1, 0, a1);                              // lqc2 vf1, 0(a1)
  if (bc) {goto block_5;}                           // branch non-likely

  c->daddiu(a2, a2, -1);                            // daddiu a2, a2, -1
  c->daddiu(a1, a1, 16);                            // daddiu a1, a1, 16
  c->vsub_bc(DEST::xyz, BC::w, vf4, vf1, vf1);      // vsubw.xyz vf4, vf1, vf1
  c->sqc2(vf1, 0, v1);                              // sqc2 vf1, 0(v1)
  c->vadd_bc(DEST::xyz, BC::w, vf5, vf1, vf1);      // vaddw.xyz vf5, vf1, vf1
  // nop                                            // sll r0, r0, 0
  c->vmini(DEST::xyz, vf2, vf2, vf4);               // vmini.xyz vf2, vf2, vf4
  // nop                                            // sll r0, r0, 0
  c->vmax(DEST::xyz, vf3, vf3, vf5);                // vmax.xyz vf3, vf3, vf5
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::xyzw, vf4, vf4);                  // vftoi0.xyzw vf4, vf4
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::xyzw, vf5, vf5);                  // vftoi0.xyzw vf5, vf5
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf4, 16, v1);                             // sqc2 vf4, 16(v1)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf5, 32, v1);                             // sqc2 vf5, 32(v1)
  //beq r0, r0, L18                                 // beq r0, r0, L18
  c->daddiu(v1, v1, 48);                            // daddiu v1, v1, 48
  goto block_3;                                     // branch always


block_5:
  c->vftoi0(DEST::xyzw, vf2, vf2);                  // vftoi0.xyzw vf2, vf2
  // nop                                            // sll r0, r0, 0
  c->vftoi0(DEST::xyzw, vf3, vf3);                  // vftoi0.xyzw vf3, vf3
  // nop                                            // sll r0, r0, 0
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf2, 64, s5);                             // sqc2 vf2, 64(s5)
  // nop                                            // sll r0, r0, 0
  c->sqc2(vf3, 80, s5);                             // sqc2 vf3, 80(s5)
  c->lwu(v1, 108, a0);                              // lwu v1, 108(a0)
  c->daddu(s4, r0, v1);                             // daddu s4, r0, v1
  c->lwu(s3, 100, gp);                              // lwu s3, 100(gp)
  c->lw(s2, 8, a0);                                 // lw s2, 8(a0)
  //beq r0, r0, L25                                 // beq r0, r0, L25
  // nop                                            // sll r0, r0, 0
  goto block_18;                                    // branch always


block_6:
  c->daddiu(s2, s2, -1);                            // daddiu s2, s2, -1
  c->lwu(v1, 16, s4);                               // lwu v1, 16(s4)
  c->and_(v1, s3, v1);                              // and v1, s3, v1
  bc = c->sgpr64(v1) == 0;                          // beq v1, r0, L24
  c->mov64(v1, s7);                                 // or v1, s7, r0
  if (bc) {goto block_17;}                          // branch non-likely

  c->lwu(v1, 120, gp);                              // lwu v1, 120(gp)
  if (((s64)c->sgpr64(s7)) == ((s64)c->sgpr64(v1))) {// beql s7, v1, L21
    c->daddiu(v1, s7, 4);                           // daddiu v1, s7, 4
    goto block_10;
  }

// block_9:
  c->daddiu(v1, s7, 4);                             // daddiu v1, s7, 4
  c->lwu(a0, 24, s4);                               // lwu a0, 24(s4)
  c->andi(a0, a0, 1);                               // andi a0, a0, 1
  c->movz(v1, s7, a0);                              // movz v1, s7, a0

block_10:
  bc = c->sgpr64(s7) == c->sgpr64(v1);              // beq s7, v1, L24
  c->mov64(v1, s7);                                 // or v1, s7, r0
  if (bc) {goto block_17;}                          // branch non-likely

  c->mov64(v1, s7);                                 // or v1, s7, r0
  c->lb(v1, 28, s4);                                // lb v1, 28(s4)
  c->slt(v1, v1, r0);                               // slt v1, v1, r0
  bc = c->sgpr64(v1) != 0;                          // bne v1, r0, L22
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_13;}                          // branch non-likely

  c->mov64(a0, s5);                                 // or a0, s5, r0
  c->load_symbol2(v1, cache.collide_puss_work);     // lw v1, collide-puss-work(s7)
  c->lwu(t9, 52, v1);                               // lwu t9, 52(v1)
  c->mov64(a1, s4);                                 // or a1, s4, r0
  c->mov64(a2, gp);                                 // or a2, gp, r0
  call_addr = c->gprs[t9].du32[0];                  // function call:
  c->sll(v0, ra, 0);                                // sll v0, ra, 0
  c->jalr(call_addr);                               // jalr ra, t9
  c->mov64(v1, v0);                                 // or v1, v0, r0
  c->mov64(a0, v1);                                 // or a0, v1, r0
  //beq r0, r0, L23                                 // beq r0, r0, L23
  // nop                                            // sll r0, r0, 0
  goto block_14;                                    // branch always


block_13:
  c->mov64(a0, s5);                                 // or a0, s5, r0
  c->load_symbol2(v1, cache.collide_puss_work);     // lw v1, collide-puss-work(s7)
  c->lwu(t9, 56, v1);                               // lwu t9, 56(v1)
  c->mov64(a1, s4);                                 // or a1, s4, r0
  c->mov64(a2, gp);                                 // or a2, gp, r0
  call_addr = c->gprs[t9].du32[0];                  // function call:
  c->sll(v0, ra, 0);                                // sll v0, ra, 0
  c->jalr(call_addr);                               // jalr ra, t9
  c->mov64(v1, v0);                                 // or v1, v0, r0
  c->mov64(a0, v1);                                 // or a0, v1, r0

block_14:
  bc = c->sgpr64(s7) == c->sgpr64(v1);              // beq s7, v1, L24
  c->mov64(v1, s7);                                 // or v1, s7, r0
  if (bc) {goto block_17;}                          // branch non-likely

  c->daddiu(v1, s7, 4);                             // daddiu v1, s7, #t
  c->mov64(v0, v1);                                 // or v0, v1, r0
  //beq r0, r0, L28                                 // beq r0, r0, L28
  // nop                                            // sll r0, r0, 0
  goto block_22;                                    // branch always

  c->gprs[v1].du64[0] = 0;                          // or v1, r0, r0

block_17:
  c->daddiu(s4, s4, 48);                            // daddiu s4, s4, 48

block_18:
  bc = c->sgpr64(s2) != 0;                          // bne s2, r0, L20
  // nop                                            // sll r0, r0, 0
  if (bc) {goto block_6;}                           // branch non-likely

  c->mov64(v1, s7);                                 // or v1, s7, r0
  c->mov64(v1, s7);                                 // or v1, s7, r0
  //beq r0, r0, L27                                 // beq r0, r0, L27
  // nop                                            // sll r0, r0, 0
  goto block_21;                                    // branch always


block_20:
  c->load_symbol2(t9, cache.format);                // lw t9, format(s7)
  c->addiu(a0, r0, 0);                              // addiu a0, r0, 0
  // daddiu a1, fp, L216                               // daddiu a1, fp, L216
  call_addr = c->gprs[t9].du32[0];                  // function call:
  c->sll(v0, ra, 0);                                // sll v0, ra, 0
  // c->jalr(call_addr);                               // jalr ra, t9
  printf("ERROR: Exceeded max # of spheres in collide-cache::probe-using-spheres!~");

block_21:
  c->mov64(v0, s7);                                 // or v0, s7, r0

block_22:
  c->ld(ra, 0, sp);                                 // ld ra, 0(sp)
  c->ld(fp, 8, sp);                                 // ld fp, 8(sp)
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
end_of_function:
  return c->gprs[v0].du64[0];
}

void link() {
  cache.fake_scratchpad_data = intern_from_c(-1, 0, "*fake-scratchpad-data*").c();
  cache.collide_puss_work = intern_from_c(-1, 0, "collide-puss-work").c();
  cache.format = intern_from_c(-1, 0, "format").c();
  gLinkedFunctionTable.reg("(method 17 collide-cache)", execute, 128);
}

} // namespace method_17_collide_cache
} // namespace Mips2C
// add method_17_collide_cache::link to the link callback table for the object file.
// FWD DEC:
