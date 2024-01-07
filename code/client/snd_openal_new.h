/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "qal.h"

typedef int          S32;
typedef unsigned int U32;

typedef enum {
    FADE_NONE,
    FADE_IN,
    FADE_OUT
} fade_t;

typedef struct {
    vec3_t   vOrigin;
    vec3_t   vRelativeOrigin;
    vec3_t   vVelocity;
    sfx_t   *pSfx;
    qboolean bPlaying;
    int      iChannel;
    float    fVolume;
    float    fPitch;
    int      iStartTime;
    qboolean bInUse;
    qboolean bCombine;
    float    fBaseVolume;
    float    fMinDist;
    float    fMaxDist;
    int      iFlags;
} openal_loop_sound_t;

struct openal_channel {
    sfx_t   *pSfx;
    int      iEntNum;
    int      iEntChannel;
    vec3_t   vOrigin;
    float    fVolume;
    int      iBaseRate;
    float    fNewPitchMult;
    float    fMinDist;
    float    fMaxDist;
    int      iStartTime;
    int      iTime;
    int      iEndTime;
    int      iFlags;
    int      iPausedOffset;
    int      song_number;
    fade_t   fading;
    int      fade_time;
    int      fade_start_time;
    ALuint   source;
    ALuint   buffer;
    ALubyte *bufferdata;

    void play();
    void stop();
    void pause();

    void  set_no_3d();
    void  set_3d();
    ALint get_state();

    void set_gain(float gain);
    void set_velocity(float v0, float v1, float v2);
    void set_position(float v0, float v1, float v2);

    bool is_free();
    bool is_paused();
    bool is_playing();

    void force_free();
    bool set_sfx(sfx_t *pSfx);

    void start_sample();
    void stop_sample();
    void resume_sample();
    void end_sample();

    void set_sample_pan(S32 pan);
    void set_sample_playback_rate(S32 pan);
    S32  sample_playback_rate();

    S32  sample_volume();
    U32  sample_offset();
    U32  sample_ms_offset();
    U32  sample_loop_count();
    void set_sample_offset(U32 offset);
    void set_sample_ms_offset(U32 offset);
    void set_sample_loop_count(S32 count);
    void set_sample_loop_block(S32 start_offset, S32 end_offset);

    U32 sample_status();
};

struct openal_internal_t {
    openal_channel      chan_3D[32];
    openal_channel      chan_2D[32];
    openal_channel      chan_2D_stream[32];
    openal_channel      chan_song[2];
    openal_channel      chan_mp3;
    openal_channel      chan_trig_music;
    openal_channel      chan_movie;
    openal_channel     *channel[101];
    openal_loop_sound_t loop_sounds[64];
    openal_channel      movieChannel;
    sfx_t               movieSFX;
    char                tm_filename[64];
    int                 tm_loopcount;
};