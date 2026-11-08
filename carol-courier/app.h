/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* Carol Courier: a Christmas phrase game for the FM-1. Own firmware, built from
 * the fm1-base template. This header is the app's half of the contract with the
 * platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "CAROL COURIER"
#define APP_SPLASH
#define APP_SPLASH_RGB RGB(196, 30, 40)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */

/* the store: the app's settings/scores and the panel calibration (the platform's),
 * with their flash offsets. OBJ_PANEL must stay index 1 (device/panel.c). */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

/* backup/restore of the saved objects over USB (fm1.toml backup = true) */
#define APP_BACKUP_OBJS {OBJ_SETTINGS}
#define APP_BACKUP_MAGIC 0x42524343u          /* "CCRB" */
#define APP_BACKUP_VERSION 2u
#define APP_BACKUP_WHAT "replaces scores, codes and settings"
int app_backup_playing(void);
void app_backup_flush(void);
void app_backup_hold(int on);
void app_backup_restored(void);

/* the resource region (fm1.toml resources = "art/carol_res.bin"): the
 * recorded sounds, read in place via plat_res. 68 sectors from 0x98000,
 * ending exactly at the panel (0xDC000) and clear of the settings (0xFC000). */
#define APP_RES_BASE 0x98000u
#define APP_RES_SECTORS 68u

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved settings (or defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */
void app_splash(void);                   /* the boot splash: the name, Frosty waving, in the snow */

/* the host simulator's checks (host/plat_host.c "expect") */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
