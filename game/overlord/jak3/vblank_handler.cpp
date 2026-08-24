#include "vblank_handler.h"

#include <cstring>

#include "common/log/log.h"
#include "common/util/Assert.h"

#include "game/overlord/jak3/dma.h"
#include "game/overlord/jak3/iso.h"
#include "game/overlord/jak3/overlord.h"
#include "game/overlord/jak3/sbank.h"
#include "game/overlord/jak3/srpc.h"
#include "game/overlord/jak3/ssound.h"
#include "game/overlord/jak3/streamlist.h"
#include "game/overlord/jak3/vag.h"
#include "game/runtime.h"
#include "game/sce/iop.h"

namespace jak3 {
using namespace iop;
u32 g_nInfoEE = 0;
SoundIOPInfo g_SRPCSoundIOPInfo;
JakXSoundIOPInfo g_JakXSoundIOPInfo;
bool g_bVBlankInitialized = false;
s32 g_nVBlankThreadID = -1;
s32 g_nVBlankSemaphoreID = -1;
bool g_bVBlankRegistered = false;
u32 g_nIopTicks = 0;
u32 g_nFrameNum = 0;
void jak3_overlord_init_globals_vblank_handler() {
  g_nInfoEE = 0;
  g_SRPCSoundIOPInfo = {};
  g_JakXSoundIOPInfo = {};
  g_bVBlankInitialized = false;
  g_nVBlankThreadID = -1;
  g_nVBlankSemaphoreID = -1;
  g_bVBlankRegistered = false;
  g_nIopTicks = 0;
  g_nFrameNum = 0;
}

int VBlankHandler(void*);
u32 VBlankThread();

void VBlank_Initialize() {
  ThreadParam thread_param;
  SemaParam sema_param;

  if (g_bVBlankInitialized == 0) {
    thread_param.attr = 0x2000000;
    thread_param.stackSize = 0x800;
    thread_param.initPriority = 0x34;
    thread_param.option = 0;
    thread_param.entry = VBlankThread;
    strcpy(thread_param.name, "vblank");
    g_nVBlankThreadID = CreateThread(&thread_param);
    ASSERT(g_nVBlankThreadID >= 0);
    sema_param.max_count = 200;  // hack
    sema_param.attr = 0;
    sema_param.init_count = 0;
    sema_param.option = 0;
    g_nVBlankSemaphoreID = CreateSema(&sema_param);
    ASSERT(g_nVBlankSemaphoreID >= 0);
    int ret = StartThread(g_nVBlankThreadID, 0);
    ASSERT(ret == 0);
    RegisterVblankHandler(0, 0x40, VBlankHandler, 0);
    g_bVBlankInitialized = true;
    g_bVBlankRegistered = true;
  }
}

int VBlankHandler(void*) {
  if ((g_bVBlankInitialized != 0) && (-1 < g_nVBlankSemaphoreID)) {
    SignalSema(g_nVBlankSemaphoreID);  // was iSignalSema
  }
  return 1;
}

// Kept behind this switch for issue 758's residual: a same-track restart every
// ~228s. Defaults off; the acceptance boot already read vag[4] as the music
// command and vag[5] as its stereo secondary.
static constexpr bool kJakxMusicFillProbe = false;

// Fills g_JakXSoundIOPInfo, the jakx-shaped counterpart to the jak3 fill below
// (issue #698, JakXSoundIOPInfo in rpc_interface.h). Called once the jak3 fill has
// finished writing g_SRPCSoundIOPInfo for this vblank, so the per-vag-command and
// chinfo work below can be reused rather than duplicated.
void FillJakXSoundIOPInfo() {
  auto& info = g_JakXSoundIOPInfo;

  // freemem/freemem2/pads: harmless per the reader inventory (debug only),
  // zeroed rather than porting jak3's freemem=12345 hack.
  info.freemem = 0;
  info.freemem2 = 0;
  info.pad0[0] = 0;
  info.pad0[1] = 0;
  info.pad1[0] = 0;
  info.pad1[1] = 0;
  info.pad1[2] = 0;
  info.pad1[3] = 0;

  // music_position/music_status/music_name: update-jukebox-music (gsound.gc:1852)
  // is the only reader of these three fields, and it decides a track has
  // finished from them: an empty name starts the first track, a frozen position
  // ends the current one. Both used to be zeroed unconditionally, which made the
  // empty-name test true on every 5-second poll and advanced the track list
  // regardless of what was actually playing (issue 758). g_aVagCmds[4] and [5]
  // are the music slots (vag.h, vag.cpp:243-250), distinguished from the four
  // stream slots g_aVagCmds[0..3] the loop above reads by music_flag; read
  // unlocked the same way that loop reads the stream slots, so this adds no new
  // hazard (no g_nMusicSemaphore taken). Both name and position are required:
  // name alone leaves the position-equality test in update-jukebox-music firing
  // every other poll.
  info.music_position = 0;
  info.music_status = 0;
  info.music_name = {};
  for (int i = 4; i < 6; i++) {
    auto* cmd = &g_aVagCmds[i];
    if (cmd->id && cmd->music_flag && !cmd->flags.stereo_secondary) {
      strncpyz(info.music_name.chars, cmd->name, sizeof(info.music_name.chars));
      info.music_position = cmd->position_for_ee;  // 1/1024 s units, gsound.gc:1863's scale
      info.music_status = cmd->pack_flags();       // no GOAL reader today
      break;
    }
  }

  if (kJakxMusicFillProbe) {
    static u32 s_probe_counter = 0;
    if (++s_probe_counter >= g_nFPS) {
      s_probe_counter = 0;
      auto& a = g_aVagCmds[4];
      auto& b = g_aVagCmds[5];
      lg::info(
          "jukebox probe: vag[4] id={} name={} pos={} running={} saw_chunks1={} | "
          "vag[5] id={} name={} pos={} running={} saw_chunks1={}",
          a.id, static_cast<const char*>(a.name), a.position_for_ee, a.flags.running,
          a.flags.saw_chunks1, b.id, static_cast<const char*>(b.name), b.position_for_ee,
          b.flags.running, b.flags.saw_chunks1);
    }
  }

  // nocd/dirtycd: today dirtycd receives the freemem 12345 hack and reads
  // permanently nonzero; jakx's is-cd-in? (gsound.gc:221) only tests nocd for zero.
  info.nocd = 0;
  info.dirtycd = 0;

  // chinfo: the same 48 bytes the jak3 fill above just wrote.
  memcpy(info.chinfo, g_SRPCSoundIOPInfo.chinfo, sizeof(info.chinfo));

  // id-info: zeroed. The jak3 bytes at this offset carry dupseg=-1 and the times[]
  // block, not per-channel ids; build-sound-list (gsound.gc:1125) compares live
  // sound ids against these words, and live ids start at #x10000 (gsound.gc:228),
  // so zero can never false-match.
  memset(info.id_info, 0, sizeof(info.id_info));

  // stream_position/stream_status/stream_name/stream_id: the real values, copied
  // straight from the jak3 fill's own per-vag-command loop above to the jakx
  // offsets. These gate loader.gc's gui-control machine (loader.gc:931-956) and
  // ambient speech (ambient.gc:592-594).
  for (int i = 0; i < 4; i++) {
    info.stream_position[i] = g_SRPCSoundIOPInfo.stream_position[i];
    info.stream_status[i] = g_SRPCSoundIOPInfo.stream_status[i];
    strncpyz(info.stream_name[i].chars, g_SRPCSoundIOPInfo.stream_name[i].chars,
             sizeof(info.stream_name[i].chars));
    info.stream_id[i] = g_SRPCSoundIOPInfo.stream_id[i];
  }

  // sound_banks: mode-driven, not positional (issue #698, the one real design
  // decision here). jakx's GOAL expects the level bank's name at sound-banks[1]
  // (EE 576), but AllocateBankName (sbank.cpp:81-146) routes "common" to gBanks[0],
  // "mode" to gBanks[1], and mode==4 ("full") banks to the first free even index
  // >= 2 (sbank.cpp:100-107); a positional copy (the jak3 fill above) leaves slot 1
  // empty and fixes nothing. jakx never loads a "mode" bank itself: the only
  // sound-bank-load calls in goal_src/jakx are "common" mode 2 (gsound.gc:1738 and
  // 1822) and the level bank (level.gc:10146), so scanning gBanks for the in-use
  // mode==4 bank and the in-use mode==2 bank is unambiguous. slot 1 gets the
  // mode==4 bank's m_name1, slot 0 gets the mode==2 bank's m_name1, slots 2-7 stay
  // zeroed.
  memset(info.sound_banks, 0, sizeof(info.sound_banks));
  for (int i = 0; i < 8; i++) {
    auto* bank = gBanks[i];
    if (!bank->in_use || !bank->loaded) {
      continue;
    }
    if (bank->mode == 4) {
      strncpyz(info.sound_banks[1], bank->m_name1, sizeof(info.sound_banks[1]));
    } else if (bank->mode == 2) {
      strncpyz(info.sound_banks[0], bank->m_name1, sizeof(info.sound_banks[0]));
    }
  }
}

u32 VBlankThread() {
  //  char *pcVar1;
  //  int iVar2;
  //  uint uVar3;
  //  SoundBankInfo *pSVar4;
  //  ISO_VAGCommand *cmd;
  //  SoundBankInfo **ppSVar5;
  //  uint *puVar6;
  //  uint uVar7;
  //  int iVar8;
  //  int iVar9;
  //  SoundIOPInfo *local_30;
  //  void *local_2c;
  //  undefined4 local_28;
  //  undefined4 local_24;
  //  undefined4 local_20 [2];

  do {
    while ((g_bVBlankInitialized == 0 || (g_nVBlankSemaphoreID < 0))) {
      DelayThread(1000000);
    }
    WaitSema(g_nVBlankSemaphoreID);
    g_nIopTicks = g_nIopTicks + 1;
    if (g_bSoundEnable != 0) {
      CheckVagStreamsProgress();
      if ((g_nIopTicks & 1U) != 0) {
        StreamListThread();
      }
      if (g_nMusicFadeDir < 0) {
        g_nMusicFade = g_nMusicFade + -0x200;
        if (g_nMusicFade < 0) {
          g_nMusicFade = 0;
        LAB_00011f60:
          g_nMusicFadeDir = 0;
        }
      } else {
        if ((0 < g_nMusicFadeDir) &&
            (g_nMusicFade = g_nMusicFade + 0x400, 0x10000 < g_nMusicFade)) {
          g_nMusicFade = 0x10000;
          goto LAB_00011f60;
        }
      }
      if (g_nInfoEE) {
        g_nFrameNum = g_nFrameNum + 1;
        // puVar6 = g_SRPCSoundIOPInfo.stream_status;
        for (int i = 0; i < 4; i++) {
          auto* cmd = &g_aVagCmds[i];
          u32 stream_status = cmd->pack_flags();

          if ((cmd->flags.file_disappeared != 0) && (cmd->flags.paused == 0)) {
            auto uVar3 = CalculateVAGPitch(0x400, cmd->pitch_cmd);
            if (g_nFPS == 0) {
              ASSERT_NOT_REACHED();
            }
            cmd->clockd = cmd->clockd + uVar3 / g_nFPS;
          }

          if ((cmd->flags.saw_chunks1 == 0) && (cmd->flags.clocks_set != 0)) {
            g_SRPCSoundIOPInfo.stream_status[i] = stream_status;
            g_SRPCSoundIOPInfo.stream_id[i] = cmd->id;
            g_SRPCSoundIOPInfo.stream_position[i] = 0;
          } else {
            g_SRPCSoundIOPInfo.stream_status[i] = stream_status;
            g_SRPCSoundIOPInfo.stream_id[i] = cmd->id;
            g_SRPCSoundIOPInfo.stream_position[i] = cmd->position_for_ee;
          }
        }
        // CpuSuspendIntr(local_20);

        // CpuResumeIntr(local_20[0]);
        g_SRPCSoundIOPInfo.iop_ticks = g_nIopTicks;
        g_SRPCSoundIOPInfo.freemem = 12345;  // hack
        g_SRPCSoundIOPInfo.frame = g_nFrameNum;
        g_SRPCSoundIOPInfo.freemem2 = QueryTotalFreeMemSize();
        g_SRPCSoundIOPInfo.nocd = 0;     // hack
        g_SRPCSoundIOPInfo.dirtycd = 0;  // hack
        g_SRPCSoundIOPInfo.dupseg = -1;
        g_SRPCSoundIOPInfo.diskspeed[0] = 0;
        g_SRPCSoundIOPInfo.diskspeed[1] = 0;
        g_SRPCSoundIOPInfo.lastspeed = 0;

        memset(&g_SRPCSoundIOPInfo.sound_bank0[0], 0, 8 * 16);

        if (gBanks[0]->in_use && gBanks[0]->loaded) {
          strcpy(g_SRPCSoundIOPInfo.sound_bank0, gBanks[0]->m_name1);
        }
        if (gBanks[1]->in_use && gBanks[1]->loaded) {
          strcpy(g_SRPCSoundIOPInfo.sound_bank1, gBanks[1]->m_name1);
        }
        if (gBanks[2]->in_use && gBanks[2]->loaded) {
          strcpy(g_SRPCSoundIOPInfo.sound_bank2, gBanks[2]->m_name1);
        }
        if (gBanks[3]->in_use && gBanks[3]->loaded) {
          strcpy(g_SRPCSoundIOPInfo.sound_bank3, gBanks[3]->m_name1);
        }
        if (gBanks[4]->in_use && gBanks[4]->loaded) {
          strcpy(g_SRPCSoundIOPInfo.sound_bank4, gBanks[4]->m_name1);
        }
        if (gBanks[5]->in_use && gBanks[5]->loaded) {
          strcpy(g_SRPCSoundIOPInfo.sound_bank5, gBanks[5]->m_name1);
        }
        if (gBanks[6]->in_use && gBanks[6]->loaded) {
          strcpy(g_SRPCSoundIOPInfo.sound_bank6, gBanks[6]->m_name1);
        }
        if (gBanks[7]->in_use && gBanks[7]->loaded) {
          strcpy(g_SRPCSoundIOPInfo.sound_bank7, gBanks[7]->m_name1);
        }

        for (int i = 0; i < 48; i++) {
          g_SRPCSoundIOPInfo.chinfo[i] = (snd_GetVoiceStatus(i) != 1) - 1;
        }

        sceSifDmaData dma;
        dma.addr = (void*)(u64)g_nInfoEE;
        // jakx's EE-side *sound-iop-info* (gsound-h.gc) is not this jak3 struct's
        // layout at all (issue #698): DMA-ing g_SRPCSoundIOPInfo byte-identity onto
        // it, even clamped to size, put jak3's fields at the wrong jakx offsets
        // (#175's clamp masked the overrun but not the mismatch). jakx now fills and
        // sends its own correctly-shaped JakXSoundIOPInfo (rpc_interface.h,
        // FillJakXSoundIOPInfo above); jak3 keeps sending its own full struct
        // unchanged.
        if (g_game_version == GameVersion::JakX) {
          FillJakXSoundIOPInfo();
          dma.data = &g_JakXSoundIOPInfo;
          dma.size = sizeof(g_JakXSoundIOPInfo);
          static_assert(sizeof(g_JakXSoundIOPInfo) == 0x2c0);
        } else {
          dma.data = &g_SRPCSoundIOPInfo;
          dma.size = sizeof(g_SRPCSoundIOPInfo);
          static_assert(sizeof(g_SRPCSoundIOPInfo) == 0x2d0);
        }
        dma.mode = 0;
        /*dmaid =*/sceSifSetDma(&dma, 1);
      }
    }
    RunDeferredVoiceTrans();
    // Poll(&g_DvdDriver);
  } while (true);
}

}  // namespace jak3