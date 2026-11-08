/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* Carol Courier: the FM-1 plays the start of a public-domain carol and you
 * replay it on the keys. The song builds as you level up: level 1 asks for
 * the first 3 notes, and each level adds 3 more on top of all the notes
 * before, so the last level (11) is the whole 32-note song on piano (music
 * box) throughout. Three misses sends you to Grump Mode (a recorded jeer,
 * then a retry), and a clean level earns a word-pair Carol Code encoding
 * carol + level that you can text to a friend. Finishing the whole song
 * brings up a celebration screen offering the next song.
 * Part of the unity build (device/unity.c, host/plat_host.c), after the
 * platform: gfx.c and announce.c are in scope. */
#include "fastmath.h"
#include "sprite_data.h"

#define SET_MAGIC 0x43435233u            /* "CCR3": the settings/scores record */
#define SET_MAGIC_OLD 0x43435232u        /* "CCR2": pre-sound settings record */
#define NVOICES 6            /* melody, each sustained with a ring-out: headroom so no voice steals another */
#define NCAROLS 3
#define NPHRASES 8
#define PHRASE_LEN 4
#define NSONG_NOTES (NPHRASES * PHRASE_LEN)   /* 32 notes per carol, flattened */
#define NOTES_PER_LEVEL 3
#define NLEVELS 11                       /* 3,6,...,30,32 notes: level 11 is the whole song */
#define NWORDS 64

/* The play mode (GLO cycles it): EASY = LISTEN blink + a steady mid-level key to
 * press in PLAY; MEDIUM = LISTEN blink only (the original behaviour); HARD =
 * no LED guidance at all. Stored in the settings' spare bytes so old saves
 * (scores, codes) keep working with no magic bump: legacy saves have 0 there,
 * i.e. EASY. */
enum { MODE_EASY = 0, MODE_MEDIUM, MODE_HARD, NMODES };
static const char *const MODE_NAME[NMODES] = {"EASY", "MEDIUM", "HARD"};

typedef struct {
    uint32_t magic;
    uint32_t best_score[NCAROLS];
    uint16_t best_level[NCAROLS];
    uint8_t last_w1, last_w2;            /* the last Carol Code entered */
    uint8_t sound_on, leds_on;           /* leds_on: kept for the store layout; superseded by mode */
    uint8_t rsv[2];                      /* rsv[0] = the play mode (MODE_*); rsv[1] = MIDI off (0 = on) */
    uint8_t snd;                         /* selected sound (SND_*) */
    uint8_t env_a, env_d, env_r;         /* envelope 0..100 (defaults match the old fixed envelopes) */
    uint8_t grain;                       /* grain shimmer 0..100 */
} settings_t;
/* The pre-sound record (CCR2, 28 bytes) loads through this and is upgraded. */
typedef struct {
    uint32_t magic;
    uint32_t best_score[NCAROLS];
    uint16_t best_level[NCAROLS];
    uint8_t last_w1, last_w2;
    uint8_t sound_on, leds_on;
    uint8_t rsv[2];
} settings_legacy_t;
static settings_t set;
static uint8_t set_dirty;

static uint8_t play_mode(void) { return set.rsv[0]; }

/* The selectable sounds. 0..3 are synth (always there); 4 is the sampled
 * piano (resource clip 5, C4 = MIDI 60). A fresh install defaults to PIANO;
 * an existing save keeps whatever the player picked. */
enum { SND_BELLS = 0, SND_MUSICBOX, SND_CHOIR, SND_KAZOO, SND_PIANO, SND_FLUTE, SND_SAX, NSOUND };
static const char *const SND_NAME[NSOUND] = {"BELLS", "MUSICBOX", "CHOIR", "KAZOO", "PIANO", "FLUTE", "SAX"};
#define PIANO_BASE 60          /* piano-c4.wav plays MIDI C4 */
#define FLUTE_BASE 72          /* flute-c5.wav plays MIDI C5 */
#define SAX_BASE 58            /* sax-a3.wav plays MIDI A#3 */
#define SND_CLIP_PIANO 5u
#define SND_CLIP_FLUTE 6u
#define SND_CLIP_SAX 7u

static uint8_t snd_id(void) { return set.snd < NSOUND ? set.snd : SND_BELLS; }

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
 * 4 = half, 3 = dotted quarter, 6 = dotted half, 8 = whole/final),
 * transcribed from the real tunes onto those pitches, so the demo has the
 * tune's rhythm and phrasing instead of a flat note per slot. */
static const uint8_t CAROL_DUR[NCAROLS][NSONG_NOTES] = {
    { /* Jingle Bells: quarters, dotted-half landings with eighth pickups */
        2, 2, 6, 1,  2, 2, 6, 1,  2, 2, 6, 1,  2, 2, 2, 4,
        2, 2, 6, 1,  2, 2, 6, 1,  2, 2, 2, 4,  2, 2, 2, 8,
    },
    { /* Deck the Halls: quarter lines, then held E/D lines, long finale */
        2, 2, 2, 2,  2, 2, 2, 2,  4, 4, 4, 4,  4, 4, 4, 4,
        2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 2, 8,
    },
    { /* We Wish You a Merry Christmas: quarters, held E/D lines, long final C */
        2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 2, 2,  4, 4, 4, 4,
        4, 4, 4, 4,  2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 2, 8,
    },
};
#define EIGHTH_MS 180u      /* one eighth note in the demo (quarter = 360 ms) */
#define PHRASE_GAP_MS 200u  /* breath after every 4-note phrase */
static const uint8_t CAROLS[NCAROLS][NPHRASES][PHRASE_LEN] = {
    { /* Jingle Bells */
        {64, 64, 67, 64}, {64, 64, 64, 62}, {64, 64, 67, 64}, {64, 67, 67, 67},
        {64, 64, 67, 64}, {64, 64, 64, 64}, {64, 64, 62, 62}, {64, 67, 64, 62},
    },
    { /* Deck the Halls */
        {62, 64, 66, 67}, {67, 67, 67, 67}, {64, 64, 64, 64}, {62, 62, 62, 62},
        {62, 64, 66, 67}, {67, 67, 67, 67}, {67, 66, 64, 62}, {64, 62, 64, 62},
    },
    { /* We Wish You a Merry Christmas */
        {67, 67, 69, 67}, {67, 69, 69, 69}, {67, 65, 65, 65}, {64, 64, 64, 64},
        {62, 62, 62, 62}, {64, 64, 67, 67}, {69, 67, 65, 62}, {60, 60, 60, 60},
    },
};
static const char *const CAROL_NAME[NCAROLS] = {"JINGLE BELLS", "DECK THE HALLS", "WE WISH YOU"};

/* 64 speakable, distinct code words. Word 1 encodes carol*16+(level-1),
 * word 2 repeats level-1 as a transcription check. */
static const char *const WORDS[NWORDS] = {
    "SLEIGH", "TREETOP", "TINSEL", "REINDEER", "SNOWMAN", "COCOA", "GINGER", "CANDY",
    "WREATH", "BELLS", "STAR", "TREE", "ELF", "TOY", "STOCKING", "MITTEN",
    "SCARF", "BOOT", "CANDLE", "GARLAND", "PUDDING", "MINCE", "CRACKER", "RIBBON",
    "BOW", "WRAP", "PRESENTS", "CAROL", "HYMN", "CHOIR", "ORGAN", "VIOLIN",
    "FLUTE", "DRUM", "HORN", "TRUMPET", "TROMBONE", "CELLO", "PIANO", "HARP",
    "MUSIC", "BOX", "LANTERN", "WINDOW", "FROST", "ICICLE", "WINTER", "DECEMBER",
    "NOEL", "ADVENT", "MISTLETOE", "HOLLY", "IVY", "PINE", "FIR", "LOG",
    "FIRE", "EMBER", "SPICE", "CINNAMON", "ORANGE", "PLUM", "FIGGY", "SUET",
};
/* The list is a fixed, documented set of 64 speakable words. */

/* ------------------------------------------------------------------ voice --- */
enum { TIM_MUSICBOX, TIM_BELLS, TIM_CHOIR, TIM_KAZOO, TIM_PIANO, TIM_FLUTE, TIM_SAX, NTIM };
typedef struct {
    int32_t note;                        /* -1 = idle */
    float phase, mphase, level, age, vel;
    float rel_age;                       /* age when a scheduled release began */
    uint8_t timbre, held, releasing;     /* releasing: ringing out, not held */
    /* Sample playback (TIM_PIANO/FLUTE/SAX): the resource clip's bytes, with
     * a Q16 position stepping by pitch. s_step 0 and s_data 0 = synth fallback. */
    const uint8_t *s_data;
    uint32_t s_idx, s_frac, s_step, s_len;
    uint32_t lfsr;                       /* per-voice grain seed */
} voice_t;
static voice_t voices[NVOICES];
static volatile uint8_t voice_timbre = TIM_BELLS;

static uint8_t snd_timbre(uint8_t id)
{
    switch (id) {
    case SND_BELLS: return TIM_BELLS;
    case SND_MUSICBOX: return TIM_MUSICBOX;
    case SND_CHOIR: return TIM_CHOIR;
    case SND_KAZOO: return TIM_KAZOO;
    case SND_PIANO: return TIM_PIANO;
    case SND_FLUTE: return TIM_FLUTE;
    case SND_SAX: return TIM_SAX;
    default: return TIM_BELLS;
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
    voices[best].lfsr = (uint32_t)note * 2654435761u | 1u;
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
 * use the same path, so RELEASE governs both: the player hears the tail
 * they asked for on the ENVELOPE page.
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
    float atk = 0.001f * fm_exp2f((float)set.env_a / 12.0f);    /* ATTACK 0..100 => 1..332 ms */
    float rlim = 0.05f + (float)set.env_r * 0.0145f;           /* RELEASE 0..100 => 0.05..1.5 s */
    gain_now += (gain - gain_now) * 0.004f;
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
            /* Envelope user scales (defaults 25/50 => factor 1.0): attack time,
             * decay speed and release all ride on the saved values. */
            {
                float afact = 0.25f + (float)set.env_a / 50.0f;   /* 0.25..2.25 */
                float dfact = 0.25f + (float)set.env_d / 50.0f;
                float a_age = vc->age / (afact > 0.0f ? afact : 1.0f);
                float d_age = vc->age * dfact;
                (void)a_age;
                w = note_inc(vc->note);
                vc->phase += w;
                if (vc->phase >= 1.0f) vc->phase -= 1.0f;
                vc->mphase += w * 3.5f;
                if (vc->mphase >= 1.0f) vc->mphase -= 1.0f;
                switch (vc->timbre) {
                case TIM_MUSICBOX: {           /* fundamental + decaying harmonics + attack click; sustains while held */
                    float ph = vc->phase * 6.2831853f;
                    env = vc->held ? (0.35f + 0.65f * fm_exp2f(-d_age * 8.0f)) : fm_exp2f(-d_age * 5.0f);
                    vs += fm_sinf(ph) * env * 0.45f * vc->vel * ramp;
                    vs += fm_sinf(ph * 2.0f) * fm_exp2f(-d_age * 10.0f) * 0.20f * vc->vel * ramp;
                    vs += fm_sinf(ph * 3.01f) * fm_exp2f(-d_age * 18.0f) * 0.10f * vc->vel * ramp;
                    vs += fm_sinf(ph * 4.0f) * fm_exp2f(-vc->age * 250.0f) * 0.25f * vc->vel * ramp;   /* click transient */
                    break;
                }
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
                     * pitch-shifted readout with the user envelope and grain
                     * shimmer. Missing clip = the bells fallback below. */
                    if (!vc->s_data) {
                        float idx = 1.2f * fm_exp2f(-d_age * 4.0f);
                        env = vc->held ? (0.25f + 0.75f * fm_exp2f(-d_age * 4.0f)) : fm_exp2f(-d_age * 2.2f);
                        vs += fm_sinf((vc->phase + idx * fm_sinf(vc->mphase * 6.2831853f)) * 6.2831853f) * env * 0.32f * vc->vel * ramp;
                        break;
                    }
                    {
                        float dec = fm_exp2f(-vc->age * (0.5f + dfact * 0.5f));
                        float g = 1.0f;
                        uint32_t step = vc->s_step;
                        float samp = 0.0f;
                        if (set.grain) {                  /* grain: per-frame pitch jitter */
                            vc->lfsr ^= vc->lfsr << 13; vc->lfsr ^= vc->lfsr >> 17; vc->lfsr ^= vc->lfsr << 5;
                            g = 1.0f + ((float)(vc->lfsr >> 16 & 0x3FFu) - 512.0f) * (float)set.grain * 0.000001953f;
                            step = (uint32_t)((float)step * g + 0.5f);
                        }
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
    set.rsv[0] = set.rsv[1] = 0;
    set.snd = SND_PIANO;
    set.env_a = 25u; set.env_d = 50u; set.env_r = 50u;
    set.grain = 0u;
}
static int load_settings(void)
{
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC
        && set.sound_on <= 1u && set.leds_on <= 1u && set.last_w1 < NWORDS && set.last_w2 < NWORDS
        && set.rsv[0] < NMODES && set.rsv[1] <= 1u && set.snd < NSOUND
        && set.env_a <= 100u && set.env_d <= 100u && set.env_r <= 100u && set.grain <= 100u)
        return 1;
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
    settings_defaults();
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
enum { ST_TITLE, ST_SELECT, ST_CODE, ST_LISTEN, ST_PLAY, ST_GRUMP, ST_RESULT, ST_SONGDONE, ST_SOUND, ST_ENV, ST_GRAIN, NST };
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
    uint8_t code_word;                   /* 0 = entering word 1, 1 = word 2 */
    uint8_t code_w[2];
    uint8_t msg_kind;                    /* last announcement kind, for tests */
    uint8_t jingle_idx;                  /* celebration jingle progress (255 = done) */
    uint32_t jingle_at;
    float conf_x[40], conf_y[40], conf_vx[40], conf_vy[40];
    uint8_t conf_c[40], conf_life[40];
    uint32_t keys, btn;
    uint8_t redraw;
    uint32_t frame;
    char msg[24];                      /* last announcement, drawn as an overlay */
    uint32_t msg_until;
    uint8_t env_idx;                   /* ENVELOPE page: 0 A, 1 D, 2 R */
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
static void code_text(char *b)
{
    b[0] = 0;
    text_cat(b, WORDS[set.last_w1]);
    text_cat(b, "-");
    text_cat(b, WORDS[set.last_w2]);
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
     * already show (toggles, encoder turns, SAVED, SOUND changes). State
     * announcements above stay silent on screen to avoid duplicates. */
    if (seq(a, "MODE") || seq(a, "MIDI") || seq(a, "SOUND") ||
        seq(a, "ENVELOPE") || seq(a, "GRAIN") || seq(a, "ATTACK") ||
        seq(a, "DECAY") || seq(a, "RELEASE") || seq(a, "SAVED") ||
        seq(a, "SAVE FAILED")) {
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
/* Phrase starts are accented, other notes a touch softer. */
static uint32_t song_vel(uint8_t idx) { return (idx % PHRASE_LEN == 0u) ? 110u : 95u; }
static int32_t sched_note = -1;          /* last game-triggered note, for its ring-out */
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
    ui.redraw = 1;
    app_say("LISTEN", CAROL_NAME[ui.carol]);
}

static void start_play(void)
{
    sched_off();                         /* the last listened-to note rings out, not under the retry screen */
    ui.state = ST_PLAY;
    ui.expect_idx = 0;
    ui.hits = 0; ui.misses = 0; ui.combo = 0;
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
    uint32_t bonus = 250u;
    ui.score += bonus;
    if (ui.score > set.best_score[ui.carol]) {
        set.best_score[ui.carol] = ui.score;
        if (ui.level > set.best_level[ui.carol]) set.best_level[ui.carol] = ui.level;
        set_dirty = 1;
    }
    /* the code for the challenge just beaten */
    set.last_w1 = (uint8_t)(ui.carol * 16u + (ui.level - 1));
    set.last_w2 = (uint8_t)(ui.level - 1);
    set_dirty = 1;
    start_celebration();
    if (ui.level >= NLEVELS) {
        ui.state = ST_SONGDONE;
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
        voice_on(note, vel, snd_timbre(snd_id()), 1);   /* free play / sound pages: the selected sound */
        return;
    }
    if (note == song_note(ui.expect_idx)) {
        voice_on(note, vel, snd_timbre(snd_id()), 1);   /* your turn sounds like the demo */
        ui.score += 100u;
        ui.combo++;
        ui.hits++;
        ui.expect_idx++;
        if (ui.expect_idx >= level_len())
            finish_level();
    } else {
        voice_on(note, vel, TIM_KAZOO, 1);
        ui.misses++;
        ui.combo = 0;
        app_say("MISS", "");
        if (ui.misses >= 3)
            enter_grump();
    }
    ui.redraw = 1;
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
            voice_release(53 + (int32_t)k);
            midi_note_off(53 + (int32_t)k);
        }
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (k == B_PLAY) {
            if (ui.state == ST_TITLE || ui.state == ST_SELECT) { ui.level = 1; start_listen(); }
            else if (ui.state == ST_LISTEN) start_play();
            else if (ui.state == ST_RESULT) next_level();
            else if (ui.state == ST_SONGDONE) next_song();
            else if (ui.state == ST_GRUMP) start_play();
            else if (ui.state == ST_PLAY) start_listen();
            else if (ui.state == ST_SOUND || ui.state == ST_ENV || ui.state == ST_GRAIN) {
                ui.state = ST_TITLE; ui.redraw = 1; app_say(APP_NAME, "");
            }
        } else if (k == B_EDIT) {
            sched_off();
            ui.state = ST_SOUND; ui.redraw = 1; app_say("SOUND", SND_NAME[snd_id()]);
        } else if (k == B_ENV) {
            sched_off();
            ui.state = ST_ENV; ui.env_idx = 0; ui.redraw = 1; app_say("ENVELOPE", "ATTACK");
        } else if (k == B_LFO) {
            sched_off();
            ui.state = ST_GRAIN; ui.redraw = 1; app_say("GRAIN", "");
        } else if (k == B_HOME) {
            sched_off();
            ui.state = ST_TITLE; ui.redraw = 1; app_say(APP_NAME, "");
        } else if (k == B_REC) {
            if (ui.state == ST_PLAY || ui.state == ST_LISTEN)
                start_listen();            /* REC replays the level */
            else if (ui.state == ST_TITLE || ui.state == ST_SELECT) { ui.level = 1; start_listen(); }
        } else if (k == B_SAVE) {
            app_say(save_settings() ? "SAVED" : "SAVE FAILED", "");
        } else if (k == B_SEQ && ui.state == ST_TITLE) {
            snd_stop();                    /* SEQ replays from the top, even mid-intro */
            snd_play_id(SND_CLIP_INTRO); /* Santa's intro voice, from the title screen only */
        } else if (k == B_ARP && ui.state == ST_SELECT) {
            ui.state = ST_CODE; ui.code_word = 0; ui.code_w[0] = set.last_w1; ui.code_w[1] = set.last_w2;
            ui.redraw = 1; app_say("CODE", WORDS[ui.code_w[0]]);
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
        if (ui.state == ST_SELECT && k == EN_PRESET) {
            ui.carol = (uint8_t)((ui.carol + e + NCAROLS) % NCAROLS);
            app_say(CAROL_NAME[ui.carol], "");
        } else if (ui.state == ST_CODE && k == EN_PRESET) {
            int32_t w = ui.code_w[ui.code_word] + e;
            w = ((w % NWORDS) + NWORDS) % NWORDS;
            ui.code_w[ui.code_word] = (uint8_t)w;
            app_say("CODE", WORDS[w]);
        } else if (ui.state == ST_CODE && k == EN_SELECT) {
            if (ui.code_word == 0) { ui.code_word = 1; app_say("CODE", WORDS[ui.code_w[1]]); }
            else {
                set.last_w1 = ui.code_w[0]; set.last_w2 = ui.code_w[1]; set_dirty = 1;
                uint8_t v1 = set.last_w1;
                /* Word 1 carries carol + level; out-of-range word values
                 * (the code words can be turned to any of 64) clamp to the
                 * nearest valid challenge rather than failing. */
                ui.carol = (uint8_t)((v1 / 16u) > NCAROLS - 1u ? NCAROLS - 1u : v1 / 16u);
                ui.level = (uint16_t)(((v1 % 16u) > NLEVELS - 1u ? NLEVELS - 1u : v1 % 16u) + 1u);
                start_listen();
            }
        } else if (ui.state == ST_SOUND && k == EN_PRESET) {
            int32_t n = (int32_t)set.snd + e;
            n = ((n % NSOUND) + NSOUND) % NSOUND;
            set.snd = (uint8_t)n; set_dirty = 1;
            app_say("SOUND", SND_NAME[snd_id()]);
        } else if (ui.state == ST_ENV && k == EN_PRESET) {
            ui.env_idx = (uint8_t)((ui.env_idx + e + 3) % 3);
            app_say("ENVELOPE", ui.env_idx == 0 ? "ATTACK" : ui.env_idx == 1 ? "DECAY" : "RELEASE");
        } else if (ui.state == ST_ENV && k == EN_SELECT) {
            uint8_t *vp = ui.env_idx == 0 ? &set.env_a : ui.env_idx == 1 ? &set.env_d : &set.env_r;
            int32_t nv = (int32_t)*vp + e;
            if (nv < 0) nv = 0; if (nv > 100) nv = 100;
            *vp = (uint8_t)nv; set_dirty = 1;
            {
                char b[16]; int_text(nv, b);
                app_say(ui.env_idx == 0 ? "ATTACK" : ui.env_idx == 1 ? "DECAY" : "RELEASE", b);
            }
        } else if (ui.state == ST_GRAIN && (k == EN_PRESET || k == EN_SELECT)) {
            int32_t nv = (int32_t)set.grain + e;
            if (nv < 0) nv = 0; if (nv > 100) nv = 100;
            set.grain = (uint8_t)nv; set_dirty = 1;
            {
                char b[16]; int_text(nv, b);
                app_say("GRAIN", b);
            }
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
            sched_on(JINGLE[ui.jingle_idx], TIM_MUSICBOX, 100);   /* music box, not bells: less shrill */
            ui.jingle_idx++;
            ui.jingle_at = now + 160u;
        } else if (ui.jingle_idx == 7u && now >= ui.jingle_at) {
            sched_off();
            ui.jingle_idx = 255u;
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

static void draw_sprite(int32_t x, int32_t y, const uint8_t *px, const uint16_t *pal, int w, int h, int scale)
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
            rrect(x + rx0 * scale, y + ry * scale, (rx - rx0) * scale, scale, pal[v]);
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
                FROSTY_PAL, FROSTY_W, FROSTY_H, 1);
}
static void santa_spr(int32_t x, int32_t yb, uint32_t ph, int celebrate)
{
    /* 4-frame arm-raising wave; faster when celebrating. All frames share the
     * same cell and ground line, so bottom-anchoring keeps him planted. */
    int32_t bob = (int32_t)(ph_sin(ph, celebrate ? 2.0f : 1.0f) * (celebrate ? 6.0f : 2.0f));
    uint32_t frame = (ph / (celebrate ? 9u : 16u)) % SANTA_NFRAMES;
    draw_sprite(x - SANTA_W / 2, yb - SANTA_H + bob,
                SANTA_PX + frame * (uint32_t)(SANTA_W * SANTA_H),
                SANTA_PAL, SANTA_W, SANTA_H, 1);
}
static void grump_spr(int32_t x, int32_t yb, uint32_t ph)
{
    /* 4-frame walk cycle (~133 ms per pose at ~60 fps). All frames share the
     * same cell and ground line, so bottom-anchoring keeps him planted. */
    uint32_t frame = (ph / 8u) % GRUMP_NFRAMES;
    draw_sprite(x - GRUMP_W / 2, yb - GRUMP_H,
                GRUMP_PX + frame * (uint32_t)(GRUMP_W * GRUMP_H),
                GRUMP_PAL, GRUMP_W, GRUMP_H, 1);
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
    const uint16_t snow = RGB(232, 238, 246);
    const uint16_t shade = RGB(178, 190, 208);
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

/* The play mode, on every screen, centered below the name. The width comes
 * from an off-canvas probe draw (cv_text returns the end x; nothing is
 * painted that far above the band), so no text is ever painted off-centre. */
static void mode_label(void)
{
    const char *s = MODE_NAME[play_mode()];
    int32_t w = ttext(0, -5000, &FONT_XS, s, C_WHITE);
    ttext((240 - w) / 2, 24, &FONT_XS, s, C_HI);
}

static void draw_content(void)
{
    char t[32];
    uint32_t now = plat_ms();
    (void)now;
    ttext(10, 8, &FONT_B, APP_NAME, C_WHITE);
    snowfall(ui.frame);
    snow_ground(ui.state == ST_GRUMP ? 206 : 228, ui.frame);
    mode_label();
    if (plat_ms() < ui.msg_until && ui.msg[0]) {   /* the last announcement, briefly centered under the mode */
        int32_t w = ttext(0, -5000, &FONT_XS, ui.msg, C_WHITE);
        ttext((240 - w) / 2, 40, &FONT_XS, ui.msg, C_WHITE);
    }
    switch (ui.state) {
    case ST_TITLE: {
        frosty(198, 234, ui.frame);
        ttext(170, 92, &FONT_XS, "PRESS SEQ", C_GRAY);
        ttext(10, 40, &FONT_B, "LISTEN.", C_HI);
        ttext(10, 64, &FONT_B, "REPLAY.", C_HI);
        ttext(10, 88, &FONT_B, "PASS IT ON.", C_HI);
        if ((ui.frame / 60u) & 1u)   /* blink ~1 Hz: on for a second, off for a second */
            ttext(10, 112, &FONT_S, "PRESS START", RGB(120, 255, 140));
        ttext(10, 136, &FONT_S, "MASTER: VOLUME", C_WHITE);
        ttext(10, 160, &FONT_S, "GLO: MODE", C_WHITE);
        ttext(10, 184, &FONT_S, "SAVE: KEEP SCORES", C_WHITE);
        int_text((int32_t)(plat_battery() < 0 ? 0 : plat_battery()), t);
        ttext(10, 208, &FONT_XS, "BATTERY", C_GRAY);
        ttext(80, 208, &FONT_XS, t, C_HI);
        break;
    }
    case ST_SELECT: {
        santa_spr(198, 234, ui.frame, 0);
        ttext(10, 40, &FONT_XS, "CAROL (PRESET)", C_GRAY);
        ttext(10, 60, &FONT_B, CAROL_NAME[ui.carol], C_WHITE);
        int_text((int32_t)ui.carol + 1, t);
        ttext(10, 90, &FONT_XS, "NO", C_GRAY);
        ttext(40, 90, &FONT_XS, t, C_HI);
        ttext(90, 90, &FONT_XS, "OF 3", C_GRAY);
        int_text((int32_t)set.best_score[ui.carol], t);
        ttext(10, 120, &FONT_XS, "BEST", C_GRAY);
        ttext(50, 120, &FONT_XS, t, C_HI);
        ttext(10, 150, &FONT_XS, "PLAY: START  ARP: CODE", C_GRAY);
        code_text(t);
        ttext(10, 180, &FONT_XS, "CODE", C_GRAY);
        ttext(50, 180, &FONT_XS, t, C_HI);
        break;
    }
    case ST_CODE: {
        frosty(198, 234, ui.frame);
        ttext(10, 40, &FONT_XS, "CAROL CODE", C_GRAY);
        ttext(10, 60, &FONT_B, WORDS[ui.code_w[0]], C_WHITE);
        ttext(10, 88, &FONT_B, WORDS[ui.code_w[1]], C_WHITE);
        ttext(10, 120, &FONT_XS, ui.code_word ? "WORD 2 (SELECT to go)" : "WORD 1 (SELECT = next)", C_GRAY);
        ttext(10, 150, &FONT_XS, "PRESET: TURN WORD", C_GRAY);
        break;
    }
    case ST_LISTEN: {
        frosty(202, 234, ui.frame);
        ttext(10, 40, &FONT_B, CAROL_NAME[ui.carol], C_WHITE);
        ttext(10, 70, &FONT_XS, "LISTEN...", C_HI);
        int_text((int32_t)ui.listen_idx, t);
        ttext(10, 100, &FONT_XS, "NOTE", C_GRAY);
        ttext(60, 100, &FONT_XS, t, C_WHITE);
        int_text((int32_t)level_len(), t);
        ttext(90, 100, &FONT_XS, "OF ", C_GRAY);
        ttext(110, 100, &FONT_XS, t, C_GRAY);
        ttext(10, 130, &FONT_XS, "PLAY: SKIP", C_GRAY);
        break;
    }
    case ST_PLAY: {
        frosty(202, 234, ui.frame);
        ttext(10, 40, &FONT_B, CAROL_NAME[ui.carol], C_WHITE);
        ttext(10, 70, &FONT_XS, "YOUR TURN", C_HI);
        int_text((int32_t)ui.expect_idx, t);
        ttext(10, 100, &FONT_XS, "NOTE", C_GRAY);
        ttext(60, 100, &FONT_XS, t, C_WHITE);
        int_text((int32_t)level_len(), t);
        ttext(90, 100, &FONT_XS, "OF ", C_GRAY);
        ttext(110, 100, &FONT_XS, t, C_GRAY);
        int_text((int32_t)ui.score, t);
        ttext(10, 130, &FONT_XS, "SCORE", C_GRAY);
        ttext(70, 130, &FONT_XS, t, C_WHITE);
        int_text((int32_t)ui.misses, t);
        ttext(140, 130, &FONT_XS, "MISS", C_GRAY);
        ttext(180, 130, &FONT_XS, t, C_HI);
        int_text((int32_t)ui.combo, t);
        ttext(10, 160, &FONT_XS, "COMBO x", C_GRAY);
        ttext(80, 160, &FONT_XS, t, C_WHITE);
        break;
    }
    case ST_GRUMP: {
        grump_spr(120, 212, ui.frame);
        ttext(10, 40, &FONT_B, "GRUMP MODE", C_HI);
        ttext(10, 70, &FONT_XS, "HE JEERS. TRY AGAIN.", C_WHITE);
        ttext(10, 100, &FONT_XS, "PLAY: RETRY NOW", C_GRAY);
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
        code_text(t);
        ttext(10, 120, &FONT_XS, "PASS IT ON", C_GRAY);
        ttext(10, 140, &FONT_XS, t, C_HI);
        ttext(10, 170, &FONT_XS, "PLAY: NEXT SONG", C_GRAY);
        break;
    }
    case ST_RESULT: {
        santa_spr(198, 234, ui.frame, 1);
        draw_confetti();
        ttext(10, 40, &FONT_B, "LEVEL CLEAR", C_HI);
        int_text((int32_t)ui.score, t);
        ttext(10, 70, &FONT_XS, "SCORE", C_GRAY);
        ttext(70, 70, &FONT_XS, t, C_WHITE);
        int_text((int32_t)ui.level, t);
        ttext(10, 100, &FONT_XS, "LEVEL", C_GRAY);
        ttext(70, 100, &FONT_XS, t, C_WHITE);
        code_text(t);
        ttext(10, 140, &FONT_XS, "PASS IT ON", C_GRAY);
        ttext(10, 160, &FONT_XS, t, C_HI);
        ttext(10, 190, &FONT_XS, "PLAY: NEXT", C_GRAY);
        break;
    }
    case ST_SOUND: {
        frosty(198, 234, ui.frame);
        ttext(10, 40, &FONT_XS, "SELECT SOUND", C_GRAY);
        ttext(10, 60, &FONT_B, SND_NAME[snd_id()], C_WHITE);
        ttext(10, 90, &FONT_XS, "PRESET: CHANGE", C_GRAY);
        ttext(10, 120, &FONT_XS, "PLAY KEYS: AUDITION", C_GRAY);
        ttext(10, 150, &FONT_XS, "PLAY: DONE  SAVE: KEEP", C_GRAY);
        break;
    }
    case ST_ENV: {
        frosty(198, 234, ui.frame);
        ttext(10, 40, &FONT_XS, "ENVELOPE", C_GRAY);
        ttext(10, 60, &FONT_B, ui.env_idx == 0 ? "ATTACK" : ui.env_idx == 1 ? "DECAY" : "RELEASE", C_WHITE);
        int_text((int32_t)(ui.env_idx == 0 ? set.env_a : ui.env_idx == 1 ? set.env_d : set.env_r), t);
        ttext(10, 90, &FONT_XS, "VALUE", C_GRAY);
        ttext(60, 90, &FONT_XS, t, C_HI);
        ttext(10, 120, &FONT_XS, "PRESET: PARAM  SELECT: VALUE", C_GRAY);
        ttext(10, 150, &FONT_XS, "PLAY: DONE  SAVE: KEEP", C_GRAY);
        break;
    }
    case ST_GRAIN: {
        frosty(198, 234, ui.frame);
        ttext(10, 40, &FONT_XS, "GRAIN", C_GRAY);
        int_text((int32_t)set.grain, t);
        ttext(10, 60, &FONT_B, t, C_WHITE);
        ttext(10, 90, &FONT_XS, "PRESET: AMOUNT 0..100", C_GRAY);
        ttext(10, 120, &FONT_XS, "0 = CLEAN SAMPLE", C_GRAY);
        ttext(10, 150, &FONT_XS, "PLAY: DONE  SAVE: KEEP", C_GRAY);
        break;
    }
    default:
        break;
    }
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
     * 6 px above them), with the name clear above his hat. */
    snow_ground(196, 0);
    draw_sprite(120 - FROSTY_W / 2, 202 - FROSTY_H,
                FROSTY_PX + (uint32_t)(FROSTY_W * FROSTY_H),
                FROSTY_PAL, FROSTY_W, FROSTY_H, 1);
    ttext(68, 36, &FONT_B, APP_NAME, APP_SPLASH_RGB);
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
    else if (!strcmp(what, "env_a")) *got = (int)set.env_a;
    else if (!strcmp(what, "env_d")) *got = (int)set.env_d;
    else if (!strcmp(what, "env_r")) *got = (int)set.env_r;
    else if (!strcmp(what, "grain")) *got = (int)set.grain;
    else if (!strcmp(what, "mid")) {     /* EASY's steady key in PLAY (else -1) */
        int32_t nk = -1;
        if (play_mode() == MODE_EASY && ui.state == ST_PLAY)
            nk = (int32_t)song_note((uint8_t)ui.expect_idx) - 53;
        *got = (int)nk;
    }
    else if (!strcmp(what, "w1")) *got = (int)set.last_w1;
    else if (!strcmp(what, "w2")) *got = (int)set.last_w2;
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
