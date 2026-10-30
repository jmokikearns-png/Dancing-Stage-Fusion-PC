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
 *   - get_mapping() / begin_mapping() / mapping_state() / clear_mapping():
 *     the KEY CONFIG pages in the game's CONTROLLER SETTINGS, on top of the
 *     host's key / pad mapping.
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

#define DSFCORE_API_VERSION 8

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
	DSFCORE_SETTING_AUTO_TIMING,		/* the core keeps TIMING right (see measure_latency) */
	DSFCORE_SETTING_SMOOTH_ARROWS,		/* v7: in-between frames for the step field */
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

/* The PC's own delays, for AUTO TIMING: how long after the game makes a
 * sound it is heard (the host's queue and the sound device's latency), and
 * after a picture is made it is on the screen (an estimate from the refresh
 * rate and vsync).  What the PC can't know (a TV's or headphones' delay) is
 * left to the core, which learns it from the player's steps. */
typedef struct DsfCoreLatency
{
	int32_t sound_ms;
	int32_t picture_ms;
	int32_t refresh_hz;		/* 0 when unknown */
	int32_t taps;			/* v8: sound_ms from a tap test with this many taps; 0: estimated */
} DsfCoreLatency;

/* Buttons of the KEY CONFIG pages: the PS1 pad.  PC_MENU is StepMania's Back,
 * which the pages no longer list (QUIT on the main menu replaced the PC menu). */
enum {
	DSFCORE_BUTTON_UP = 0, DSFCORE_BUTTON_DOWN, DSFCORE_BUTTON_LEFT, DSFCORE_BUTTON_RIGHT,
	DSFCORE_BUTTON_CROSS, DSFCORE_BUTTON_CIRCLE, DSFCORE_BUTTON_SQUARE, DSFCORE_BUTTON_TRIANGLE,
	DSFCORE_BUTTON_L1, DSFCORE_BUTTON_L2, DSFCORE_BUTTON_R1, DSFCORE_BUTTON_R2,
	DSFCORE_BUTTON_START, DSFCORE_BUTTON_SELECT, DSFCORE_BUTTON_PC_MENU,
	DSFCORE_BUTTON_COUNT
};

/* What is mapped to one button: the player's own two slots, then the
 * default.  Plain ASCII as the host names the input ("KEY X", "JOY1 B3"),
 * "" for none. */
#define DSFCORE_MAPPING_SLOTS 3
typedef struct DsfCoreMapping
{
	char slot[DSFCORE_MAPPING_SLOTS][24];
} DsfCoreMapping;

enum {
	DSFCORE_CAPTURE_IDLE = 0,
	DSFCORE_CAPTURE_WAITING = 1,	/* the next key / pad button pressed gets mapped */
	DSFCORE_CAPTURE_DONE = 2,
	DSFCORE_CAPTURE_CANCELLED = 3	/* Esc */
};

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
	/* Key / pad mapping (player 0/1, DSFCORE_BUTTON_*).  begin: the next
	 * press goes to one of the player's own slots (the game gets no input
	 * meanwhile); clear: empties them, the default stays.  Changes are saved
	 * at once. */
	int (*get_mapping)( void *user, int player, int button, DsfCoreMapping *out );
	void (*begin_mapping)( void *user, int player, int button );
	int (*mapping_state)( void *user );
	void (*clear_mapping)( void *user, int player, int button );
	/* The player chose QUIT on the game's main menu: leave the program as
	 * the host's own exit does.  The core keeps running until then. */
	void (*quit)( void *user );
	/* v6: the delays as measured now; 0 when the host can't tell.  v8: the
	 * host may run a tap test first (the call then takes some seconds). */
	int (*measure_latency)( void *user, DsfCoreLatency *out );
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
