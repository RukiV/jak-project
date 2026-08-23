#include "srpc.h"

#include "common/util/Assert.h"

#include "game/overlord/jak3/iso.h"
#include "game/overlord/jak3/iso_api.h"
#include "game/overlord/jak3/overlord.h"
#include "game/overlord/jak3/rpc_interface.h"
#include "game/overlord/jak3/sbank.h"
#include "game/overlord/jak3/soundcommon.h"
#include "game/overlord/jak3/spustreams.h"
#include "game/overlord/jak3/ssound.h"
#include "game/overlord/jak3/vag.h"
#include "game/overlord/jak3/vblank_handler.h"
#include "game/runtime.h"
#include "game/sce/iop.h"
#include "game/sound/sndshim.h"

namespace jak3 {

using namespace iop;

// This file has two RPCs: PLAYER and LOADER
// Generally, PLAYER receives commands to play/pause sound effects or streams, which complete
// quickly.

// LOADER will load soundbanks, and can take some time to complete - likely why it is moved
// into its own RPC, to avoid having soundbank loads block playback of other sounds.

constexpr int kPlayerBufSize = 0x50 * 128;
static uint8_t s_anRPC_PlayerBuf[kPlayerBufSize];

constexpr int kLoaderBufSize = 0x50;
static uint8_t s_anRPC_LoaderBuf[kLoaderBufSize];

constexpr u32 kNumLanguages = 12;
static const char* languages[kNumLanguages] = {"ENG", "FRE", "GER", "SPA", "ITA", "COM",
                                               "JAP", "KOR", "RUS", "POR", "DUT", "UKE"};

const char* g_pszLanguage = languages[0];
u8 g_nFPS = 60;
SoundBankInfo* g_LoadingSoundBank = nullptr;

void jak3_overlord_init_globals_srpc() {
  g_nFPS = 60;
  g_LoadingSoundBank = nullptr;
  g_pszLanguage = languages[0];
}

u32 Thread_Player() {
  sceSifQueueData dq;
  sceSifServeData serve;

  CpuDisableIntr();
  sceSifInitRpc(0);
  sceSifSetRpcQueue(&dq, GetThreadId());
  sceSifRegisterRpc(&serve, RpcId::Player, RPC_Player, &s_anRPC_PlayerBuf, kPlayerBufSize, nullptr,
                    nullptr, &dq);

  CpuEnableIntr();
  sceSifRpcLoop(&dq);
  return 0;
}

u32 Thread_Loader() {
  sceSifQueueData dq;
  sceSifServeData serve;

  CpuDisableIntr();
  sceSifInitRpc(0);
  sceSifSetRpcQueue(&dq, GetThreadId());
  sceSifRegisterRpc(&serve, RpcId::Loader, RPC_Loader, &s_anRPC_LoaderBuf, kLoaderBufSize, nullptr,
                    nullptr, &dq);

  CpuEnableIntr();
  sceSifRpcLoop(&dq);
  return 0;
}

// Jak X renumbered the sound-command enum (nearly sequential 0-22, cancel-dgo 20,
// stereo-mode 21) and shrank the RPC element stride to 48 bytes; this shared jak3
// overlord implementation switches on the jak3 values, so translate at the boundary.
// Values recovered from the decompiled jakx gsound.gc/load-dgo.gc RPC senders.
SoundCommand TranslateJakXSoundCommand(SoundCommand in) {
  if (g_game_version != GameVersion::JakX) {
    return in;
  }
  switch ((u16)in) {
    case 0:
      return SoundCommand::LOAD_BANK;
    case 1:
      return SoundCommand::UNLOAD_BANK;
    case 2:
      return SoundCommand::LOAD_MUSIC;
    case 3:
      return SoundCommand::UNLOAD_MUSIC;
    case 4:
      return SoundCommand::PLAY;
    case 5:
      return SoundCommand::PAUSE_SOUND;
    case 6:
      return SoundCommand::STOP_SOUND;
    case 7:
      return SoundCommand::CONTINUE_SOUND;
    case 8:
      return SoundCommand::SET_PARAM;
    case 9:
      return SoundCommand::SET_MASTER_VOLUME;
    case 10:
      return SoundCommand::PAUSE_GROUP;
    case 11:
      return SoundCommand::STOP_GROUP;
    case 12:
      return SoundCommand::CONTINUE_GROUP;
    case 13:
      return SoundCommand::GET_IRX_VERSION;
    case 14:
      return SoundCommand::SET_LANGUAGE;
    case 15:
      return SoundCommand::SET_REVERB;
    case 17:
      return SoundCommand::LIST_SOUNDS;
    case 18:
      return SoundCommand::SET_FPS;
    case 20:
      return SoundCommand::CANCEL_DGO;
    case 21:
      return SoundCommand::SET_STEREO_MODE;
    case 22:
      // jakx set-globals is NOT jak3's set-ear-trans: it is an 8-byte payload
      // (u8 cam-inverted at +4, s16 music-pitch-mod at +6) sent every frame by
      // swap-sound-buffers. Parsing it as jak3's five-vector ear command fed 76
      // bytes of adjacent-command garbage into SetEarTrans each frame and crashed
      // the mixer once a 3D voice went live. jakx moved the ear/pan math EE-side
      // (build-sound-list's per-instance updaters), so the IOP needs nothing from
      // this command today; route it to the ignored case. Revisit for cam-inverted
      // (split-screen pan mirroring) and music-pitch-mod (jukebox/slow-mo) when
      // those legs land.
      return SoundCommand::SET_MIDI_REG;
    default:
      return in;
  }
}

// Jak X sends 48-byte RPC elements (jak1/2/3 use 0x50).
constexpr int kJakXCommandStride = 48;

void* RPC_Player(unsigned int, void* msg, int size) {
  if (!g_bSoundEnable) {
    return nullptr;
  }

  const int stride =
      (g_game_version == GameVersion::JakX) ? kJakXCommandStride : kPlayerCommandStride;
  // const auto* cmd = (RPC_Player_Cmd*)msg;
  ovrld_log(LogCategory::PLAYER_RPC, "Got Player RPC with {} cmds", size / stride);
  u8* m_ptr = (u8*)msg;
  const u8* end = m_ptr + size;

  for (; m_ptr < end; m_ptr += stride) {
    auto* base_cmd = (Rpc_Player_Base_Cmd*)m_ptr;
    base_cmd->command = TranslateJakXSoundCommand(base_cmd->command);
    switch (((const Rpc_Player_Base_Cmd*)m_ptr)->command) {
      case SoundCommand::PLAY: {
        if (g_game_version == GameVersion::JakX) {
          // jakx's sound-rpc-play carries name@16 and a 10-byte sound-play-params
          // (mask/group/volume/pitch-mod/bend/pan) at offset 32
          // (decompiler/config/jakx/all-types.gc), not jak3's 32-byte
          // SoundPlayParams at the same offset. jakx also does falloff and ear/pan
          // math EE-side (see the set-globals note on TranslateJakXSoundCommand
          // above), so its volume and pan are already final values: push them
          // straight through instead of recomputing via
          // CalculateFalloffVolume/CalculateAngle.
          //
          // jakx's volume is a 0..1024 Q10 multiplier
          // (goal_src/jakx/engine/sound/gsound.gc:
          // (max 0 (min 1024 (the int (* volume volumef))))), exactly what
          // snd_PlaySoundByNameVolPanPMPB's vol argument expects: play_vol =
          // (sfx_default_vol * vol) >> 10, so 1024 is unity gain against the sfx
          // block's own default volume and 0 is explicit silence
          // (game/sound/989snd/blocksound_handler.cpp:32-69). jakx's pan is
          // already a 0-360 degree angle
          // (goal_src/jakx/engine/sound/gsound.gc's update-pan-angle scaled by
          // 360/65536), the same units CalculateAngle returns for jak3. Neither
          // needs rescaling, just the right offsets.
          const auto* cmd = (const JakXPlayCmd*)m_ptr;
          ovrld_log(LogCategory::PLAYER_RPC, "[Player RPC] command PLAY {} id {}", cmd->name.data,
                    cmd->sound_id);
          s32 id = cmd->sound_id;
          if (id) {
            auto* sound = LookupSound(id);
            bool is_new = !sound;
            if (is_new) {
              ovrld_log(LogCategory::PLAYER_RPC, "[Player RPC] allocating a new one");
              sound = AllocateSound();
            }
            if (sound) {
              strcpy_toupper(sound->name.data, cmd->name.data);
              sound->params.mask = cmd->params.mask;
              sound->params.group = cmd->params.group;
              sound->params.volume = cmd->params.volume;
              sound->params.pitch_mod = cmd->params.pitch_mod;
              sound->params.bend = cmd->params.bend;
              sound->auto_time = 0;
              s32 vol = cmd->params.volume;
              s32 pan = cmd->params.pan;
              if (is_new) {
                auto handle = snd_PlaySoundByNameVolPanPMPB(0, 0, sound->name.data, vol, pan,
                                                            (int)cmd->params.pitch_mod,
                                                            (int)cmd->params.bend);
                sound->id = id;
                sound->sound_handle = handle;
              } else {
                snd_SetSoundVolPan(sound->sound_handle, vol, pan);
                snd_SetSoundPitchModifier(sound->sound_handle, (int)cmd->params.pitch_mod);
                snd_SetSoundPitchBend(sound->sound_handle, (int)cmd->params.bend);
              }
            }
          }
          break;
        }
        const auto* cmd = (const Rpc_Player_Play_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[Player RPC] command PLAY {} id {}", cmd->name.data,
                  cmd->sound_id);
        s32 id = cmd->sound_id;
        if (id) {
          auto* sound = LookupSound(id);
          if (!sound) {
            ovrld_log(LogCategory::PLAYER_RPC, "[Player RPC] allocating a new one");
            sound = AllocateSound();
            if (sound) {
              SFXUserData user_val;
              strcpy_toupper(sound->name.data, cmd->name.data);
              sound->params = cmd->params;
              sound->auto_time = 0;
              s32 get_status =
                  snd_GetSoundUserData(nullptr, nullptr, -1, sound->name.data, &user_val);
              s32 mask = sound->params.mask;
              if ((mask & 8) == 0) {
                sound->params.group = 0;
              }
              if ((mask & 0x40) == 0) {
                if (get_status == 0 || user_val.data[0] == 0) {
                  sound->params.fo_min = 5;
                } else {
                  sound->params.fo_min = (int16_t)user_val.data[0];
                }
              }
              if ((mask & 0x80) == 0) {
                if (get_status == 0 || user_val.data[1] == 0) {
                  sound->params.fo_max = 0x1e;
                } else {
                  sound->params.fo_max = (int16_t)user_val.data[1];
                }
              }
              if ((mask & 0x100) == 0) {
                u32 fo_curve = 0;
                if (get_status != 0) {
                  fo_curve = user_val.data[2];
                }
                (sound->params).fo_curve = fo_curve;
              }
              (sound->params).fo_curve = GetFalloffCurve(sound->params.fo_curve);
              auto handle = snd_PlaySoundByNameVolPanPMPB(
                  0, 0, sound->name.data, GetVolume(sound), GetPan(sound),
                  (int)(sound->params).pitch_mod, (int)(sound->params).bend);
              sound->id = id;
              sound->sound_handle = handle;
              if (handle != 0) {
                if ((sound->params.mask & 0x800) != 0) {
                  snd_SetSoundReg(sound->sound_handle, 0, sound->params.reg[0]);
                }
                if ((sound->params.mask & 0x1000) != 0) {
                  snd_SetSoundReg(sound->sound_handle, 1, sound->params.reg[1]);
                }
                if ((sound->params.mask & 0x2000) != 0) {
                  snd_SetSoundReg(sound->sound_handle, 2, sound->params.reg[2]);
                }
              }
            }
          } else {
            SFXUserData user_val;
            sound->params = cmd->params;
            s32 get_status =
                snd_GetSoundUserData(nullptr, nullptr, -1, sound->name.data, &user_val);
            s32 mask = (sound->params).mask;
            if ((mask & 8) == 0) {
              (sound->params).group = 0;
            }
            if ((mask & 0x40) == 0) {
              if (get_status == 0 || user_val.data[0] == 0) {
                (sound->params).fo_min = 5;
              } else {
                (sound->params).fo_min = user_val.data[0];
              }
            }
            if ((mask & 0x80) == 0) {
              if (get_status == 0 || user_val.data[1] == 0) {
                (sound->params).fo_max = 0x1e;
              } else {
                (sound->params).fo_max = user_val.data[1];
              }
            }
            if ((mask & 0x100) == 0) {
              s8 fo_curve = 0;
              if (get_status != 0) {
                fo_curve = user_val.data[2];
              }
              (sound->params).fo_curve = fo_curve;
            }
            (sound->params).fo_curve = GetFalloffCurve(sound->params.fo_curve);
            UpdateVolume(sound);
            snd_SetSoundPitchModifier(sound->sound_handle, (int)(sound->params).pitch_mod);
            if (((sound->params).mask & 4) != 0) {
              snd_SetSoundPitchBend(sound->sound_handle, (int)(sound->params).bend);
            }
            if (((sound->params).mask & 0x800) != 0) {
              snd_SetSoundReg(sound->sound_handle, 0, sound->params.reg[0]);
            }
            if (((sound->params).mask & 0x1000) != 0) {
              snd_SetSoundReg(sound->sound_handle, 1, (int)(char)(sound->params).reg[1]);
            }
            if (((sound->params).mask & 0x2000) != 0) {
              snd_SetSoundReg(sound->sound_handle, 2, sound->params.reg[2]);
            }
          }
        }
      } break;
      case SoundCommand::PAUSE_SOUND: {
        const auto* cmd = (const Rpc_Player_Sound_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Pause Sound ID {}", cmd->sound_id);
        if (cmd->sound_id) {
          auto* sound = LookupSound(cmd->sound_id);
          if (sound) {
            ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Found matching Sound {} to pause",
                      sound->name.data);
            snd_PauseSound(sound->sound_handle);
          } else {
            auto* vag = FindVagStreamId(cmd->sound_id);
            if (vag) {
              ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Found matching VAG {} to pause",
                        vag->name);
              PauseVAG(vag);
            }
          }
        }
      } break;
      case SoundCommand::STOP_SOUND: {
        const auto* cmd = (const Rpc_Player_Sound_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] stop Sound ID {}", cmd->sound_id);
        if (cmd->sound_id) {
          auto* sound = LookupSound(cmd->sound_id);
          if (sound) {
            ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Found matching Sound {} to stop",
                      sound->name.data);
            snd_StopSound(sound->sound_handle);
          } else {
            auto* vag = FindVagStreamId(cmd->sound_id);
            if (vag) {
              ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Found matching VAG {} to stop",
                        vag->name);
              StopVagStream(vag);
            }
          }
        }
      } break;
      case SoundCommand::CONTINUE_SOUND: {
        const auto* cmd = (const Rpc_Player_Sound_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] continue Sound ID {}", cmd->sound_id);
        if (cmd->sound_id) {
          auto* sound = LookupSound(cmd->sound_id);
          if (sound) {
            ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Found matching Sound {} to continue",
                      sound->name.data);
            snd_ContinueSound(sound->sound_handle);
          } else {
            auto* vag = FindVagStreamId(cmd->sound_id);
            if (vag) {
              ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Found matching VAG {} to continue",
                        vag->name);
              UnPauseVAG(vag);
            }
          }
        }
      } break;
      case SoundCommand::SET_PARAM: {
        if (g_game_version == GameVersion::JakX) {
          // jakx's sound-rpc-set-param has params@16, auto-time@28 and
          // auto-from@32 (decompiler/config/jakx/all-types.gc), not jak3's
          // params@8/auto_time@40/auto_from@44; JakXSetParamCmd mirrors it. jakx's
          // params only cover mask/group/volume/pitch-mod/bend (no
          // trans/fo_min/fo_max/fo_curve/priority/reg - it does falloff EE-side,
          // same as PLAY above), and the volume update pushes straight through via
          // snd_SetSoundVolPan instead of UpdateVolume's
          // CalculateFalloffVolume, for the same reason PLAY bypasses it.
          const auto* cmd = (const JakXSetParamCmd*)m_ptr;
          ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] SET_PARAM Sound ID {}", cmd->sound_id);
          if (cmd->sound_id) {
            auto* sound = LookupSound(cmd->sound_id);
            if (sound) {
              ovrld_log(LogCategory::PLAYER_RPC,
                        "[RPC Player] Found matching Sound {} to SET_PARAM", sound->name.data);
              auto& params = cmd->params;
              u16 mask = params.mask;
              s32 atime = cmd->auto_time;
              s32 afrom = cmd->auto_from;
              if ((mask & 1) != 0) {
                if ((mask & 0x10) == 0) {
                  sound->params.volume = params.volume;
                  snd_SetSoundVolPan(sound->sound_handle, sound->params.volume, -2);
                } else {
                  sound->auto_time = atime;
                  sound->new_volume = params.volume;
                }
              }
              if ((mask & 2) != 0) {
                auto pitch_mod = params.pitch_mod;
                sound->params.pitch_mod = pitch_mod;
                if ((mask & 0x10) == 0) {
                  snd_SetSoundPitchModifier(sound->sound_handle, params.pitch_mod);
                } else {
                  snd_AutoPitch(sound->sound_handle, pitch_mod, atime, afrom);
                }
              }
              if ((mask & 4) != 0) {
                auto bend = params.bend;
                sound->params.bend = bend;
                if ((mask & 0x10) == 0) {
                  snd_SetSoundPitchBend(sound->sound_handle, params.bend);
                } else {
                  snd_AutoPitchBend(sound->sound_handle, (int)bend, atime, afrom);
                }
              }
              if ((mask & 8) != 0) {
                sound->params.group = params.group;
              }
            } else {
              auto* vag = FindVagStreamId(cmd->sound_id);
              if (vag) {
                ovrld_log(LogCategory::PLAYER_RPC,
                          "[RPC Player] Found matching VAG {} to SET_PARAM", vag->name);
                auto& params = cmd->params;
                auto mask = params.mask;
                if ((mask & 2) != 0) {
                  SetVAGStreamPitch(cmd->sound_id, params.pitch_mod);
                }
                if ((mask & 8) != 0) {
                  vag->play_group = params.group;
                }
                if ((mask & 1) != 0) {
                  vag->play_volume = params.volume;
                }
              }
            }
          }
          break;
        }
        const auto* cmd = (const Rpc_Player_Set_Param_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] SET_PARAM Sound ID {}", cmd->sound_id);
        if (cmd->sound_id) {
          auto* sound = LookupSound(cmd->sound_id);
          if (sound) {
            ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Found matching Sound {} to SET_PARAM",
                      sound->name.data);
            auto& params = cmd->params;
            u16 mask = cmd->params.mask;
            s32 atime = cmd->auto_time;
            s32 afrom = cmd->auto_from;
            if ((mask & 1) != 0) {
              if ((mask & 0x10) == 0) {
                sound->params.volume = params.volume;
              } else {
                sound->auto_time = atime;
                sound->new_volume = params.volume;
              }
            }
            if ((mask & 0x20) != 0) {
              sound->params.trans[0] = params.trans[0];
              sound->params.trans[1] = params.trans[1];
              sound->params.trans[2] = params.trans[2];
            }
            if ((mask & 0x21) != 0) {
              UpdateVolume(sound);
            }
            if ((mask & 2) != 0) {
              auto pitch_mod = params.pitch_mod;
              sound->params.pitch_mod = pitch_mod;
              if ((mask & 0x10) == 0) {
                snd_SetSoundPitchModifier(sound->sound_handle, params.pitch_mod);
              } else {
                snd_AutoPitch(sound->sound_handle, pitch_mod, atime, afrom);
              }
            }
            if ((mask & 4) != 0) {
              auto bend = params.bend;
              sound->params.bend = bend;
              if ((mask & 0x10) == 0) {
                snd_SetSoundPitchBend(sound->sound_handle, params.bend);
              } else {
                snd_AutoPitchBend(sound->sound_handle, (int)bend, atime, afrom);
              }
            }
            if ((mask & 0x400) != 0) {
              sound->params.priority = params.priority;
            }
            if ((mask & 8) != 0) {
              sound->params.group = params.group;
            }
            if ((mask & 0x40) != 0) {
              sound->params.fo_min = params.fo_min;
            }
            if ((mask & 0x80) != 0) {
              sound->params.fo_max = params.fo_max;
            }
            if ((mask & 0x100) != 0) {
              sound->params.fo_curve = GetFalloffCurve(params.fo_curve);
            }
            if ((mask & 0x800) != 0) {
              sound->params.reg[0] = params.reg[0];
              snd_SetSoundReg(sound->sound_handle, 0, params.reg[0]);
            }
            if ((mask & 0x1000) != 0) {
              sound->params.reg[1] = params.reg[1];
              snd_SetSoundReg(sound->sound_handle, 1, params.reg[1]);
            }
            if ((mask & 0x2000) != 0) {
              sound->params.reg[2] = params.reg[2];
              snd_SetSoundReg(sound->sound_handle, 2, params.reg[2]);
            }
          } else {
            auto* vag = FindVagStreamId(cmd->sound_id);
            if (vag) {
              ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Found matching VAG {} to SET_PARAM",
                        vag->name);
              auto& params = cmd->params;
              auto mask = params.mask;
              if ((mask & 2) != 0) {
                SetVAGStreamPitch(cmd->sound_id, params.pitch_mod);
              }
              if ((mask & 0x20) != 0) {
                vag->trans[0] = params.trans[0];
                vag->trans[1] = params.trans[1];
                vag->trans[2] = params.trans[2];
                vag->updated_trans = 1;
              }
              if ((mask & 8) != 0) {
                vag->play_group = params.group;
              }
              if ((mask & 0x40) != 0) {
                vag->fo_min = (int)params.fo_min;
              }
              if ((mask & 0x80) != 0) {
                vag->fo_max = (int)params.fo_max;
              }
              if ((mask & 0x100) != 0) {
                vag->fo_curve = GetFalloffCurve(params.fo_curve);
              }
              if ((mask & 1) != 0) {
                vag->play_volume = params.volume;
              }
            }
          }
        }
      } break;
      case SoundCommand::SET_MASTER_VOLUME: {
        const auto* cmd = (const Rpc_Player_Set_Master_Volume_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Set Master Volume to {}", cmd->volume);
        for (int i = 0; i < 17; i++) {
          if (cmd->group & (1 << i)) {
            g_anMasterVolume[i] = cmd->volume;
            snd_SetMasterVolume(i, cmd->volume);
            SetAllVagsVol(i);
          }
        }
      } break;
      case SoundCommand::PAUSE_GROUP: {
        const auto* cmd = (const Rpc_Player_Group_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Pause groups 0b{:b}", cmd->group);
        snd_PauseAllSoundsInGroup(cmd->group);
        if (cmd->group & 4) {
          PauseVAGStreams();
        }
        if (cmd->group & 2) {
          g_bMusicPause = true;
        }
      } break;
      case SoundCommand::STOP_GROUP: {
        const auto* cmd = (const Rpc_Player_Group_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Stop groups 0b{:b}", cmd->group);
        KillSoundsInGroup(cmd->group);
        if (cmd->group & 4) {
          ISO_VAGCommand vag_cmd;
          vag_cmd.msg_type = ISO_Hdr::MsgType::VAG_STOP;  // seems unsupported by iso thread.
          vag_cmd.mbox_reply = 0;
          vag_cmd.thread_to_wake = 0;
          vag_cmd.vag_dir_entry = nullptr;
          vag_cmd.name[0] = 0;
          vag_cmd.maybe_sound_handler = 0;
          vag_cmd.id = 0;
          vag_cmd.priority_pq = 0;
          StopVagStream(&vag_cmd);
        }
      } break;
      case SoundCommand::CONTINUE_GROUP: {
        const auto* cmd = (const Rpc_Player_Group_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Continue groups 0b{:b}", cmd->group);
        snd_ContinueAllSoundsInGroup(cmd->group);
        if (cmd->group & 4) {
          UnpauseVAGStreams();
        }
        if (cmd->group & 2) {
          g_bMusicPause = false;
        }
      } break;
      case SoundCommand::SET_REVERB: {
        ovrld_log(LogCategory::WARN, "[RPC Player] Unimplemented set reverb.");
      } break;
      case SoundCommand::SET_EAR_TRANS: {
        const auto* cmd = (const Rpc_Player_Set_Ear_Trans_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] set ear trans");
        SetEarTrans(cmd->ear_trans0, cmd->ear_trans1, cmd->ear_trans, cmd->cam_forward,
                    cmd->cam_left, cmd->cam_scale, (cmd->cam_inverted != 0));
      } break;
      case SoundCommand::SHUTDOWN: {
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] Shutdown!");
        WaitSema(g_n989Semaphore);
        if (g_bSoundEnable) {
          g_bSoundEnable = false;
          snd_StopSoundSystem();
        }
        SignalSema(g_n989Semaphore);
      } break;
      case SoundCommand::SET_FPS: {
        const auto* cmd = (const Rpc_Player_Set_Fps_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] set fps {}", (int)cmd->fps);
        g_nFPS = cmd->fps;
      } break;
      case SoundCommand::CANCEL_DGO: {
        const auto* cmd = (const Rpc_Player_Cancel_Dgo_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Player] cancel dgo {}", cmd->id);
        CancelDGONoSync(cmd->id);
      } break;
      case SoundCommand::SET_MIDI_REG:
        // this is what the real overlord does - just ignore it!
        break;
      default:
        ovrld_log(LogCategory::WARN, "[RPC Player] Unsupported Player {}",
                  (int)((const Rpc_Player_Base_Cmd*)m_ptr)->command);
        ASSERT_NOT_REACHED();
    }
  }

  return nullptr;
}

void* RPC_Loader(unsigned int, void* msg, int size) {
  if (!g_bSoundEnable) {
    return nullptr;
  }

  const int stride =
      (g_game_version == GameVersion::JakX) ? kJakXCommandStride : kLoaderCommandStride;
  // const auto* cmd = (RPC_Player_Cmd*)msg;
  ovrld_log(LogCategory::PLAYER_RPC, "[RPC Loader] Got Loader RPC with {} cmds", size / stride);
  u8* m_ptr = (u8*)msg;
  const u8* end = m_ptr + size;

  for (; m_ptr < end; m_ptr += stride) {
    auto* base_cmd = (Rpc_Player_Base_Cmd*)m_ptr;
    base_cmd->command = TranslateJakXSoundCommand(base_cmd->command);
    switch (((const Rpc_Player_Base_Cmd*)m_ptr)->command) {
      case SoundCommand::LOAD_BANK: {
        auto* cmd = (const Rpc_Loader_Load_Bank_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Loader] Got sound bank load command: {}",
                  cmd->bank_name.data);
        // src = &cmd->bank_name;
        if (!LookupBank(cmd->bank_name.data)) {
          auto* info = AllocateBankName(cmd->bank_name.data, cmd->mode);
          if (info) {
            strncpyz(info->m_name1, cmd->bank_name.data, 0x10);
            info->in_use = 1;
            info->unk0 = 0;
            g_LoadingSoundBank = info;
            if (LoadSoundBankToIOP(cmd->bank_name.data, info, cmd->priority) == 0) {
              info->loaded = 1;
            } else {
              info->loaded = 0;
              info->in_use = 0;
            }
            g_LoadingSoundBank = nullptr;
          }
        }
      } break;
      case SoundCommand::LOAD_MUSIC: {
        auto* cmd = (const Rpc_Loader_Bank_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Loader] Got music load command: {}",
                  cmd->bank_name.data);

        // lock
        u32 wait_status = 1;
        while (wait_status) {
          wait_status = WaitSema(g_nMusicSemaphore);
        }

        // set music name
        if ((cmd->bank_name).data[0] == 0) {
          g_szTargetMusicName[0] = 0;
        } else {
          strcpy(g_szTargetMusicName, cmd->bank_name.data);
        }

        // release
        SignalSema(g_nMusicSemaphore);
      } break;

      case SoundCommand::UNLOAD_BANK: {
        if (g_game_version == GameVersion::JakX) {
          // jakx's sound-rpc-unload-bank has no bank name on the wire at all - it
          // sends the bank's mode at offset 4 (goal_src/jakx/engine/sound/gsound.gc
          // sound-bank-unload, all-types.gc sound-rpc-unload-bank), so it has to
          // resolve against AllocateBankName's stamped gBanks[i]->mode instead of
          // LookupBank's name compare.
          auto* cmd = (const JakXUnloadBankCmd*)m_ptr;
          ovrld_log(LogCategory::PLAYER_RPC, "[RPC Loader] Got bank unload command: mode {}",
                    cmd->mode);
          SoundBankInfo* ifno = LookupBankByMode(cmd->mode);
          if (ifno) {
            auto snd_handle = ifno->snd_handle;
            ifno->snd_handle = nullptr;
            if (ifno->unk0 == 0) {
              ifno->in_use = 0;
            }
            ifno->mode = 0;
            ifno->loaded = 0;
            snd_UnloadBank(snd_handle);
            snd_ResolveBankXREFS();
          }
          break;
        }
        auto* cmd = (const Rpc_Loader_Bank_Cmd*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Loader] Got bank load unload command: {}",
                  cmd->bank_name.data);
        SoundBankInfo* ifno = LookupBank(cmd->bank_name.data);
        if (ifno) {
          auto snd_handle = ifno->snd_handle;
          ifno->snd_handle = nullptr;
          if (ifno->unk0 == 0) {
            ifno->in_use = 0;
          }
          ifno->mode = 0;
          ifno->loaded = 0;
          snd_UnloadBank(snd_handle);
          snd_ResolveBankXREFS();
        }
      } break;

      case SoundCommand::GET_IRX_VERSION: {
        auto* cmd = (Rpc_Loader_Get_Irx_Version*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Loader] Got IRX version command");
        g_nInfoEE = cmd->ee_addr;
        cmd->major = 4;
        cmd->minor = 0;
        return cmd;
      } break;

      case SoundCommand::SET_LANGUAGE: {
        auto* cmd = (Rpc_Loader_Set_Language*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Loader] Got set language command {}", cmd->lang);
        ASSERT(cmd->lang < kNumLanguages);
        g_pszLanguage = languages[cmd->lang];
      } break;

      case SoundCommand::UNLOAD_MUSIC: {
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Loader] Got unload music command");

        // lock
        u32 wait_status = 1;
        while (wait_status) {
          wait_status = WaitSema(g_nMusicSemaphore);
        }

        // set music name
        g_szTargetMusicName[0] = 0;

        // release
        SignalSema(g_nMusicSemaphore);
      } break;

      case SoundCommand::SET_STEREO_MODE: {
        auto* cmd = (Rpc_Loader_Set_Stereo_Mode*)m_ptr;
        ovrld_log(LogCategory::PLAYER_RPC, "[RPC Loader] Got set stereo command {}", cmd->mode);

        switch (cmd->mode) {
          case 0:
            SetPlaybackMode(1);
            break;
          case 1:
            SetPlaybackMode(2);
            break;
          case 2:
            SetPlaybackMode(0);
            break;
          default:
            ASSERT_NOT_REACHED();
        }
      } break;

      // added
      case SoundCommand::LIST_SOUNDS: {
        PrintBanks();
        PrintSounds();
      } break;

      case SoundCommand::MIRROR_MODE: {
        auto* cmd = (Rpc_Loader_Set_Mirror_Mode*)m_ptr;
        if (cmd->mode == 0) {
          g_CameraInvert = false;
        } else {
          g_CameraInvert = true;
        }
      } break;

      default:
        ovrld_log(LogCategory::WARN, "[RPC Loader] Unsupported Loader {}",
                  (int)((const Rpc_Player_Base_Cmd*)m_ptr)->command);
        ASSERT_NOT_REACHED();
        break;
    }
  }
  return nullptr;
}

void SetVagStreamName(ISO_VAGCommand* cmd, int len) {
  ASSERT(cmd);
  if (!cmd->music_flag && cmd->info_idx < 4) {
    if (!len) {
      g_SRPCSoundIOPInfo.stream_name[cmd->info_idx].chars[0] = 0;
    } else {
      strncpy(g_SRPCSoundIOPInfo.stream_name[cmd->info_idx].chars, cmd->name, 0x30);
    }
  } else {
    //    ASSERT_NOT_REACHED();
  }
}

}  // namespace jak3