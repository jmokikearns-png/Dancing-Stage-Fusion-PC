/* dsfcore_api.h -- the recompiled Dancing Stage Fusion as a library.
 *
 * dsfcore.dll is the original game (recompiled from the player's own disc) on
 * the PSXRecomp runtime in its embedded mode.  The host -- the port's
 * StepMania -- owns the window, sound output, input and pacing:
 *
 *   - dsfcore_start() boots the PlayStation on a thread of its own.
 *   - At every guest VBlank the core calls video() with the displayed frame,
 *     then frame_done(), which must block until the host wants the next frame
 *     and returns the pad words to use for it.
 *   - audio() receives the SPU's output (44.1 kHz, signed 16-bit, stereo) as
 *     it is produced: 882 frames per PAL video frame.
 *   - read_pads() is called from inside the game's own pad-read routine (a
 *     hook, dsfcore_hooks.cpp): the game takes its buttons straight from the
 *     host's input mapping at the moment it reads them.
 *   - get_setting() / set_setting() / commit_settings() let the game's own
 *     OPTIONS screens show and change the host's PC settings (rows added to
 *     them by dsfcore_menus.cpp).
 *
 * All callbacks run on the core's thread.  The host loads the DLL at run time
 * (LoadLibrary + GetProcAddress("dsfcore_start")), so the StepMania program
 * itself contains no game code.
 */
#ifndef DSFCORE_API_H
#define DSFCORE_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DSFCORE_API_VERSION 3

enum {
	DSFCORE_FRAME_BLANK = 1,	/* display disabled: show black */
	DSFCORE_FRAME_24BIT = 2		/* 24-bit (movie) scanout at native size */
};

/* PC settings shown in the game's OPTIONS screens. */
enum {
	DSFCORE_SETTING_VOLUME = 0,
	DSFCORE_SETTING_AUDIO_BUFFER,
	DSFCORE_SETTING_SYNC_PICTURE,		/* show each frame when its sound is heard */
	DSFCORE_SETTING_PICTURE_OFFSET,
	DSFCORE_SETTING_SCREEN_MODE,		/* full screen / window */
	DSFCORE_SETTING_RESOLUTION,
	DSFCORE_SETTING_REFRESH_RATE,
	DSFCORE_SETTING_VSYNC,
	DSFCORE_SETTING_RENDER_QUALITY,		/* the game's internal resolution */
	DSFCORE_SETTING_RENDERER,
	DSFCORE_SETTING_COUNT
};

/* What a setting shows.  Choice text is plain ASCII as the host names it
 * ("12 ms", "1280x960", "ON"); the core translates and upper-cases it. */
typedef struct DsfCoreSetting
{
	int32_t choice;			/* current choice */
	int32_t num_choices;
	int32_t default_choice;		/* -1 when there is none */
	int32_t applies_later;		/* 1: takes effect the next time the game starts */
	char text[40];			/* the current choice */
} DsfCoreSetting;

enum {
	DSFCORE_STATE_NOT_STARTED = 0,
	DSFCORE_STATE_RUNNING = 1,
	DSFCORE_STATE_EXITED = 2	/* boot failed or the runtime returned; see dsfcore_exit_code() */
};

typedef struct DsfCoreHost
{
	uint32_t struct_size;		/* sizeof(DsfCoreHost) */
	uint32_t api_version;		/* DSFCORE_API_VERSION */
	void *user;
	/* 0xAARRGGBB pixels, pitch in pixels; scale = internal resolution factor */
	void (*video)( void *user, const uint32_t *argb, int width, int height, int pitch_px, int scale, int flags );
	void (*audio)( void *user, const int16_t *stereo, int frames );
	/* Block until the next frame may run; fill pads[0..1] (active-low PS1 pad words). */
	void (*frame_done)( void *user, uint16_t pads_out[2] );
	/* The buttons right now, for the game's own pad read (active-low). */
	void (*read_pads)( void *user, uint16_t pads_out[2] );
	/* PC settings (DSFCORE_SETTING_*).  get: 0 when the host doesn't have it.
	 * set: takes effect at once where that is cheap (volume, picture sync);
	 * display changes wait for commit (leaving the screen), like StepMania's
	 * own option screens.  commit: apply and save what changed. */
	int (*get_setting)( void *user, int id, DsfCoreSetting *out );
	void (*set_setting)( void *user, int id, int choice );
	void (*commit_settings)( void *user );
} DsfCoreHost;

/* argv[0] is ignored; the rest are runtime options (--game, --disc, --bios,
 * --memcard-dir ...).  stdout/stderr of the core go to log_path when given.
 * Returns 0 when the core thread was started. */
typedef int (*dsfcore_start_fn)( const DsfCoreHost *host, int argc, const char *const *argv, const char *log_path );
typedef int (*dsfcore_state_fn)( void );
typedef int (*dsfcore_exit_code_fn)( void );

#ifdef __cplusplus
}
#endif

#endif
