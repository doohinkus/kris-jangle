/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* Kris Jangle: the FM-1 plays the start of a public-domain carol (SONG:
 * on the HOME screen, turn SELECT) and you replay it on the keys. The
 * song builds as you level up: level 1 asks for the first 3 notes, and
 * each level adds 3 more on top of all the notes before, so the last
 * level (11) is the whole 32-note song on piano (or the selected sound)
 * throughout. Three misses sends you to Grump Mode (a recorded jeer,
 * then a retry). Finishing the whole song
 * brings up a celebration screen offering the next song.
 * Part of the unity build (device/unity.c, host/plat_host.c), after the
 * platform: gfx.c and announce.c are in scope. */
#include "fastmath.h"
#include "sprite_data.h"
#include "qr_data.h"

#define SET_MAGIC 0x43435235u            /* "CCR5": 4-carol settings/scores + the staff byte */
#define SET_MAGIC_V4 0x43435234u         /* "CCR4": pre-staff 4-carol record (upgraded below) */
#define SET_MAGIC_V3 0x43435233u           /* "CCR3": 3-carol record (pre-O Christmas Tree) */
#define SET_MAGIC_OLD 0x43435232u        /* "CCR2": pre-sound settings record */
#define NVOICES 6            /* melody, each sustained with a ring-out: headroom so no voice steals another */
#define NCAROLS 4
#define NPHRASES 8
#define PHRASE_LEN 4
#define NSONG_NOTES (NPHRASES * PHRASE_LEN)   /* 32 notes per carol, flattened */
#define NOTES_PER_LEVEL 3
#define NLEVELS 11                       /* 3,6,...,30,32 notes: level 11 is the whole song */
#define NWORDS 64              /* legacy: Carol Codes were removed; kept for the old record load */

/* The play mode (GLO cycles it): EASY = LISTEN blink + a steady mid-level key to
 * press in PLAY; MEDIUM = LISTEN blink only (the original behaviour); HARD =
 * no LED guidance at all. Stored in the settings' spare bytes so old saves
 * (scores, codes) keep working with no magic bump. The default is MEDIUM
 * (settings_defaults); a save that already holds a mode keeps it. */
enum { MODE_EASY = 0, MODE_MEDIUM, MODE_HARD, NMODES };
static const char *const MODE_NAME[NMODES] = {"EASY", "MEDIUM", "HARD"};

typedef struct {
    uint32_t magic;
    uint32_t best_score[NCAROLS];
    uint16_t best_level[NCAROLS];
    uint8_t last_w1, last_w2;            /* dead: Pass It On was removed; kept for record size */
    uint8_t sound_on, leds_on;           /* leds_on: kept for the store layout; superseded by mode */
    uint8_t rsv[2];                      /* rsv[0] = the play mode (MODE_*); rsv[1] = MIDI off (0 = on) */
    uint8_t snd;                         /* selected sound (SND_*) */
    uint8_t env_a, env_d, env_r;         /* dead: envelope was removed; kept for record size */
    uint8_t grain;                       /* dead: grain was removed; kept for record size */
    uint8_t staff;                       /* the grown-ups' staff on the game screens: 0 = off, 1 = on */
} settings_t;
/* The pre-staff record (CCR4, a byte shorter) loads through this and is upgraded. */
typedef struct {
    uint32_t magic;
    uint32_t best_score[NCAROLS];
    uint16_t best_level[NCAROLS];
    uint8_t last_w1, last_w2;
    uint8_t sound_on, leds_on;
    uint8_t rsv[2];
    uint8_t snd;
    uint8_t env_a, env_d, env_r;
    uint8_t grain;
} settings_v4_t;
/* The pre-sound record (CCR2, 28 bytes) loads through this and is upgraded.
 * The 3-carol CCR3 record (with sound/envelope/grain) loads through the
 * 3-wide v3 record below; both keep [3] so the current [NCAROLS] widening
 * does not change what an old save looks like. */
typedef struct {
    uint32_t magic;
    uint32_t best_score[3];
    uint16_t best_level[3];
    uint8_t last_w1, last_w2;
    uint8_t sound_on, leds_on;
    uint8_t rsv[2];
    uint8_t snd;
    uint8_t env_a, env_d, env_r;
    uint8_t grain;
} settings_v3_t;
typedef struct {
    uint32_t magic;
    uint32_t best_score[3];
    uint16_t best_level[3];
    uint8_t last_w1, last_w2;
    uint8_t sound_on, leds_on;
    uint8_t rsv[2];
} settings_legacy_t;
static settings_t set;
static uint8_t set_dirty;

static uint8_t play_mode(void) { return set.rsv[0]; }

/* The selectable sounds. 0..2 are synth (always there); 3..5 are the sampled
 * piano/flute/sax (resource clips 5/6/7). A fresh install defaults to PIANO;
 * an existing save keeps whatever the player picked. */
enum { SND_BELLS = 0, SND_CHOIR, SND_KAZOO, SND_PIANO, SND_FLUTE, SND_SAX, NSOUND };
static const char *const SND_NAME[NSOUND] = {"BELLS", "CHOIR", "KAZOO", "PIANO", "FLUTE", "SAX"};
/* the sound page's icon for each sound (art/gen_art.py writes them, art/png2c.py
 * converts them): the sprite, its palette and its size */
static const struct {
    const uint8_t *px;
    const uint16_t *pal;
    int w, h;
} SND_ICON[NSOUND] = {
    {ICON_BELLS_PX, ICON_BELLS_PAL, ICON_BELLS_W, ICON_BELLS_H},
    {ICON_CHOIR_PX, ICON_CHOIR_PAL, ICON_CHOIR_W, ICON_CHOIR_H},
    {ICON_KAZOO_PX, ICON_KAZOO_PAL, ICON_KAZOO_W, ICON_KAZOO_H},
    {ICON_PIANO_PX, ICON_PIANO_PAL, ICON_PIANO_W, ICON_PIANO_H},
    {ICON_FLUTE_PX, ICON_FLUTE_PAL, ICON_FLUTE_W, ICON_FLUTE_H},
    {ICON_SAX_PX, ICON_SAX_PAL, ICON_SAX_W, ICON_SAX_H},
};
#define PIANO_BASE 60          /* piano-c4.wav plays MIDI C4 */
#define FLUTE_BASE 72          /* flute-c5.wav plays MIDI C5 */
#define SAX_BASE 58            /* sax-a3.wav plays MIDI A#3 */
#define SND_CLIP_PIANO 5u
#define SND_CLIP_FLUTE 6u
#define SND_CLIP_SAX 7u

static uint8_t snd_id(void) { return set.snd < NSOUND ? set.snd : SND_PIANO; }

/* The sound id (SND_*) -> the voice timbre (TIM_*). TIM_* is declared below,
 * so the mapping lives next to the voice code. */
static uint8_t snd_timbre(uint8_t id);

/* MIDI out (USB and Bluetooth, channel 1): the keys you press send a
 * note-on with their velocity, the release a note-off. Demo/listen playback
 * stays internal, so a DAW sees only the player. rsv[1] is an off flag
 * (inverted) so a save that predates MIDI, with zeroes here, defaults to on. */
static uint8_t midi_on(void) { return (uint8_t)!set.rsv[1]; }
static void midi_note_on(int32_t note, uint32_t vel)
{
    if (!midi_on())
        return;
    if (vel == 0u)
        vel = 100u;
    if (vel > 127u)
        vel = 127u;
    plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | vel << 24, TO_USB | TO_BLE);   /* BLE: if built with it */
}
static void midi_note_off(int32_t note)
{
    if (!midi_on())
        return;
    plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)note << 16, TO_USB | TO_BLE);
}

/* ------------------------------------------------------------------- data --- */
/* MIDI notes, all inside F3 (53) .. G5 (80). Each carol: 8 phrases of 4 notes,
 * a singable contour of the tune, simplified to the notes that carry it.
 * CAROL_DUR gives each note's length in eighth notes (2 = quarter,
 * 4 = half, 6 = dotted half, 8 = whole/final), transcribed from the real
 * tunes onto those pitches, so the demo has the tune's rhythm and phrasing
 * instead of a flat note per slot. Jingle Bells carries the real chorus
 * pitches (E E E, E G C D E, F F, E - ...), not a three-note contour;
 * the phrase breath supplies the beat-4 rest after each long landing. */
static const uint8_t CAROL_DUR[NCAROLS][NSONG_NOTES] = {
    { /* Jingle Bells: as transcribed (low octave): E E E | E E E |
       * E G C D E | F F F F | F E E E | E D D E | D G ... */
        2, 2, 4, 2,  2, 4, 2, 2,  2, 2, 6, 6,  2, 2, 2, 2,
        2, 2, 2, 2,  2, 2, 2, 2,  4, 8, 4, 4,  4, 4, 4, 8,
    },
    { /* Deck the Halls: from deck-the-halls.mid (top voice, F major).
       * Ticks: 256 tpq, 128/eighth. The tune repeats after 17 attacks;
       * the level's 32 notes are the first 32 onsets, so phrases 0-3
       * (17 notes) lead in and 4-7 are the varied repeat. */
        3, 1, 2, 2,  2, 2, 2, 2,  1, 1, 1, 1,  3, 1, 2, 2,
        4, 3, 1, 2,  2, 2, 2, 2,  2, 1, 1, 1,  1, 3, 1, 2,
    },
    { /* Ding Dong Merrily on High: from ding-dong-merrily-on-high.mid
       * (G major, 4/4, 256 tpq, 128/eighth). Top voice per onset;
       * bass dyads left out. First 32 onsets include the long
       * dotted-half (768 ticks = 6) on idx 6 and 20. */
        2, 2, 1, 1,  1, 1, 6, 2,  2, 2, 2, 2,  4, 4, 2, 2,
        1, 1, 1, 1,  6, 2,  2, 2,  2, 2, 4, 4,  1, 1, 1, 1,
    },
    { /* O Christmas Tree: from o-christmas-tree.mid (F major piano).
       * Melody is the >=55 voice; bass-only rests are skipped and their
       * time merged in, ticks/128 rounded to the nearest eighth (>=1). */
        1, 2, 1, 3,  1, 2, 1, 3,  1, 1, 1, 2,  2, 2, 3, 1,
        1, 1, 3, 1,  2, 1, 3, 1,  1, 1, 1, 1,  1, 1, 2, 2,
    },
};
#define EIGHTH_MS 180u      /* one eighth note in the demo (quarter = 360 ms) */
#define PHRASE_GAP_MS 200u  /* breath after every 4-note phrase */
static const uint8_t CAROLS[NCAROLS][NPHRASES][PHRASE_LEN] = {
    { /* Jingle Bells (low octave): E E E | E E E | E G C D E | F F | F E E E
       * E D D E | D G -- pads holds to 32 slots by repeating the held note
       * (the transcription's 26 tokens, filler repeats don't change pitch) */
        {64, 64, 64, 64}, {64, 64, 64, 67}, {60, 62, 64, 64}, {65, 65, 65, 65},
        {65, 64, 64, 64}, {64, 62, 62, 64}, {62, 67, 67, 67}, {67, 67, 67, 67},
    },
    { /* Deck the Halls (F, from deck-the-halls.mid top voice):
       * 72 70 69 67 | 65 67 69 65 | 67 69 70 67 | 69 67 65 64 | ...
       * the MIDI's organ bass (53/F, 60/C pedal) is left out, so the
       * demo is the single top line, keyed 64..72 */
        {72, 70, 69, 67}, {65, 67, 69, 65}, {67, 69, 70, 67}, {69, 67, 65, 64},
        {65, 72, 70, 69}, {67, 65, 67, 69}, {65, 67, 69, 70}, {67, 69, 67, 65},
    },
    { /* Ding Dong Merrily on High (G): 67 67 69 67 | 66 64 62 62 | ...
       * top voice of ding-dong-merrily-on-high.mid, keyed 62..74 */
        {67, 67, 69, 67}, {66, 64, 62, 62}, {64, 67, 67, 66}, {67, 67, 67, 67},
        {69, 67, 66, 64}, {62, 62, 64, 67}, {67, 66, 67, 67}, {74, 69, 71, 72},
    },
    { /* O Christmas Tree (F): 60 65 65 65 | 67 69 69 69 | 69 67 69 70 ...
       * >=55-voice of o-christmas-tree.mid; inner/bass (41..53) dropped,
       * repeats that give the tune its waltz lift kept */
        {60, 65, 65, 65}, {67, 69, 69, 69}, {69, 67, 69, 70}, {64, 67, 65, 72},
        {72, 69, 74, 72}, {72, 70, 70, 70}, {70, 67, 72, 64}, {65, 70, 70, 69},
    },
};
static const char *const CAROL_NAME[NCAROLS] = {"JINGLE BELLS", "DECK THE HALLS", "MERRILY ON HIGH", "O CHRISTMAS TREE"};

/* ------------------------------------------------------------------ voice --- */
enum { TIM_BELLS, TIM_CHOIR, TIM_KAZOO, TIM_PIANO, TIM_FLUTE, TIM_SAX, NTIM };
typedef struct {
    int32_t note;                        /* -1 = idle */
    float phase, mphase, level, age, vel;
    float rel_age;                       /* age when a scheduled release began */
    uint8_t timbre, held, releasing;     /* releasing: ringing out, not held */
    /* Sample playback (TIM_PIANO/FLUTE/SAX): the resource clip's bytes, with
     * a Q16 position stepping by pitch. s_step 0 and s_data 0 = synth fallback. */
    const uint8_t *s_data;
    uint32_t s_idx, s_frac, s_step, s_len;
} voice_t;
static voice_t voices[NVOICES];
static volatile uint8_t voice_timbre = TIM_BELLS;

static uint8_t snd_timbre(uint8_t id)
{
    switch (id) {
    case SND_BELLS: return TIM_BELLS;
    case SND_CHOIR: return TIM_CHOIR;
    case SND_KAZOO: return TIM_KAZOO;
    case SND_PIANO: return TIM_PIANO;
    case SND_FLUTE: return TIM_FLUTE;
    case SND_SAX: return TIM_SAX;
    default: return TIM_PIANO;
    }
}
static float gain_now;

static float note_inc(int32_t note) { return 440.0f * fm_exp2f((float)(note - 69) / 12.0f) / 44100.0f; }

/* Instrument sample lookup (per voice): same CSND payload as the clips below,
 * but without touching the one-shot clip player. clip id -> data/len/rate. */
static int samp_locate(uint16_t want, const uint8_t **out_data, uint32_t *out_len, uint32_t *out_rate)
{
    uint32_t len = 0, count, i;
    const uint8_t *res = plat_res(&len);
    *out_data = 0; *out_len = 0; *out_rate = 0;
    if (!res || len < 28u)
        return 0;
    if (*(const uint32_t *)res != 0x444E5343u || *(const uint32_t *)(res + 4) != 1u)
        return 0;
    count = *(const uint32_t *)(res + 8);
    for (i = 0; i < count && 16u + i * 12u + 12u <= len; i++) {
        const uint8_t *e = res + 16u + i * 12u;
        uint16_t id = (uint16_t)(e[0] | (e[1] << 8));
        uint16_t rate = (uint16_t)(e[2] | (e[3] << 8));
        uint32_t off = *(const uint32_t *)(e + 4);
        uint32_t n = *(const uint32_t *)(e + 8);
        if (id == want && rate && off + n <= len) {
            *out_data = res + off;
            *out_len = n;
            *out_rate = rate;
            return 1;
        }
    }
    return 0;
}
/* Pitch ratio Q16 for a semitone distance (C4 = 60): 2^(d/12) * 65536. */
static uint32_t ratio_q16(int32_t d)
{
    static const uint32_t R[41] = {
        20702u, 21931u, 23224u, 24591u, 26046u, 27585u, 29215u, 30941u, 32768u, /* -20..-12 */
        34703u, 36754u, 38924u, 41221u, 43657u, 46236u, 48968u, 51861u, 54926u, 58181u, 61621u, 65262u, /* -11..0? see below */
    };
    /* The table above is 2^((d+8)/12) * 65536 for d = -20..0; compute the rest from it. */
    (void)R;
    if (d < -20) d = -20;
    if (d > 20) d = 20;
    {
        float r = 1.0f;
        int32_t k;
        for (k = 0; k < d; k++) r *= 1.0594631f;
        for (k = 0; k < -d; k++) r /= 1.0594631f;
        return (uint32_t)(r * 65536.0f + 0.5f);
    }
}

static void voice_on(int32_t note, uint32_t vel, uint8_t timbre, uint8_t held)
{
    uint32_t i, best = 0;
    float oldest = -1.0f;
    for (i = 0; i < NVOICES; i++) {
        if (voices[i].note < 0) { best = i; break; }
        if (voices[i].age > oldest) { oldest = voices[i].age; best = i; }
    }
    voices[best].note = note;
    voices[best].phase = 0.0f;
    voices[best].mphase = 0.0f;
    voices[best].level = 0.0f;
    voices[best].age = 0.0f;
    voices[best].vel = 0.55f + 0.45f * ((float)vel / 127.0f);   /* compress: light playing still speaks (demo uses 100) */
    voices[best].timbre = timbre;
    voices[best].held = held;
    voices[best].releasing = 0;
    /* Sample setup for the sampled timbres; missing clip = synth fallback. */
    voices[best].s_data = 0;
    voices[best].s_idx = 0;
    voices[best].s_frac = 0;
    voices[best].s_step = 0;
    voices[best].s_len = 0;
    if (timbre == TIM_PIANO || timbre == TIM_FLUTE || timbre == TIM_SAX) {
        const uint8_t *data;
        uint32_t slen, rate;
        uint16_t clip = timbre == TIM_PIANO ? SND_CLIP_PIANO : timbre == TIM_FLUTE ? SND_CLIP_FLUTE : SND_CLIP_SAX;
        int32_t base = timbre == TIM_PIANO ? PIANO_BASE : timbre == TIM_FLUTE ? FLUTE_BASE : SAX_BASE;
        if (samp_locate(clip, &data, &slen, &rate)) {
            voices[best].s_data = data;
            voices[best].s_len = slen;
            voices[best].s_step = (uint32_t)((float)rate * ((float)ratio_q16(note - base) / 65536.0f) * 65536.0f / 44100.0f + 0.5f);
            if (voices[best].s_step == 0)
                voices[best].s_step = 1;
        }
    }
}
/* A scheduled demo note rings out instead of being cut by the next note:
 * its envelope keeps decaying in app_render (extra release factor there),
 * and the voice frees itself after about a second. Player key releases
 * use the same path, so the fixed RELEASE tail governs both.
 * Only non-releasing voices are taken, so releasing the previous note
 * before starting the same pitch again can't silence the new one. */
static void voice_release(int32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICES; i++) {
        voice_t *vc = &voices[i];
        if (vc->note == note && !vc->releasing) {
            vc->releasing = 1;
            vc->held = 0;
            vc->rel_age = vc->age;
        }
    }
}

/* --------------------------------------------------- recorded sounds --- */
/* One-shot sample playback from the resource region (art/make_res.py:
 * unsigned 8-bit mono, with a small header: magic "CSND", version, count
 * and per-clip id/rate/offset/length). The player streams in place from
 * flash; plat_res reports 0 when the region is missing or not this
 * build's, and then there is simply no clip. */
#define SND_MAGIC 0x444E5343u          /* "CSND" */
#define SND_CLIP_INTRO 1u
#define SND_CLIP_GRUMP 2u
#define SND_CLIP_REWARD 3u
#define SND_CLIP_LOSE 4u
static const uint8_t *snd_data;        /* current clip's samples (in flash) */
static uint32_t snd_len;               /* samples in the current clip */
static uint32_t snd_rate;              /* the current clip's sample rate */
static uint32_t snd_idx, snd_frac;     /* position: sample index + Q16 fraction */
static uint32_t snd_step;              /* Q16 samples per output sample */
static int snd_on;

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
/* Find clip id in the resource region; 1 if found (and armed to play). */
static int snd_locate(uint16_t want)
{
    uint32_t len = 0, count, i;
    const uint8_t *res = plat_res(&len);
    snd_data = 0;
    snd_len = 0;
    if (!res || len < 28u)
        return 0;
    if (rd32(res) != SND_MAGIC || rd32(res + 4) != 1u)
        return 0;
    count = rd32(res + 8);
    for (i = 0; i < count && 16u + i * 12u + 12u <= len; i++) {
        const uint8_t *e = res + 16u + i * 12u;
        uint16_t id = rd16(e);
        uint16_t rate = rd16(e + 2);
        uint32_t off = rd32(e + 4);
        uint32_t nsamp = rd32(e + 8);
        if (id == want && rate && off + nsamp <= len) {
            snd_data = res + off;
            snd_len = nsamp;
            snd_rate = rate;
            /* source samples consumed per output sample at 44.1 kHz, Q16:
             * a 22050 Hz clip advances half a sample per output sample */
            snd_step = ((uint32_t)rate << 16) / 44100u;
            return 1;
        }
    }
    return 0;
}

static void snd_play_id(uint16_t id)
{
    if (!snd_locate(id))
        return;
    snd_idx = 0;
    snd_frac = 0;
    snd_on = 1;
}
static void snd_stop(void) { snd_on = 0; }

static float snd_next(void)   /* next sample at the output rate, or 0 when done */
{
    float a, b, f;
    uint32_t tot;
    if (!snd_on)
        return 0.0f;
    if (snd_idx >= snd_len) {
        snd_on = 0;
        return 0.0f;
    }
    a = (float)snd_data[snd_idx] - 128.0f;
    b = (snd_idx + 1 < snd_len) ? (float)snd_data[snd_idx + 1] - 128.0f : a;
    f = (float)snd_frac / 65536.0f;
    tot = snd_frac + snd_step;
    snd_idx += tot >> 16;
    snd_frac = tot & 0xFFFFu;
    return (a + (b - a) * f) / 128.0f * 0.75f;
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, v;
    float gain = (float)master_q12 / 4096.0f * 8300000.0f;   /* sound is always on; MASTER is the volume */
    float atk = 0.001f * fm_exp2f(25.0f / 12.0f);              /* fixed ATTACK (the old default 25 => ~1 ms) */
    float rlim = 0.05f + 50.0f * 0.0145f;                      /* fixed RELEASE (the old default 50 => 0.775 s) */
    for (i = 0; i < n; i++) {
        float s = 0.0f;
        for (v = 0; v < NVOICES; v++) {
            voice_t *vc = &voices[v];
            float env, w, vs = 0.0f, rel = 1.0f, ramp;
            if (vc->note < 0) {
                vc->level *= 0.999f;
                continue;
            }
            vc->age += 1.0f / 44100.0f;
            if (vc->releasing) {           /* ring-out: RELEASE sets the audible tail */
                float ra = vc->age - vc->rel_age;
                if (ra > rlim * 1.5f) { vc->note = -1; vc->releasing = 0; continue; }
                rel = fm_exp2f(-ra * 3.0f / rlim);
            }
            ramp = vc->age < atk ? vc->age / atk : 1.0f;
            if (ramp < 0.0f) ramp = 0.0f;
            /* Fixed envelope (the old defaults 25/50): attack, decay and
             * release are constant, with no user page. */
            {
                float afact = 0.75f;    /* 0.25 + 25/50 */
                float dfact = 1.25f;    /* 0.25 + 50/50 */
                float a_age = vc->age / (afact > 0.0f ? afact : 1.0f);
                float d_age = vc->age * dfact;
                (void)a_age;
                w = note_inc(vc->note);
                vc->phase += w;
                if (vc->phase >= 1.0f) vc->phase -= 1.0f;
                vc->mphase += w * 3.5f;
                if (vc->mphase >= 1.0f) vc->mphase -= 1.0f;
                switch (vc->timbre) {
                case TIM_BELLS: {              /* softened: low FM index, quiet bright partial, gentle click */
                    float idx = 1.2f * fm_exp2f(-d_age * 4.0f);
                    env = vc->held ? (0.25f + 0.75f * fm_exp2f(-d_age * 4.0f)) : fm_exp2f(-d_age * 2.2f);
                    vs += fm_sinf((vc->phase + idx * fm_sinf(vc->mphase * 6.2831853f)) * 6.2831853f) * env * 0.32f * vc->vel * ramp;
                    vs += fm_sinf(vc->phase * 2.76f * 6.2831853f) * fm_exp2f(-d_age * 8.0f) * 0.08f * vc->vel * ramp;
                    vs += fm_sinf(vc->phase * 5.4f * 6.2831853f) * fm_exp2f(-vc->age * 250.0f) * 0.08f * vc->vel * ramp;
                    break;
                }
                case TIM_CHOIR: {              /* 3 detuned sines + fifth, slow attack, vibrato */
                    float vib = 0.0025f * fm_sinf(vc->age * 31.4f);   /* ~5 Hz vibrato */
                    float ph = vc->phase + vib;
                    vs += (fm_sinf(ph * 6.2831853f)
                          + 0.7f * fm_sinf((ph + vc->age * 0.8f) * 6.2831853f)
                          + 0.5f * fm_sinf((ph - vc->age * 0.6f) * 6.2831853f)
                          + 0.35f * fm_sinf(ph * 1.5f * 6.2831853f)) * ramp * 0.20f * vc->vel;
                    break;
                }
                case TIM_PIANO:
                case TIM_FLUTE:
                case TIM_SAX: {
                    /* The sampled piano/flute/sax (clips 5/6/7, C4/C5/A#3 bases):
                     * pitch-shifted readout with the fixed envelope. Missing
                     * clip = the bells fallback below. */
                    if (!vc->s_data) {
                        float idx = 1.2f * fm_exp2f(-d_age * 4.0f);
                        env = vc->held ? (0.25f + 0.75f * fm_exp2f(-d_age * 4.0f)) : fm_exp2f(-d_age * 2.2f);
                        vs += fm_sinf((vc->phase + idx * fm_sinf(vc->mphase * 6.2831853f)) * 6.2831853f) * env * 0.32f * vc->vel * ramp;
                        break;
                    }
                    {
                        float dec = fm_exp2f(-vc->age * (0.5f + dfact * 0.5f));
                        uint32_t step = vc->s_step;
                        float samp = 0.0f;
                        if (vc->s_idx < vc->s_len) {
                            float u8a = (float)vc->s_data[vc->s_idx] - 128.0f;
                            float u8b = (vc->s_idx + 1 < vc->s_len) ? (float)vc->s_data[vc->s_idx + 1] - 128.0f : u8a;
                            float f = (float)vc->s_frac / 65536.0f;
                            uint32_t tot = vc->s_frac + step;
                            vc->s_idx += tot >> 16;
                            vc->s_frac = tot & 0xFFFFu;
                            samp = ((u8a + (u8b - u8a) * f) / 128.0f) * 0.65f;
                        } else {
                            vc->s_idx = vc->s_len;   /* clip over: the envelope tail is all that is left */
                        }
                        vs += samp * ramp * dec * vc->vel;
                    }
                    break;
                }
                default: {                     /* kazoo: triangle-ish, detuned, wobble, harmonic bite */
                    float p = vc->phase + 0.004f * fm_sinf(vc->age * 40.0f);
                    vs += (4.0f * fm_fabsf(p - 0.5f) - 1.0f) * ramp * 0.38f * vc->vel;
                    vs += fm_sinf(p * 3.0f * 6.2831853f) * ramp * 0.10f * vc->vel;
                    break;
                }
                }
            }
            s += vs * rel;
        }
        s += snd_next();                   /* recorded sounds (Santa/SFX), through the same gain */
        s = fm_tanhf(s * 2.2f) * 0.62f;    /* soft-clip: louder without harsh clipping when voices stack */
        gain_now += (gain - gain_now) * 0.004f;   /* ~6 ms: MASTER answers at once, no click */
        s *= gain_now;
        lr[2 * i] = (int32_t)s;
        lr[2 * i + 1] = (int32_t)s;
    }
}

void app_panic(void)
{
    uint32_t i;
    snd_stop();                          /* a resource write may be coming: the region stops sounding */
    for (i = 0; i < NVOICES; i++)
        voices[i].note = -1;
}

/* --------------------------------------------------------------- settings --- */
static void settings_defaults(void)
{
    uint32_t i;
    set.magic = SET_MAGIC;
    for (i = 0; i < NCAROLS; i++) { set.best_score[i] = 0; set.best_level[i] = 0; }
    set.last_w1 = 0; set.last_w2 = 0;
    set.sound_on = 1; set.leds_on = 1;
    set.rsv[0] = MODE_MEDIUM; set.rsv[1] = 0;
    set.snd = SND_PIANO;
    set.env_a = 25u; set.env_d = 50u; set.env_r = 50u;
    set.grain = 0u;
    set.staff = 0u;                      /* the staff stays off for existing players */
}
static int load_settings_v3_upgrade(void);  /* defined below: CCR3 -> CCR4 */
static int load_settings_v4_upgrade(void);  /* defined below: CCR4 -> CCR5 */
static int load_settings(void)
{
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC
        && set.sound_on <= 1u && set.leds_on <= 1u && set.last_w1 < NWORDS && set.last_w2 < NWORDS
        && set.rsv[0] < NMODES && set.rsv[1] <= 1u && set.snd < NSOUND && set.staff <= 1u
        && set.env_a <= 100u && set.env_d <= 100u && set.env_r <= 100u && set.grain <= 100u)
        return 1;
    if (load_settings_v4_upgrade())
        return 2;
    {
        settings_legacy_t old;
        if (plat_store_load(OBJ_SETTINGS, &old, sizeof old) == (int)sizeof old && old.magic == SET_MAGIC_OLD
            && old.last_w1 < NWORDS && old.last_w2 < NWORDS && old.rsv[0] < NMODES && old.rsv[1] <= 1u) {
            settings_defaults();
            set.best_score[0] = old.best_score[0];
            set.best_score[1] = old.best_score[1];
            set.best_score[2] = old.best_score[2];
            set.best_level[0] = old.best_level[0];
            set.best_level[1] = old.best_level[1];
            set.best_level[2] = old.best_level[2];
            set.last_w1 = old.last_w1; set.last_w2 = old.last_w2;
            set.sound_on = old.sound_on; set.leds_on = old.leds_on;
            set.rsv[0] = old.rsv[0]; set.rsv[1] = old.rsv[1];
            return 2;   /* upgraded: caller marks dirty so the new shape is saved soon */
        }
    }
    if (load_settings_v3_upgrade())
        return 2;
    settings_defaults();
    return 0;
}
static int load_settings_v4_upgrade(void)
{
    /* CCR4 (pre-staff): the scores, the mode, MIDI and the sound keep their
     * bytes; the staff byte comes in as the default (off). */
    settings_v4_t old;
    if (plat_store_load(OBJ_SETTINGS, &old, sizeof old) == (int)sizeof old && old.magic == SET_MAGIC_V4
        && old.sound_on <= 1u && old.leds_on <= 1u && old.last_w1 < NWORDS && old.last_w2 < NWORDS
        && old.rsv[0] < NMODES && old.rsv[1] <= 1u && old.snd < NSOUND
        && old.env_a <= 100u && old.env_d <= 100u && old.env_r <= 100u && old.grain <= 100u) {
        settings_defaults();
        set.best_score[0] = old.best_score[0];
        set.best_score[1] = old.best_score[1];
        set.best_score[2] = old.best_score[2];
        set.best_score[3] = old.best_score[3];
        set.best_level[0] = old.best_level[0];
        set.best_level[1] = old.best_level[1];
        set.best_level[2] = old.best_level[2];
        set.best_level[3] = old.best_level[3];
        set.last_w1 = old.last_w1; set.last_w2 = old.last_w2;
        set.sound_on = old.sound_on; set.leds_on = old.leds_on;
        set.rsv[0] = old.rsv[0]; set.rsv[1] = old.rsv[1];
        set.snd = old.snd;
        set.env_a = old.env_a; set.env_d = old.env_d; set.env_r = old.env_r;
        set.grain = old.grain;
        return 2;
    }
    return 0;
}
static int load_settings_v3_upgrade(void)
{
    /* CCR3 (3-carol): keep the three scores/levels and the sound pages. */
    settings_v3_t old;
    if (plat_store_load(OBJ_SETTINGS, &old, sizeof old) == (int)sizeof old && old.magic == SET_MAGIC_V3
        && old.sound_on <= 1u && old.leds_on <= 1u && old.last_w1 < NWORDS && old.last_w2 < NWORDS
        && old.rsv[0] < NMODES && old.rsv[1] <= 1u && old.snd < NSOUND
        && old.env_a <= 100u && old.env_d <= 100u && old.env_r <= 100u && old.grain <= 100u) {
        settings_defaults();
        set.best_score[0] = old.best_score[0];
        set.best_score[1] = old.best_score[1];
        set.best_score[2] = old.best_score[2];
        set.best_level[0] = old.best_level[0];
        set.best_level[1] = old.best_level[1];
        set.best_level[2] = old.best_level[2];
        set.last_w1 = old.last_w1; set.last_w2 = old.last_w2;
        set.sound_on = old.sound_on; set.leds_on = old.leds_on;
        set.rsv[0] = old.rsv[0]; set.rsv[1] = old.rsv[1];
        set.snd = old.snd;
        set.env_a = old.env_a; set.env_d = old.env_d; set.env_r = old.env_r;
        set.grain = old.grain;
        return 2;
    }
    return 0;
}
static int save_settings(void)
{
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) { set_dirty = 0; return 1; }
    return 0;
}

void app_init(void)
{
    int upgraded = load_settings();
    if (upgraded == 2)
        set_dirty = 1;
    voice_timbre = TIM_BELLS;
    {
        uint32_t i;
        for (i = 0; i < NVOICES; i++) {
            voices[i].note = -1;
            voices[i].level = 0.0f;
            voices[i].age = 0.0f;
        }
    }
}

/* the backup/restore hooks (device/backup.c): the saved objects are the
 * settings/scores; a backup is never refused, and a restore reloads. */
int app_backup_playing(void) { return 0; }
void app_backup_flush(void)
{
    if (set_dirty)
        save_settings();
}
void app_backup_hold(int on) { (void)on; }
void app_backup_restored(void)
{
    load_settings();
    set_dirty = 0;
}

/* --------------------------------------------------------------------- UI --- */
enum { ST_TITLE, ST_LISTEN, ST_PLAY, ST_GRUMP, ST_RESULT, ST_SONGDONE, ST_SOUND, ST_SCAN, NST };
#define STAFF_N 12                       /* staff entries at most; the oldest drops off */
typedef struct {
    int8_t step;                         /* the diatonic step above C4 */
    uint8_t sharp, miss;                 /* a black key, and a miss (in the crimson) */
    uint8_t start;                       /* the eighth it started at, since the turn began */
    uint8_t len;                         /* its length in eighths */
} staff_ent_t;
static struct {
    uint8_t state;
    uint8_t carol;
    uint16_t level;
    uint32_t score;
    uint8_t combo, misses, hits;
    uint8_t expect_idx;                  /* next expected note in the song prefix */
    uint32_t listen_at, listen_idx;      /* listen playback scheduling */
    uint32_t led_at;                     /* when the sounding listen note started (LED blink) */
    uint32_t grump_until;
    uint8_t grump_snd;                   /* Grump Mode: 1 = a recorded clip is playing, 0 = kazoo fallback */
    uint8_t grump_stage;                 /* Grump sound sequence: 0 = lose clip, 1 = jeer clip */
    uint8_t msg_kind;                    /* last announcement kind, for tests */
    uint8_t jingle_idx;                  /* celebration jingle progress (255 = done) */
    uint32_t jingle_at;
    float conf_x[40], conf_y[40], conf_vx[40], conf_vy[40];
    uint8_t conf_c[40], conf_life[40];
    uint32_t keys, btn;
    uint32_t key_good;                     /* held keys correct at press (YOUR TURN only; coloured dots) */
    /* the staff's notes, oldest first: what the demo and the player played */
    staff_ent_t st[STAFF_N];
    uint8_t st_n;
    uint8_t st_pos;                         /* the eighth the staff is at: where the next note starts */
    uint8_t qr_carol, qr_level;             /* the badge on the scan screen: the carol and the level */
    uint8_t qr_from;                        /* the screen the scan screen came from (PLAY goes back) */
    uint32_t songdone_at;                   /* when the song was finished: the celebration, then the badge */
    uint8_t redraw;
    uint32_t frame;
    char msg[24];                      /* last announcement, drawn as an overlay */
    uint32_t msg_until;
} ui;

static void text_cat(char *d, const char *s) { while (*d) d++; while (*s) *d++ = *s++; *d = 0; }
static void int_text(int32_t v, char *b)
{
    char t[12]; int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do { t[n++] = (char)('0' + u % 10u); } while ((u /= 10u) && n < 11);
    if (v < 0) *b++ = '-';
    while (n) *b++ = t[--n];
    *b = 0;
}
/* The spoken text for a state/page announcement. This goes to the reader
 * only: it is already on the screen as the page, so drawing it again as a
 * centred overlay would show it twice (the Jingle Bells / GRUMP MODE bug). */
static void sense_say(const char *a, const char *b)
{
    char t[64];
    t[0] = 0;
    if (a) text_cat(t, a);
    if (b && *b) { if (*t) text_cat(t, " "); text_cat(t, b); }
    if (t[0] && ann_on())
        ann_say(t, 0, ANN_NOW);
}

/* String equality without string.h (the device toolchain has no libc string
 * for app code): compare a few ASCII strings exactly. */
static int seq(const char *a, const char *b)
{
    if (a == b)
        return 1;
    if (!a || !b)
        return 0;
    while (*a && *b) {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }
    return !*a && !*b;
}

void app_say(const char *a, const char *b)
{
    sense_say(a, b);
    /* Control feedback also needs a screen confirmation the page does not
     * already show (toggles, encoder turns, SAVED, SOUND changes). MODE is
     * excluded: mode_label() draws it on every screen, so an overlay would
     * show it twice. STAFF is excluded for the same reason: the sound page
     * shows `SELECT: STAFF ON/OFF`, and the overlay sits on its title.
     * State announcements above stay silent for the same reason. */
    if (seq(a, "MIDI") || seq(a, "SOUND") ||
        seq(a, "SAVED") || seq(a, "SAVE FAILED")) {
        char t[64];
        t[0] = 0;
        if (a) text_cat(t, a);
        if (b && *b) { if (*t) text_cat(t, " "); text_cat(t, b); }
        if (t[0]) {
            uint32_t mi = 0;
            while (t[mi] && mi < sizeof ui.msg - 1u) { ui.msg[mi] = t[mi]; mi++; }
            ui.msg[mi] = 0;
            ui.msg_until = plat_ms() + 1300u;
        }
    } else {
        ui.msg_until = 0;
    }
    ui.redraw = 1;
}

/* The song is the carol's phrases concatenated; a level asks for a prefix
 * of it. The phrases are only how the data is stored. */
static uint8_t song_note(uint8_t idx) { return CAROLS[ui.carol][idx / PHRASE_LEN][idx % PHRASE_LEN]; }
static uint8_t level_len(void)
{
    uint32_t len = (uint32_t)ui.level * NOTES_PER_LEVEL;
    return (uint8_t)(len > NSONG_NOTES ? NSONG_NOTES : len);
}
/* How long demo note idx sounds before the next one: its CAROL_DUR eighths,
 * plus a breath after each 4-note phrase. */
static uint32_t song_dur_ms(uint8_t idx)
{
    uint32_t ms = (uint32_t)CAROL_DUR[ui.carol][idx] * EIGHTH_MS;
    if (idx % PHRASE_LEN == PHRASE_LEN - 1u)
        ms += PHRASE_GAP_MS;
    return ms;
}
/* Melody velocity with a phrase contour: phrase starts accented, long
 * landings louder, final whole note loudest, slight fall within a phrase.
 * A deterministic jitter (~±4) humanizes identical repeated samples. */
static uint32_t song_vel(uint8_t idx)
{
    uint32_t v = 88u;
    uint32_t d = CAROL_DUR[ui.carol][idx];
    if (idx % PHRASE_LEN == 0u) v += 18u;
    if (d >= 4u) v += 10u;
    if (d >= 8u) v += 4u;
    v -= (idx % PHRASE_LEN) * 2u;
    v = (uint32_t)((int32_t)v + (int32_t)((idx * 37u) % 9u) - 4);
    if (v < 84u) v = 84u;
    if (v > 118u) v = 118u;
    return v;
}

static int32_t sched_note = -1;          /* last game-triggered note, for its ring-out */

/* The staff's diary: note (a MIDI number on the 27 keys) in, as an entry.
 * The diatonic step above C4 places it on the treble staff; a black key's
 * step is its lower natural (F#3 draws on F3 with a sharp sign). A miss is
 * recorded too, in the crimson. len is the note's length in eighths (the
 * tune's own length, so the staff writes the rhythm). */
static void staff_note_add(int32_t note, uint8_t miss, uint8_t len)
{
    uint32_t i;
    int32_t oct, pc;
    static const int32_t DIA[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    if (note < 53 || note > 53 + (int32_t)NKEYS - 1)
        return;
    oct = note / 12 - 1;
    pc = note - (oct + 1) * 12;
    for (i = 0; i + 1 < STAFF_N; i++)   /* drop the oldest, add at the end */
        ui.st[i] = ui.st[i + 1];
    {
        staff_ent_t *e = &ui.st[STAFF_N - 1];
        e->step = (int8_t)((oct - 4) * 7 + DIA[pc]);
        e->sharp = (uint8_t)(pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        e->miss = miss ? 1u : 0u;
        e->start = ui.st_pos;
        e->len = len ? len : 1u;
        ui.st_pos = (uint8_t)(ui.st_pos + e->len);
    }
    if (ui.st_n < STAFF_N)
        ui.st_n++;
}

static void sched_on(int32_t note, uint8_t timbre, uint32_t vel)
{
    if (sched_note >= 0)
        voice_release(sched_note);       /* ring the previous note out, don't cut it */
    sched_note = note;
    voice_on(note, vel, timbre, 1);      /* held: sustain for the note's full length */
}
static void sched_off(void)
{
    if (sched_note >= 0) {
        voice_release(sched_note);       /* last demo note rings out into the next screen */
        sched_note = -1;
    }
}

static void start_listen(void)
{
    sched_off();                         /* a held demo note must not hum on under a restart */
    ui.state = ST_LISTEN;
    ui.listen_idx = 0;
    ui.listen_at = plat_ms() + 400u;
    ui.expect_idx = 0;
    ui.hits = 0; ui.misses = 0; ui.combo = 0;
    ui.key_good = 0;                       /* a new turn: any still-held key is judged again on its next press */
    ui.st_n = 0; ui.st_pos = 0;             /* the staff clears: the next entries are this turn's */
    ui.redraw = 1;
    app_say("LISTEN", CAROL_NAME[ui.carol]);
}

static void start_play(void)
{
    sched_off();                         /* the last listened-to note rings out, not under the retry screen */
    ui.state = ST_PLAY;
    ui.expect_idx = 0;
    ui.hits = 0; ui.misses = 0; ui.combo = 0;
    ui.key_good = 0;                       /* a new turn: any still-held key is judged again on its next press */
    ui.st_n = 0; ui.st_pos = 0;            /* the staff clears: it writes the player's turn */
    ui.redraw = 1;
    app_say("YOUR TURN", "");
}

/* The second Grump sound: the jeer clip, or the kazoo riff when it is missing. */
static void grump_next(void)
{
    snd_play_id(SND_CLIP_GRUMP);
    ui.grump_stage = 1;
    ui.grump_snd = snd_on;
    ui.grump_until = ui.grump_snd ? plat_ms() + (snd_len * 1000u / snd_rate) + 300u
                                 : plat_ms() + 2600u;
}

static void enter_grump(void)
{
    sched_off();
    ui.state = ST_GRUMP;
    ui.key_good = 0;                       /* leaving YOUR TURN: held judgements end with the turn */
    ui.listen_idx = 0;
    ui.listen_at = plat_ms() + 300u;
    snd_play_id(SND_CLIP_LOSE);          /* the lose sting plays first... */
    ui.grump_stage = 0;
    ui.grump_snd = snd_on;
    if (ui.grump_snd)   /* ...for about its own length, then the jeer (grump_next) */
        ui.grump_until = plat_ms() + (snd_len * 1000u / snd_rate) + 300u;
    else                /* no lose clip: straight to the jeer/kazoo */
        grump_next();
    ui.redraw = 1;
    app_say("GRUMP MODE", "TRY AGAIN");
}

static void start_celebration(void)
{
    snd_play_id(SND_CLIP_REWARD);        /* the recorded reward jingle, where the arpeggio was */
    /* 255 = done: no procedural jingle over the clip; 0 = clip missing, fall back to the arpeggio */
    ui.jingle_idx = snd_on ? 255u : 0u;
    ui.jingle_at = plat_ms() + 150u;
    {
        uint32_t i;
        for (i = 0; i < 40u; i++) {
            ui.conf_x[i] = (float)((i * 67u + 13u) % 240u);
            ui.conf_y[i] = -20.0f + (float)((i * 31u) % 60u);
            ui.conf_vx[i] = ((float)((i * 17u) % 20u) - 10.0f) * 0.05f;
            ui.conf_vy[i] = 1.0f + (float)((i * 13u) % 20u) * 0.1f;
            ui.conf_c[i] = (uint8_t)(i % 5u);
            ui.conf_life[i] = 90;
        }
    }
}

static void finish_level(void)
{
    sched_off();                         /* the last note rings out into the celebration */
    ui.key_good = 0;                     /* leaving YOUR TURN: held judgements end with the turn */
    uint32_t bonus = 250u;
    ui.score += bonus;
    if (ui.score > set.best_score[ui.carol]) {
        set.best_score[ui.carol] = ui.score;
        if (ui.level > set.best_level[ui.carol]) set.best_level[ui.carol] = ui.level;
        set_dirty = 1;
    }
    set_dirty = 1;
    start_celebration();
    if (ui.level >= NLEVELS) {
        ui.state = ST_SONGDONE;
        ui.songdone_at = plat_ms();
        ui.redraw = 1;
        app_say("SONG COMPLETE", CAROL_NAME[ui.carol]);
    } else {
        ui.state = ST_RESULT;
        ui.redraw = 1;
        app_say("LEVEL", "CLEAR");
    }
}

static void next_level(void)
{
    if (ui.level < NLEVELS)
        ui.level++;
    start_listen();
}

static void next_song(void)
{
    ui.carol = (uint8_t)((ui.carol + 1) % NCAROLS);
    ui.level = 1;
    start_listen();
}

static void key_hit(uint32_t k)
{
    int32_t note = 53 + (int32_t)k;
    uint32_t vel = touch_vel((uint8_t)k);
    midi_note_on(note, vel);
    if (ui.state != ST_PLAY) {
        ui.key_good &= ~(1u << k);                     /* no judgement outside YOUR TURN */
        voice_on(note, vel, snd_timbre(snd_id()), 1);   /* free play / sound pages: the selected sound */
        return;
    }
    if (note == song_note(ui.expect_idx)) {
        ui.key_good |= (1u << k);                      /* correct at press: green dot until release */
        voice_on(note, vel, snd_timbre(snd_id()), 1);   /* your turn sounds like the demo */
        staff_note_add(note, 0, CAROL_DUR[ui.carol][ui.expect_idx]);   /* the correct head, in ink */
        ui.score += 100u;
        ui.combo++;
        ui.hits++;
        ui.expect_idx++;
        if (ui.expect_idx >= level_len())
            finish_level();
    } else {
        ui.key_good &= ~(1u << k);                     /* a miss at press: red dot until release */
        voice_on(note, vel, TIM_KAZOO, 1);
        staff_note_add(note, 1, 1);                    /* the miss: a crimson head, an eighth long */
        ui.misses++;
        ui.combo = 0;
        app_say("MISS", "");
        if (ui.misses >= 3)
            enter_grump();
    }
    ui.redraw = 1;
}

/* The badge: the QR code for a carol and a level. It comes from a screen
 * (from) the player can go back to with PLAY. */
static void show_badge(uint8_t from, uint8_t carol, uint8_t level)
{
    ui.qr_from = from;
    ui.qr_carol = carol;
    ui.qr_level = level;
    ui.state = ST_SCAN;
    ui.redraw = 1;
    app_say("PASS IT ON", CAROL_NAME[carol]);
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k;
    int32_t e;
    touch_poll();
    keys = touch_keys();
    if (snd_on && ((keys & ~ui.keys) || (btn & ~ui.btn)))
        snd_stop();                      /* any fresh press skips the intro clip */
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit))
            key_hit(k);
        else if (!(keys & bit) && (ui.keys & bit)) {
            ui.key_good &= ~bit;                   /* the dot goes with the key */
            voice_release(53 + (int32_t)k);
            midi_note_off(53 + (int32_t)k);
        }
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (k == B_PLAY) {
            if (ui.state == ST_TITLE) { ui.level = 1; start_listen(); }
            else if (ui.state == ST_LISTEN) start_play();
            else if (ui.state == ST_RESULT) next_level();
            else if (ui.state == ST_SONGDONE) show_badge(ST_SONGDONE, ui.carol, (uint8_t)NLEVELS);
            else if (ui.state == ST_GRUMP) start_play();
            else if (ui.state == ST_PLAY) start_listen();
            else if (ui.state == ST_SOUND) {
                ui.state = ST_TITLE; ui.redraw = 1; app_say(APP_NAME, "");
            } else if (ui.state == ST_SCAN) {
                if (ui.qr_from == ST_SONGDONE) {
                    next_song();                 /* the trophy of the song done: on to the next one */
                } else {
                    ui.state = ui.qr_from; ui.redraw = 1;
                    app_say(ui.qr_from == ST_TITLE ? APP_NAME : "", "");
                }
            }
        } else if (k == B_EDIT) {
            sched_off();
            ui.state = ST_SOUND; ui.redraw = 1; app_say("SOUND", SND_NAME[snd_id()]);
        } else if (k == B_HOME) {
            sched_off();
            ui.state = ST_TITLE; ui.redraw = 1; app_say(APP_NAME, "");
        } else if (k == B_REC) {
            if (ui.state == ST_PLAY || ui.state == ST_LISTEN)
                start_listen();            /* REC replays the level */
            else if (ui.state == ST_TITLE) { ui.level = 1; start_listen(); }
        } else if (k == B_SAVE) {
            app_say(save_settings() ? "SAVED" : "SAVE FAILED", "");
        } else if (k == B_LFO) {
            /* LFO is otherwise unassigned: it shows the badge, the QR code that
             * opens the page on the phone. On the title it is the song's best
             * level, after a level the level just cleared. */
            if (ui.state == ST_TITLE || ui.state == ST_RESULT) {
                uint16_t lv = (ui.state == ST_TITLE) ? set.best_level[ui.carol] : ui.level;
                show_badge(ui.state, ui.carol, (uint8_t)(lv ? lv : 1u));
            }
        } else if (k == B_SEQ && ui.state == ST_TITLE) {
            snd_stop();                    /* SEQ replays from the top, even mid-intro */
            snd_play_id(SND_CLIP_INTRO); /* Santa's intro voice, from the title screen only */
        } else if (k == B_GLO) {
            set.rsv[0] = (uint8_t)((set.rsv[0] + 1u) % NMODES);
            set_dirty = 1;
            app_say("MODE", MODE_NAME[play_mode()]);
        } else if (k == B_FX) {
            uint32_t hk;
            if (midi_on()) {               /* turning off: release what a DAW is holding */
                for (hk = 0; hk < NKEYS; hk++)
                    if (ui.keys & (1u << hk))
                        midi_note_off(53 + (int32_t)hk);
                set.rsv[1] = 1u;
                app_say("MIDI", "OFF");
            } else {
                set.rsv[1] = 0u;
                app_say("MIDI", "ON");
            }
            set_dirty = 1;
        }
        /* The other unassigned panels (OCT etc.) do nothing: no accidental change. */
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0)
            continue;
        ui.redraw = 1;
        if (ui.state == ST_TITLE && k == EN_SELECT) {
            ui.carol = (uint8_t)((ui.carol + e + NCAROLS) % NCAROLS);
            app_say(CAROL_NAME[ui.carol], "");
        } else if (ui.state == ST_SOUND && k == EN_SELECT) {
            set.staff = set.staff ? 0u : 1u;   /* the grown-ups' staff: any SELECT turn toggles it */
            set_dirty = 1;
            app_say("STAFF", set.staff ? "ON" : "OFF");
        } else if (ui.state == ST_SOUND && k == EN_PRESET) {
            int32_t n = (int32_t)set.snd + e;
            n = ((n % NSOUND) + NSOUND) % NSOUND;
            set.snd = (uint8_t)n; set_dirty = 1;
            app_say("SOUND", SND_NAME[snd_id()]);
        }
    }
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);   /* the buttons' ambient glow is always on; the mode only changes guidance */
    {
        uint32_t led_keys = keys, mid_keys = 0;
        /* LED walkthrough: the sounding note's key blinks once per note —
         * on for the first 150 ms of its 450 ms slot, then off until the
         * next note. EASY and MEDIUM keep this; HARD has no guidance.
         * listen_idx points past the sounding note, hence -1. GRUMP's
         * replay follows the same rules as LISTEN. */
        if (play_mode() != MODE_HARD && ui.listen_idx > 0
            && (ui.state == ST_LISTEN || ui.state == ST_GRUMP)
            && plat_ms() - ui.led_at < 150u) {
            int32_t nk = (int32_t)song_note((uint8_t)(ui.listen_idx - 1)) - 53;
            if (nk >= 0 && nk < (int32_t)NKEYS)
                led_keys |= 1u << nk;
        }
        plat_leds(btn | (set_dirty ? 1u << B_SAVE : 0u), led_keys);
        /* EASY: the key to press next stays on at the mid level for the whole turn. */
        if (play_mode() == MODE_EASY && ui.state == ST_PLAY) {
            int32_t nk = (int32_t)song_note((uint8_t)ui.expect_idx) - 53;
            if (nk >= 0 && nk < (int32_t)NKEYS)
                mid_keys = 1u << nk;
        }
        plat_lights_mid(0, mid_keys);
    }
}

void app_input_only(void) { input(); }

static float ph_sin(uint32_t ph, float k);   /* defined with the sprites, below */

static void tick_game(void)
{
    uint32_t now = plat_ms();
    if (ui.state == ST_LISTEN) {
        if (ui.listen_idx < level_len() && now >= ui.listen_at) {
            uint8_t played = (uint8_t)ui.listen_idx;
            sched_on(song_note(played), snd_timbre(snd_id()), song_vel(played));
            staff_note_add(song_note(played), 0, CAROL_DUR[ui.carol][played]);   /* the demo's note lands on the staff as it sounds */
            ui.led_at = now;
            ui.listen_idx++;
            ui.listen_at = now + song_dur_ms(played);
            ui.redraw = 1;
        } else if (ui.listen_idx >= level_len() && now >= ui.listen_at) {
            start_play();
        }
    } else if (ui.state == ST_PLAY) {
        /* no timeout: idling here does nothing. Only a wrong key counts as a
         * miss (key_hit); when it was a timed window, an untouched keyboard
         * looped PLAY -> GRUMP -> PLAY forever. */
    } else if (ui.state == ST_RESULT || ui.state == ST_SONGDONE) {
        static const uint8_t JINGLE[7] = {60, 64, 67, 72, 76, 79, 84};   /* C E G C E G C' (soft, low); fallback when the reward clip is missing */
        uint32_t i;
        for (i = 0; i < 40u; i++) {
            if (ui.conf_life[i]) {
                ui.conf_x[i] += ui.conf_vx[i] + ph_sin(ui.frame + i, 0.05f) * 0.3f;
                ui.conf_y[i] += ui.conf_vy[i];
                if (ui.conf_y[i] > 240.0f) { ui.conf_y[i] = -10.0f; ui.conf_x[i] = (float)((i * 53u + ui.frame) % 240u); }
                ui.conf_life[i]--;
            }
        }
        if (ui.jingle_idx < 7u && now >= ui.jingle_at) {
            sched_on(JINGLE[ui.jingle_idx], TIM_PIANO, 100);   /* piano: the fallback when the reward clip is missing */
            ui.jingle_idx++;
            ui.jingle_at = now + 160u;
        } else if (ui.jingle_idx == 7u && now >= ui.jingle_at) {
            sched_off();
            ui.jingle_idx = 255u;
        }
        if (ui.state == ST_SONGDONE && now >= ui.songdone_at + 4000u) {
            show_badge(ST_SONGDONE, ui.carol, (uint8_t)NLEVELS);   /* the celebration, then the trophy */
        }
    } else if (ui.state == ST_GRUMP) {
        if (ui.grump_snd) {
            if (now >= ui.grump_until) {
                if (ui.grump_stage == 0)
                    grump_next();        /* lose clip done: the jeer follows */
                else
                    start_play();
            }
        } else if (ui.listen_idx < level_len() && now >= ui.listen_at) {
            uint8_t played = (uint8_t)ui.listen_idx;
            sched_on(song_note(played), TIM_KAZOO, song_vel(played));
            ui.led_at = now;                 /* the LED blink follows the GRUMP replay too */
            ui.listen_idx++;
            ui.listen_at = now + song_dur_ms(played);
            ui.redraw = 1;
        } else if (now >= ui.grump_until) {
            start_play();
        }
    }
}

static int draw_dy;
static int32_t ttext(int32_t x, int32_t y, const void *f, const char *str, uint16_t c)
{
    return cv_text(x, y - draw_dy, f, str, c);
}

/* --------------------------------------------------------------- sprites --- */
/* Procedural characters: filled circles / rects / lines, no bitmap assets.
 * phase is a frame counter (~60 fps); bob/wave come from it. */
static void rrect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { cv_rect(x, y - draw_dy, w, h, c); }
static void ln(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c) { cv_line(x0, y0 - draw_dy, x1, y1 - draw_dy, c); }
static void ball(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t half = (int32_t)fm_sqrtf((float)(r * r - dy * dy));
        rrect(cx - half, cy + dy, half * 2 + 1, 1, c);
    }
}
static float ph_sin(uint32_t ph, float k) { return fm_sinf((float)(ph % 628u) * 0.01f * k); }

static void draw_sprite(int32_t x, int32_t y, const uint8_t *px, const uint16_t *pal, int w, int h, int scale, int mirror)
{
    int32_t ry, rx;
    for (ry = 0; ry < h; ry++) {
        rx = 0;
        while (rx < w) {
            uint8_t v = px[ry * w + rx];
            if (v == 0) { rx++; continue; }
            int32_t rx0 = rx;
            while (rx < w && px[ry * w + rx] == v)
                rx++;
            /* mirrored: the run [rx0, rx) lands at the far end of the row */
            int32_t x0 = mirror ? x + (w - rx) * scale : x + rx0 * scale;
            rrect(x0, y + ry * scale, (rx - rx0) * scale, scale, pal[v]);
        }
    }
}
static void frosty(int32_t x, int32_t yb, uint32_t ph)
{
    /* Two poses (standing / mid-step). He does one short step, then rests
     * standing still (no bob) for a pause. The pauses cycle 3.5, 4, 5, 5.5 s
     * (~60 fps: 210, 240, 300, 330 frames) so he isn't waving constantly.
     * Both frames share the same cell and ground line, so he stays planted.
     * Stateless in ph, so both draw passes in a frame agree. */
    static const uint16_t PAUSE[4] = {210u, 240u, 300u, 330u};
    const uint32_t WAVE = 28u;               /* 14 frames per pose, one step */
    uint32_t period = 0, p, frame = 0;
    int32_t bob = 0;
    uint8_t i;
    for (i = 0; i < 4u; i++)
        period += WAVE + PAUSE[i];
    p = ph % period;
    for (i = 0; i < 4u; i++) {
        if (p < WAVE) {
            frame = (p / 14u) % FROSTY_NFRAMES;
            bob = (int32_t)(ph_sin(ph, 0.7f) * 5.0f);
            break;
        }
        p -= WAVE;
        if (p < PAUSE[i])
            break;                           /* resting: frame 0, bob 0 */
        p -= PAUSE[i];
    }
    draw_sprite(x - FROSTY_W / 2, yb - FROSTY_H + bob,
                FROSTY_PX + frame * (uint32_t)(FROSTY_W * FROSTY_H),
                FROSTY_PAL, FROSTY_W, FROSTY_H, 1, 0);
}
static void santa_spr(int32_t x, int32_t yb, uint32_t ph, int celebrate)
{
    /* 4-frame arm-raising wave; faster when celebrating. All frames share the
     * same cell and ground line, so bottom-anchoring keeps him planted. */
    int32_t bob = (int32_t)(ph_sin(ph, celebrate ? 2.0f : 1.0f) * (celebrate ? 6.0f : 2.0f));
    uint32_t frame = (ph / (celebrate ? 9u : 16u)) % SANTA_NFRAMES;
    draw_sprite(x - SANTA_W / 2, yb - SANTA_H + bob,
                SANTA_PX + frame * (uint32_t)(SANTA_W * SANTA_H),
                SANTA_PAL, SANTA_W, SANTA_H, 1, 0);
}
static void grump_spr(int32_t x, int32_t yb, uint32_t ph)
{
    /* 4-frame walk cycle (~133 ms per pose at ~60 fps). All frames share the
     * same cell and ground line, so bottom-anchoring keeps him planted. */
    uint32_t frame = (ph / 8u) % GRUMP_NFRAMES;
    draw_sprite(x - GRUMP_W / 2, yb - GRUMP_H,
                GRUMP_PX + frame * (uint32_t)(GRUMP_W * GRUMP_H),
                GRUMP_PAL, GRUMP_W, GRUMP_H, 1, 0);
}
static void snowfall(uint32_t ph)
{
    /* 24 flakes, two speeds, two sizes, drifting sideways; on every screen */
    uint32_t i;
    for (i = 0; i < 24u; i++) {
        int32_t x = (int32_t)((i * 97u + ph / (i & 1u ? 1u : 2u)) % 240u);
        int32_t y = (int32_t)((i * 61u + ph * (i & 1u ? 1u : 2u)) % 240u);
        int32_t sz = (i % 3u) ? 1 : 2;
        rrect(x, y, sz, sz, (i % 4u) ? C_GRAY : C_WHITE);
    }
}

/* The dusk sky the game sits in: ten bands from deep blue at the top to a
 * warm gold at the horizon, with the first stars out in the dark blue. The
 * palette is the same one the art (art/palette.py) is drawn in, so the
 * generated sprites sit in the sky rather than on it. Fixed in x and in the
 * stars' twinkle (ph), so both draw passes agree. */
#define SKY_BANDS 10
#define SKY_BAND 24
static const uint16_t SKY_COL[SKY_BANDS] = {
    RGB(14, 22, 56),   RGB(24, 36, 86),   RGB(38, 52, 116),  RGB(54, 68, 138),
    RGB(74, 84, 154),  RGB(100, 96, 162), RGB(128, 106, 160), RGB(156, 120, 154),
    RGB(184, 138, 148), RGB(212, 160, 140),
};
static void sky(uint32_t ph)
{
    uint32_t i;
    for (i = 0; i < SKY_BANDS; i++)
        rrect(0, (int32_t)i * SKY_BAND, 240, SKY_BAND, SKY_COL[i]);
    /* 18 stars in the top third, each on its own slow twinkle */
    for (i = 0; i < 18u; i++) {
        int32_t x = (int32_t)((i * 71u + 13u) % 236u);
        int32_t y = (int32_t)((i * 29u + 7u) % 70u);
        if (((ph / 90u) + i) % 5u == 0u)
            continue;                   /* the slow blink: off for a while */
        rrect(x, y, 1, 1, C_WHITE);
    }
}

/* The far treeline: the firs on the ridge behind the snow, so the horizon is
 * not empty. The firs are sprites (art/pine.png, art/fir_small.png and
 * art/fir_tiny.png, from art/gen_art.py); the places are fixed, so both draw
 * passes agree. The firs that would stand behind the staff are left off:
 * staff_x is the x the staff's ink reaches (0 for none), so the staff is
 * written on the sky and not lost in the trees. */
#define PINE_COL RGB(26, 30, 66)
/* The wood on the ridge: three shapes in six sizes, some of them facing the
 * other way, so the treeline is a wood and not a row of twins. */
typedef struct {
    int16_t x;                           /* where it stands */
    int16_t w, h;                        /* its size */
    const uint8_t *px;                   /* its pixels */
    const uint16_t *pal;
    uint8_t mirror;                      /* 1: its lit side faces the other way */
} fir_t;
static const fir_t FIRS[] = {
    {  2, FIR_TINY_W,  FIR_TINY_H,  FIR_TINY_PX,  FIR_TINY_PAL,  0 },
    { 24, PINE_W,     PINE_H,     PINE_PX,     PINE_PAL,     1 },
    { 56, FIR_SMALL_W, FIR_SMALL_H, FIR_SMALL_PX, FIR_SMALL_PAL, 0 },
    { 82, FIR_TINY_W,  FIR_TINY_H,  FIR_TINY_PX,  FIR_TINY_PAL,  1 },
    {106, PINE_W,     PINE_H,     PINE_PX,     PINE_PAL,     0 },
    {138, FIR_SMALL_W, FIR_SMALL_H, FIR_SMALL_PX, FIR_SMALL_PAL, 1 },
    {166, FIR_TINY_W,  FIR_TINY_H,  FIR_TINY_PX,  FIR_TINY_PAL,  0 },
    {188, PINE_W,     PINE_H,     PINE_PX,     PINE_PAL,     1 },
};
static void treeline(int32_t surface, uint32_t ph, int32_t staff_x)
{
    /* the ridge: a dark band with a bumpy top, so it is not a ruler */
    static const int8_t RIDGE[8] = {0, 1, 2, 2, 1, 0, 0, 1};
    uint32_t i;
    (void)ph;
    for (i = 0; i < 40u; i++) {
        int32_t x = (int32_t)i * 6;
        int32_t top = surface - 8 + RIDGE[i % 8u];
        rrect(x, top, 6, surface + 8 - top, PINE_COL);
    }
    for (i = 0; i < sizeof FIRS / sizeof FIRS[0]; i++) {   /* the firs on it */
        const fir_t *f = &FIRS[i];
        if (f->x < staff_x)
            continue;                                   /* none behind the staff */
        draw_sprite(f->x, surface - 6 - f->h, f->px, f->pal, f->w, f->h, 1, f->mirror);
    }
}

/* Shallow snow ground under whoever is standing on this screen: mostly-white
 * drifts with a soft, gently rolling top edge (6 px columns whose height
 * changes by at most 2 px per column, so no sawtooth), a shaded lower band,
 * and a few slow twinkles. surface is the baseline top edge, a few px above
 * the feet (yb = surface + 6 in draw_content), so the character stands in
 * it. The ground fills to the bottom of the screen. Deterministic in x
 * (and ph for the twinkles), so both draw passes agree. */
static void snow_ground(int32_t surface, uint32_t ph)
{
    /* Rolling profile, one full hill per 48 px; neighbouring columns differ
     * by at most 2 px, which reads as soft drifts rather than a jagged edge. */
    static const int8_t HILL[8] = {1, 2, 4, 6, 7, 6, 4, 2};
    const uint16_t snow = RGB(238, 232, 238);
    const uint16_t shade = RGB(176, 166, 200);
    int32_t x;
    uint32_t k;
    {
        int32_t shade_top = surface + (240 - surface) * 3 / 4;
        for (x = 0; x < 240; x += 6) {
            int32_t top = surface - HILL[(x / 6) % 8];
            rrect(x, top, 6, 240 - top, snow);
            rrect(x, top, 6, 1, C_WHITE);
            rrect(x, shade_top, 6, 240 - shade_top, shade);
        }
    }
    for (k = 0; k < 8u; k++) {
        int32_t sx = (int32_t)((k * 47u + 9u) % 236u);
        int32_t sy = surface + 2 + (int32_t)((k * 3u) % 10u);
        if (((ph / 40u) + k) % 6u == 0u)
            rrect(sx, sy, 1, 1, C_WHITE);
    }
}

static void draw_confetti(void)
{
    /* confetti over the scene, under the text */
    static const uint16_t CONF[5] = {0xF800u, 0x07E0u, 0xFFE0u, 0xFFFFu, 0x07FFu};
    uint32_t i;
    for (i = 0; i < 40u; i++)
        if (ui.conf_life[i])
            rrect((int32_t)ui.conf_x[i], (int32_t)ui.conf_y[i], 3, 2, CONF[ui.conf_c[i] % 5u]);
}

/* Battery: a lightbulb by the title whose glow reads at a glance —
 * red when low (0), orange at medium (1-2), green when high (3, or USB-powered).
 * plat_battery's four steps map to the glow's radius; the empty step still
 * shows a short red stub so low is visible, and -1 (not read yet) draws
 * the bare outline only. */
static void battery_meter(int32_t x, int32_t y)
{
    int32_t cx = x + 6, cy = y + 6;
    int raw = plat_battery(), lvl = raw;
    uint16_t col;
    if (lvl < 0)
        lvl = 0;
    if (lvl > 3)
        lvl = 3;                                    /* 4: USB-powered, treat as full */
    col = (lvl == 0) ? RGB(232, 76, 60) : (lvl <= 2) ? RGB(232, 136, 30) : RGB(80, 220, 120);
    ball(cx, cy, 6, C_GRAY);                        /* the bulb outline */
    ball(cx, cy, 4, C_BLACK);
    if (raw >= 0) {
        int32_t r = (lvl == 0) ? 1 : 2 + lvl;       /* the glow grows with the charge */
        ball(cx, cy, r, col);
    }
    rrect(cx - 3, cy + 6, 6, 1, C_GRAY);            /* the bulb base */
    rrect(cx - 3, cy + 8, 6, 1, C_GRAY);
    rrect(cx - 2, cy + 10, 4, 1, C_GRAY);
}

/* The panels (the boxes and the note modal) are dark blue, not black: they
 * sit on the dusk sky like glass over the picture, and the white border
 * around them keeps the text readable. */
#define C_PANEL RGB(10, 14, 38)
#define C_INK   RGB(10, 14, 38)      /* the ink the staff is written in: the same near-black */

/* The grown-ups' staff (toggled with SELECT on the SELECT SOUND page, kept
 * over power-off): a treble clef and five lines that write the notes the demo
 * and the player played, with their real values, in ink on the dusk sky
 * under the grid boxes and clear of Frosty and Santa (who stand from x=160
 * down). There is no panel behind it: the firs on the ridge step aside
 * instead (see treeline), so the ink is written on the sky.
 *
 * The staff is a bar of 4/4 (eight eighths) that scrolls: the notes stay
 * until a bar has passed and the oldest leaves at the left. A note has the
 * head (filled for an eighth or a quarter, hollow for a half or a whole), the
 * stem (up or down, from the head's other side), the flag of an eighth, the
 * dot of a dotted quarter or a dotted half, a sharp before a black key's
 * head, ledger lines under a note below the staff, and the bar lines every
 * eight eighths. The values are the carols' own lengths (CAROL_DUR, in
 * eighths): the demo's notes are the tune's, and your notes are the ones you
 * were asked to play.
 * Step s = diatonic steps above C4; the head row of s is:
 *   y(s) = 192 - (s - 2) * 3      (E4, the bottom line, at 192; 3 px a step) */
#define STAFF_X0 40                     /* the lines' left end, right of the clef */
#define STAFF_X1 152                    /* their right end */
#define STAFF_COL 14                    /* an eighth's width on the staff */
#define STAFF_COLS 8                   /* the eighths the staff shows: a bar of 4/4 */
#define STAFF_TOP 168                   /* the top line */
#define STAFF_LINE 6                    /* the lines' spacing */
#define STAFF_STEM 21                   /* the stem's length: three and a half spaces */
#define STAFF_CLEF_X 11                 /* the clef's corner: its coil sits on the G4 line */
#define STAFF_CLEF_Y 159

/* The staff's colours: it is written in ink on the dusk sky, with no panel
 * behind it, so the notes and the lines are the game's near-black and a miss
 * is a deep crimson (a bright red cannot read on the pink sky). */
#define C_STAFF C_INK                    /* the five lines and the ledger lines */
#define C_BAR C_INK                      /* the bar lines */
#define C_MISS RGB(120, 10, 30)          /* a miss: a deep crimson, so it reads on the sky */

static int32_t staff_y(int32_t step) { return 192 - (step - 2) * 3; }
/* the note head: an oval that leans to the right, in five rows (5, 7, 9, 7
 * and 5 pixels wide), filled for an eighth or a quarter, a ring for a half,
 * a wider ring for a whole */
static void staff_head(int32_t cx, int32_t cy, uint16_t c, int hollow, int whole)
{
    static const int8_t W[5] = {2, 3, 4, 3, 2};    /* the half width a row */
    static const int8_t S[5] = {1, 0, 0, 0, -1};   /* the lean: the row's shift */
    int32_t i;
    for (i = 0; i < 5; i++) {
        int32_t dy = i - 2;
        int32_t w = whole ? W[i] + 1 : W[i];
        int32_t x = cx - w + S[i];
        if (hollow) {
            if (i == 0 || i == 4)
                rrect(x, cy + dy, w * 2 + 1, 1, c);
            else {
                rrect(x, cy + dy, 1, 1, c);
                rrect(x + w * 2, cy + dy, 1, 1, c);
            }
        } else
            rrect(x, cy + dy, w * 2 + 1, 1, c);
    }
}
/* the sharp sign, four strokes: it stands before the head, in its own space */
static void staff_sharp(int32_t cx, int32_t cy, uint16_t c)
{
    ln(cx - 2, cy - 3, cx - 2, cy + 3, c);
    ln(cx + 1, cy - 4, cx + 1, cy + 2, c);
    ln(cx - 3, cy - 1, cx + 2, cy - 2, c);
    ln(cx - 3, cy + 1, cx + 2, cy, c);
}
/* the flag of an eighth, hanging off the stem's end: a curve that tapers */
static void staff_flag(int32_t sx, int32_t sy, int up, uint16_t c)
{
    if (up) {
        rrect(sx + 1, sy + 1, 2, 3, c);
        rrect(sx + 2, sy + 3, 3, 2, c);
        rrect(sx + 3, sy + 5, 2, 2, c);
    } else {
        rrect(sx - 3, sy - 4, 2, 3, c);
        rrect(sx - 4, sy - 6, 3, 2, c);
        rrect(sx - 5, sy - 8, 2, 2, c);
    }
}
static void draw_staff(void)
{
    uint32_t i;
    int32_t view = (int32_t)ui.st_pos - STAFF_COLS;   /* the bar's left end, in eighths */
    if (view < 0)
        view = 0;
    /* no panel behind the staff: it is written in ink on the dusk sky, and
     * the firs are kept clear of it (see treeline) */
    /* the clef (art/clef.png): the ink glyph, its coil on the G4 line */
    draw_sprite(STAFF_CLEF_X, STAFF_CLEF_Y, CLEF_PX, CLEF_PAL, CLEF_W, CLEF_H, 1, 0);
    for (i = 0; i < 5u; i++)                                   /* the five lines */
        ln(STAFF_X0, STAFF_TOP + (int32_t)i * STAFF_LINE, STAFF_X1, STAFF_TOP + (int32_t)i * STAFF_LINE, C_STAFF);
    for (i = 0; i < STAFF_COLS; i++) {                        /* the bar lines, every eight eighths */
        int32_t c = view + (int32_t)i;
        if (c % 8 == 0 && c > 0)
            ln(46 + (int32_t)i * STAFF_COL - 6, STAFF_TOP, 46 + (int32_t)i * STAFF_COL - 6,
               STAFF_TOP + 4 * STAFF_LINE, C_BAR);
    }
    for (i = 0; i < ui.st_n; i++) {
        staff_ent_t *e = &ui.st[STAFF_N - ui.st_n + i];   /* the entries fill from the tail: oldest first */
        int32_t col = (int32_t)e->start - view;
        int32_t cx, cy, s;
        uint16_t c = e->miss ? C_MISS : C_INK;
        if (col < 0 || col >= STAFF_COLS)
            continue;                                   /* its head has scrolled away */
        cx = 46 + col * STAFF_COL;
        cy = staff_y(e->step);
        if (e->step <= 0)                                      /* the ledger lines, one a line's row */
            for (s = 0; s >= e->step; s -= 2)
                ln(cx - 5, staff_y(s), cx + 5, staff_y(s), C_STAFF);
        if (e->sharp)
            staff_sharp(cx - 9, cy, c);
        staff_head(cx, cy, c, e->len >= 4, e->len >= 8);   /* hollow for a half or a whole */
        if (e->len != 8) {                               /* a whole note has no stem */
            int up = cy > 180;                          /* the middle line: the stem's side */
            if (up)
                rrect(cx + 5, cy - STAFF_STEM, 2, STAFF_STEM + 1, c);   /* up: beside the head's right */
            else
                rrect(cx - 6, cy, 2, STAFF_STEM + 1, c);               /* down: beside the head's left */
            if (e->len == 1 || e->len == 3)             /* the flag of an eighth */
                staff_flag(up ? cx + 5 : cx - 6, up ? cy - STAFF_STEM : cy + STAFF_STEM, up, c);
            if (e->len == 3 || e->len == 6) {           /* the dot of a dotted note */
                int32_t dy = (e->step % 2) ? 0 : -3;   /* on a line: the dot goes in the space above */
                rrect(cx + 7, cy + dy - 1, 2, 2, c);
            }
        }
    }
}

/* The play mode, on every screen but the title (the title shows the modes in
 * its PLAY MODE box instead), centered below the name. The width comes
 * from an off-canvas probe draw (cv_text returns the end x; nothing is
 * painted that far above the band), so no text is ever painted off-centre. */
static void mode_label(void)
{
    const char *s = MODE_NAME[play_mode()];
    int32_t w = ttext(0, -5000, &FONT_XS, s, C_WHITE);
    ttext((240 - w) / 2, 24, &FONT_XS, s, C_HI);
}

/* The held-key note modal, free-play/audition + YOUR TURN only (the sound
 * pages and ST_PLAY; not on the title/HOME screen): while keys are physically
 * held, a bordered panel shows them big (FONT_M) in side-by-side columns, the
 * name+octave on top and the MIDI number centred below it, e.g.
 *
 *     C4    E4
 *     60    64
 *
 * Derived from the live ui.keys bitmask each frame, so it appears on press
 * and is gone the frame after release, with no timer or release handler.
 * Visual only: no ann_say, no words-file entry for the reader. Truncated:
 * only two columns are shown, so a third note and beyond show as "+N" on the
 * name line rather than wrapping. On YOUR TURN each column carries a dot
 * latched at press until release, at the name line's height: green if that
 * key was correct when pressed, red if it was a miss. */
static int note_modal_shown(void)      /* the modal's gate: sound pages + YOUR TURN, a key held,
                                         * and only with the staff off (the staff writes the
                                         * notes instead, so the two do not share the screen) */
{
    return (ui.state == ST_SOUND || ui.state == ST_PLAY) && ui.keys && !set.staff;
}
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
#define NOTE_SHOWN 2u                  /* columns shown; the rest become +N */
#define NOTE_PH 60                     /* modal height: two FONT_M lines (24 each) plus padding */
static void note_label(void)
{
    char names[NOTE_SHOWN][8], nums[NOTE_SHOWN][12];
    uint8_t good[NOTE_SHOWN];
    char extra[8];
    uint32_t k, held = 0, shown = 0, i;
    int32_t wn[NOTE_SHOWN], wu[NOTE_SHOWN], col[NOTE_SHOWN], total = 0, ew = 0, px, py, pw, cx;
    int player = (ui.state == ST_PLAY);
    if (!note_modal_shown())
        return;
    for (k = 0; k < NKEYS; k++) {
        int32_t note;
        char oct[2];
        if (!(ui.keys & (1u << k)))
            continue;
        held++;
        if (shown >= NOTE_SHOWN)
            continue;
        note = 53 + (int32_t)k;                 /* key 0 = F3 (MIDI 53) */
        names[shown][0] = 0;
        text_cat(names[shown], NOTE_NAME[(uint32_t)note % 12u]);
        oct[0] = (char)('0' + (note / 12 - 1));
        oct[1] = 0;
        text_cat(names[shown], oct);
        int_text(note, nums[shown]);
        good[shown] = (ui.key_good & (1u << k)) ? 1u : 0u;
        shown++;
    }
    if (!held || !shown)
        return;
    for (i = 0; i < shown; i++) {
        wn[i] = ttext(0, -5000, &FONT_M, names[i], C_WHITE);
        wu[i] = ttext(0, -5000, &FONT_M, nums[i], C_WHITE);
        col[i] = wn[i] > wu[i] ? wn[i] : wu[i]; /* the column is as wide as its wider line */
        total += col[i] + (player ? 14 : 0);    /* the dot + its gap on YOUR TURN */
        if (i)
            total += 16;
    }
    if (held > shown) {
        char num[12];
        int_text((int32_t)(held - shown), num);
        extra[0] = '+';
        extra[1] = 0;
        text_cat(extra, num);
        ew = ttext(0, -5000, &FONT_M, extra, C_WHITE);
        total += 16 + ew;
    }
    py = (240 - NOTE_PH) / 2;                   /* vertically centred on every screen */
    pw = total + 16;
    if (pw > 236)
        pw = 236;
    px = (240 - pw) / 2;
    rrect(px, py, pw, NOTE_PH, C_WHITE);            /* the modal rim */
    rrect(px + 2, py + 2, pw - 4, NOTE_PH - 4, C_PANEL); /* the modal panel, on top of snow/frosty */
    cx = px + 8;
    for (i = 0; i < shown; i++) {
        if (player) {
            ball(cx + 5, py + 16, 5, good[i] ? RGB(90, 255, 130) : RGB(255, 110, 110));
            cx += 14;
        }
        ttext(cx + (col[i] - wn[i]) / 2, py + 4, &FONT_M, names[i], C_WHITE);
        ttext(cx + (col[i] - wu[i]) / 2, py + 32, &FONT_M, nums[i], C_WHITE);
        cx += col[i] + 16;
    }
    if (held > shown)
        ttext(cx, py + 4, &FONT_M, extra, C_WHITE);
}

/* ------------------------------------------------------------ the badge --- */
/* The badge is a QR code: the page on the phone, with the carol and the level
 * in the address, so the phone knows what was played. The codes themselves are
 * in qr_data.h (tools/gen_qr.py made them; one per carol and level). */
#define QR_PX 5                            /* a module, in pixels */
#define QR_QUIET 4                         /* the quiet zone, in modules */
#define QR_TOTAL ((QR_MODULES + 2 * QR_QUIET) * QR_PX)

static uint8_t qr_variant(void)
{
    return (uint8_t)(ui.qr_carol * QR_LEVELS + (ui.qr_level - 1u));
}

static void draw_qr(int32_t x0, int32_t y0, uint8_t variant)
{
    const uint8_t *bits = QR_BITS[variant];
    int32_t mx, my;
    for (my = 0; my < QR_MODULES; my++)
        for (mx = 0; mx < QR_MODULES; mx++) {
            uint32_t i = (uint32_t)(my * QR_MODULES + mx);
            if (bits[i >> 3] & (1u << (i & 7)))
                rrect(x0 + mx * QR_PX, y0 + my * QR_PX, QR_PX, QR_PX, C_PANEL);
        }
}

static void draw_scan(void)
{
    const uint16_t paper = RGB(250, 244, 232);
    char t[16];
    int32_t w;
    int32_t qx = (240 - QR_TOTAL) / 2 + QR_QUIET * QR_PX;   /* the first module */
    rrect(0, 0, 240, 240, paper);            /* the code must be dark on light */
    draw_qr(qx, QR_QUIET * QR_PX, qr_variant());
    if (ui.qr_from == ST_SONGDONE) {
        /* the trophy of a song done: the whole song, and on to the next one */
        w = ttext(0, -5000, &FONT_XS, "SONG COMPLETE", RGB(255, 200, 60));
        ttext((240 - w) / 2, 190, &FONT_XS, "SONG COMPLETE", RGB(255, 200, 60));
        w = ttext(0, -5000, &FONT_XS, CAROL_NAME[ui.qr_carol], C_PANEL);
        ttext((240 - w) / 2, 202, &FONT_XS, CAROL_NAME[ui.qr_carol], C_PANEL);
        w = ttext(0, -5000, &FONT_XS, "SCAN FOR YOUR BADGE", C_PANEL);
        ttext((240 - w) / 2, 214, &FONT_XS, "SCAN FOR YOUR BADGE", C_PANEL);
        w = ttext(0, -5000, &FONT_XS, "PLAY: NEXT SONG", C_GRAY);
        ttext((240 - w) / 2, 226, &FONT_XS, "PLAY: NEXT SONG", C_GRAY);
        return;
    }
    w = ttext(0, -5000, &FONT_XS, CAROL_NAME[ui.qr_carol], C_PANEL);
    ttext((240 - w) / 2, 190, &FONT_XS, CAROL_NAME[ui.qr_carol], C_PANEL);
    t[0] = 'L'; t[1] = 'E'; t[2] = 'V'; t[3] = 'E'; t[4] = 'L'; t[5] = ' ';
    t[6] = (char)('0' + (ui.qr_level / 10u));
    t[7] = (char)('0' + (ui.qr_level % 10u));
    t[8] = 0;
    w = ttext(0, -5000, &FONT_XS, t, C_PANEL);
    ttext((240 - w) / 2, 202, &FONT_XS, t, C_PANEL);
    w = ttext(0, -5000, &FONT_XS, "SCAN FOR YOUR BADGE", C_PANEL);
    ttext((240 - w) / 2, 214, &FONT_XS, "SCAN FOR YOUR BADGE", C_PANEL);
    w = ttext(0, -5000, &FONT_XS, "PLAY: BACK", C_GRAY);
    ttext((240 - w) / 2, 226, &FONT_XS, "PLAY: BACK", C_GRAY);
}

static void draw_content(void)
{
    char t[32];
    uint32_t now = plat_ms();
    int staff_here = set.staff && (ui.state == ST_LISTEN || ui.state == ST_PLAY || ui.state == ST_RESULT);
    (void)now;
    if (ui.state == ST_SCAN) {                 /* the badge: its own paper, no sky */
        draw_scan();
        return;
    }
    sky(ui.frame);
    snowfall(ui.frame);
    treeline(ui.state == ST_GRUMP ? 206 : 228, ui.frame, staff_here ? STAFF_X1 : 0);
    snow_ground(ui.state == ST_GRUMP ? 206 : 228, ui.frame);
    if (ui.state == ST_TITLE) {                 /* the title name is centred, with the bulb beside it */
        int32_t w = ttext(0, -5000, &FONT_B, APP_NAME, C_WHITE);
        ttext((240 - w) / 2, 6, &FONT_B, APP_NAME, C_WHITE);
    } else
        ttext(10, 8, &FONT_B, APP_NAME, C_WHITE);
    battery_meter(210, 4);                 /* the lightbulb: on every game screen */
    if (ui.state != ST_TITLE)
        mode_label();
    if (plat_ms() < ui.msg_until && ui.msg[0]) {   /* the last announcement, briefly centered under the mode */
        int32_t w = ttext(0, -5000, &FONT_XS, ui.msg, C_WHITE);
        ttext((240 - w) / 2, 40, &FONT_XS, ui.msg, C_WHITE);
    }
    if (staff_here)
        draw_staff();                            /* written on the sky, under the characters */
    switch (ui.state) {
    case ST_TITLE: {
        uint32_t mi;
        /* PLAY MODE box: the modes as a list; GLO still cycles them, this
         * only shows which is picked (checked + highlighted). The box grows
         * downward to hold the GLO hint below the modes. */
        rrect(6, 32, 106, 102, C_WHITE);
        rrect(8, 34, 102, 98, C_PANEL);
        ttext(14, 36, &FONT_XS, "PLAY MODE", C_GRAY);
        for (mi = 0; mi < NMODES; mi++) {
            int32_t my = 52 + (int32_t)mi * 16;
            int cur = (mi == play_mode());
            rrect(14, my + 4, 9, 9, C_GRAY);                   /* the checkbox: +4 to centre on the label */
            rrect(15, my + 5, 7, 7, C_PANEL);
            if (cur)
                rrect(16, my + 6, 5, 5, RGB(232, 136, 30));
            ttext(28, my + 1, &FONT_XS, MODE_NAME[mi], cur ? RGB(232, 136, 30) : C_GRAY);   /* +1: centre the visible glyphs on the 9px box */
        }
        ttext(14, 104, &FONT_XS, "PRESS GLO", C_WHITE);
        ttext(14, 116, &FONT_XS, "TO CYCLE MODE", C_GRAY);
        /* SONG box: the carols as a list; SELECT still cycles them, the red
         * arrow marks the picked one (names match the saved record order).
         * It grows downward to hold the SELECT SONG hint below the list. */
        rrect(118, 32, 116, 102, C_WHITE);
        rrect(120, 34, 112, 98, C_PANEL);
        ttext(126, 36, &FONT_XS, "SONG:", C_GRAY);
        for (mi = 0; mi < NCAROLS; mi++) {
            int32_t sy = 50 + (int32_t)mi * 14;
            int cur = (mi == ui.carol);
            if (cur)
                ttext(122, sy, &FONT_XS, ">", RGB(255, 80, 80));
            ttext(132, sy, &FONT_XS, CAROL_NAME[mi], cur ? C_WHITE : C_GRAY);   /* 132: the longest name (O CHRISTMAS TREE) ends inside the box */
        }
        ttext(126, 106, &FONT_XS, "SELECT SONG", RGB(255, 200, 60));
        ttext(126, 118, &FONT_XS, "TURN SELECT", C_GRAY);
if ((ui.frame / 60u) & 1u)   /* blink ~1 Hz: on for a second, off for a second */
            ttext(10, 140, &FONT_S, "PRESS PLAY", RGB(120, 255, 140));
        /* SAVE box: what SAVE keeps. */
        rrect(6, 162, 150, 56, C_WHITE);
        rrect(8, 164, 146, 52, C_PANEL);
        ttext(14, 168, &FONT_XS, "KEEP SCORE", C_HI);
        ttext(14, 184, &FONT_S, "PRESS SAVE TO", C_WHITE);
        ttext(14, 200, &FONT_S, "KEEP PROGRESS", C_WHITE);
        frosty(198, 234, ui.frame);       /* over the boxes, so he stands in front of the SONG box's corner */
        break;
    }
    case ST_LISTEN: {
        ttext(10, 40, &FONT_B, CAROL_NAME[ui.carol], C_WHITE);
        /* The listen info as a boxed grid like YOUR TURN: a spanning LISTEN...
         * header, then Note (demo progress) and the skip hint, split by a rule.
         * The divider sits left of centre so the Press hint fits its column. */
        rrect(10, 62, 220, 64, C_WHITE);
        rrect(12, 64, 216, 60, C_PANEL);
        {
            int32_t hw = ttext(0, -5000, &FONT_B, "LISTEN...", C_HI);
            ttext((240 - hw) / 2, 68, &FONT_B, "LISTEN...", C_HI);
        }
        rrect(12, 88, 216, 1, C_GRAY);              /* row rule under the header */
        rrect(100, 88, 1, 36, C_GRAY);              /* the column divider, under the header */
        char nt[12], of[12];
        int_text((int32_t)ui.listen_idx, nt);
        int_text((int32_t)level_len(), of);
        ttext(20, 94, &FONT_XS, "Note:", C_GRAY);
        ttext(64, 94, &FONT_XS, nt, C_WHITE);
        ttext(78, 94, &FONT_XS, "/", C_GRAY);     /* spaced: "Note: 1 / 3" */
        ttext(90, 94, &FONT_XS, of, C_WHITE);
        /* The Press hint as one line in its wider column (no label/value split). */
        ttext(110, 94, &FONT_XS, "Press PLAY", C_WHITE);
        ttext(110, 108, &FONT_XS, "to skip", C_GRAY);
        frosty(202, 234, ui.frame);       /* top layer: over the box */
        break;
    }
    case ST_PLAY: {
        ttext(10, 40, &FONT_B, CAROL_NAME[ui.carol], C_WHITE);
        /* The turn stats as a boxed grid: a spanning YOUR TURN header, then
         * two rows of two columns (Note/Score, Combo/Misses), split by a rule. */
        rrect(10, 62, 220, 76, C_WHITE);
        rrect(12, 64, 216, 72, C_PANEL);
        {
            int32_t hw = ttext(0, -5000, &FONT_B, "YOUR TURN", C_HI);
            ttext((240 - hw) / 2, 68, &FONT_B, "YOUR TURN", C_HI);
        }
        rrect(12, 88, 216, 1, C_GRAY);              /* row rules */
        rrect(12, 112, 216, 1, C_GRAY);
        rrect(120, 88, 1, 48, C_GRAY);             /* the column divider, under the header */
        char nt[12], of[12];
        int_text((int32_t)ui.expect_idx, nt);
        int_text((int32_t)level_len(), of);
        ttext(20, 94, &FONT_XS, "Note:", C_GRAY);
        ttext(64, 94, &FONT_XS, nt, C_WHITE);
        ttext(78, 94, &FONT_XS, "/", C_GRAY);     /* spaced: "Note: 1 / 3" */
        ttext(90, 94, &FONT_XS, of, C_WHITE);
        int_text((int32_t)ui.score, t);
        ttext(130, 94, &FONT_XS, "Score:", C_GRAY);
        ttext(180, 94, &FONT_XS, t, C_WHITE);
        int_text((int32_t)ui.combo, t);
        ttext(20, 118, &FONT_XS, "Combo:", C_GRAY);
        ttext(72, 118, &FONT_XS, t, C_WHITE);
        int_text((int32_t)ui.misses, t);
        ttext(130, 118, &FONT_XS, "Misses:", C_GRAY);
        ttext(182, 118, &FONT_XS, t, C_HI);
        /* The repeat hint below the box: PLAY replays the level (via Listen).
         * With the staff on it moves under the staff (the strip takes its row). */
        if ((ui.frame / 60u) & 1u)   /* blink ~1 Hz: on for a second, off for a second */
            ttext(14, 144, &FONT_XS, "Press PLAY to Repeat", RGB(120, 255, 140));
        frosty(202, 234, ui.frame);       /* top layer: over the box */
        break;
    }
    case ST_GRUMP: {
        grump_spr(120, 212, ui.frame);
        ttext(10, 40, &FONT_B, "GRUMP MODE", C_HI);
        ttext(10, 70, &FONT_XS, "HE JEERS. TRY AGAIN.", C_WHITE);
        ttext(10, 100, &FONT_XS, "PLAY: RETRY NOW", RGB(120, 255, 140));
        break;
    }
    case ST_SONGDONE: {
        santa_spr(198, 234, ui.frame, 1);
        draw_confetti();
        ttext(10, 40, &FONT_B, "SONG COMPLETE", C_HI);
        ttext(10, 64, &FONT_XS, CAROL_NAME[ui.carol], C_WHITE);
        int_text((int32_t)ui.score, t);
        ttext(10, 90, &FONT_XS, "SCORE", C_GRAY);
        ttext(70, 90, &FONT_XS, t, C_WHITE);
        ttext(10, 170, &FONT_XS, "PLAY: YOUR BADGE", C_GRAY);   /* then the trophy, the QR code */
        break;
    }
    case ST_RESULT: {
        draw_confetti();                          /* behind the box */
        /* The clear stats as a boxed grid like YOUR TURN: a spanning LEVEL
         * CLEAR header, Score/Level in two columns, and the blinking PRESS
         * PLAY spanning a bottom row. Santa draws over the box. */
        rrect(10, 62, 220, 76, C_WHITE);
        rrect(12, 64, 216, 72, C_PANEL);
        {
            int32_t hw = ttext(0, -5000, &FONT_B, "LEVEL CLEAR", C_HI);
            ttext((240 - hw) / 2, 68, &FONT_B, "LEVEL CLEAR", C_HI);
        }
        rrect(12, 88, 216, 1, C_GRAY);              /* row rules */
        rrect(12, 112, 216, 1, C_GRAY);
        rrect(120, 88, 1, 24, C_GRAY);              /* the column divider, in the stats row only */
        int_text((int32_t)ui.score, t);
        ttext(20, 94, &FONT_XS, "Score:", C_GRAY);
        ttext(76, 94, &FONT_XS, t, C_WHITE);
        int_text((int32_t)ui.level, t);
        ttext(130, 94, &FONT_XS, "Level:", C_GRAY);
        ttext(182, 94, &FONT_XS, t, C_WHITE);
        if ((ui.frame / 60u) & 1u) {                /* blink ~1 Hz, centered in the bottom row */
            int32_t pw2 = ttext(0, -5000, &FONT_XS, "PRESS PLAY", RGB(120, 255, 140));
            ttext((240 - pw2) / 2, 118, &FONT_XS, "PRESS PLAY", RGB(120, 255, 140));
        }
        santa_spr(198, 234, ui.frame, 1);           /* top layer: over the box */
        break;
    }
    case ST_SOUND: {
        /* The hints are yellow, not grey: grey on the dusk sky is hard to read. */
        ttext(10, 40, &FONT_XS, "SELECT SOUND", RGB(255, 200, 60));
        draw_sprite(10, 58, SND_ICON[snd_id()].px, SND_ICON[snd_id()].pal,
                    SND_ICON[snd_id()].w, SND_ICON[snd_id()].h, 1, 0);
        ttext(32, 60, &FONT_B, SND_NAME[snd_id()], C_WHITE);
        ttext(10, 90, &FONT_XS, "PRESET: CHANGE", RGB(255, 200, 60));
        draw_sprite(10, 104, ICON_STAFF_PX, ICON_STAFF_PAL, 16, 16, 1, 0);
        ttext(32, 108, &FONT_XS, "SELECT: STAFF", RGB(255, 200, 60));   /* the state follows the hint's measured width */
        ttext(32 + ttext(0, -5000, &FONT_XS, "SELECT: STAFF", RGB(255, 200, 60)) + 4, 108,
              &FONT_XS, set.staff ? "ON" : "OFF", set.staff ? RGB(120, 255, 140) : RGB(255, 200, 60));
        ttext(10, 126, &FONT_XS, "PLAY KEYS: AUDITION", RGB(255, 200, 60));
        ttext(10, 144, &FONT_XS, "PLAY: DONE  SAVE: KEEP", RGB(255, 200, 60));
        frosty(198, 234, ui.frame);       /* top layer: over the text */
        break;
    }
    default:
        break;
    }
    note_label();                               /* on top, over snow/frosty, so the held notes stay readable */
}

static void draw(void)
{
    draw_dy = 0;
    cv_begin(240, 144, C_BLACK);
    draw_content();
    cv_blit(0, 0);
    draw_dy = 144;
    cv_begin(240, 96, C_BLACK);
    draw_content();
    cv_blit(0, 144);
    draw_dy = 0;
}

/* The boot splash (before app_init, so nothing here may use ui or the
 * saved settings): the name on paper, Frosty in his wave pose (frame 1)
 * standing in the snow. Drawn in the same two bands as the game screen. */
static void splash_content(void)
{
    /* Frosty sits high, near the screen's centre (feet at 202, snow surface
     * 6 px above them), with the frosted banner clear above his hat. */
    sky(0);
    treeline(196, 0, 0);                             /* the splash: the whole treeline */
    snow_ground(196, 0);
    draw_sprite(120 - FROSTY_W / 2, 202 - FROSTY_H,
                FROSTY_PX + (uint32_t)(FROSTY_W * FROSTY_H),
                FROSTY_PAL, FROSTY_W, FROSTY_H, 1, 0);
    draw_sprite((240 - TITLE_W) / 2, 30, TITLE_PX, TITLE_PAL, TITLE_W, TITLE_H, 1, 0);
}

void app_splash(void)
{
    const uint16_t paper = RGB(250, 244, 232);
    lcd_fill(0, 0, 240, 240, paper);
    draw_dy = 0;
    cv_begin(240, 144, paper);
    splash_content();
    cv_blit(0, 0);
    draw_dy = 144;
    cv_begin(240, 96, paper);
    splash_content();
    cv_blit(0, 144);
    draw_dy = 0;
#ifndef PLAT_HOST
    fm1_delay_ms(1000);   /* hold the splash so it can be read; TIMER4 needs no interrupts */
#endif
}

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.state = ST_TITLE;
    ui.carol = 0; ui.level = 1; ui.score = 0;
    ui.keys = touch_keys();              /* held inputs at boot are not fresh presses */
    ui.btn = plat_buttons();
    ui.st_n = 0; ui.st_pos = 0;
    ui.redraw = 1;
    snd_play_id(SND_CLIP_INTRO);         /* Santa's intro voice, once at boot on the title screen */
    app_say(APP_NAME, "");
}

void app_frame(void)
{
    ui.frame++;
    ui.redraw = 1;                       /* sprites animate every frame */
    input();
    tick_game();
    if (ann_on())
        ann_tick(plat_ms());
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "score")) *got = (int)ui.score;
    else if (!strcmp(what, "level")) *got = (int)ui.level;
    else if (!strcmp(what, "misses")) *got = (int)ui.misses;
    else if (!strcmp(what, "state")) *got = (int)ui.state;
    else if (!strcmp(what, "carol")) *got = (int)ui.carol;
    else if (!strcmp(what, "len")) *got = (int)level_len();
    else if (!strcmp(what, "snd")) *got = snd_on;
    else if (!strcmp(what, "slen")) *got = (int)snd_len;
    else if (!strcmp(what, "dirty")) *got = (int)set_dirty;
    else if (!strcmp(what, "mode")) *got = (int)play_mode();
    else if (!strcmp(what, "midi_on")) *got = (int)midi_on();
    else if (!strcmp(what, "snd_sel")) *got = (int)set.snd;
    else if (!strcmp(what, "staff")) *got = (int)set.staff;
    else if (!strcmp(what, "best")) *got = (int)set.best_score[0];   /* Jingle Bells' best (CCR4-upgrade test) */
    else if (!strcmp(what, "staff_n")) *got = (int)ui.st_n;
    else if (!strcmp(what, "staff_step")) {   /* the newest entry's staff step (diatonic above C4), -99 = none */
        *got = ui.st_n ? (int)ui.st[STAFF_N - 1].step : -99;
    }
    else if (!strcmp(what, "staff_sharp")) *got = ui.st_n ? (int)ui.st[STAFF_N - 1].sharp : -1;
    else if (!strcmp(what, "staff_len")) *got = ui.st_n ? (int)ui.st[STAFF_N - 1].len : -1;   /* the newest entry's length, in eighths */
    else if (!strcmp(what, "staff_pos")) *got = (int)ui.st_pos;   /* the eighth the staff is at */
    else if (!strcmp(what, "mid")) {     /* EASY's steady key in PLAY (else -1) */
        int32_t nk = -1;
        if (play_mode() == MODE_EASY && ui.state == ST_PLAY)
            nk = (int32_t)song_note((uint8_t)ui.expect_idx) - 53;
        *got = (int)nk;
    }
    else if (!strcmp(what, "note_modal")) *got = note_modal_shown();
    else if (!strcmp(what, "qr_carol")) *got = (int)ui.qr_carol;
    else if (!strcmp(what, "qr_level")) *got = (int)ui.qr_level;
    else if (!strcmp(what, "qr_variant")) *got = (int)qr_variant();
    else if (!strcmp(what, "note")) {
        uint32_t i; *got = -1;
        /* a releasing voice is a fading tail (demo/bass ring-out), not a note */
        for (i = 0; i < NVOICES; i++) if (voices[i].note >= 0 && !voices[i].releasing) { *got = (int)voices[i].note; break; }
    }
    else
        return 0;
    return 1;
}
#endif
