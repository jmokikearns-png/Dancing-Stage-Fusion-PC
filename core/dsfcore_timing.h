/* dsfcore_timing.h -- AUTO TIMING (dsfcore_timing.cpp). */
#ifndef DSFCORE_TIMING_H
#define DSFCORE_TIMING_H

/* What the PC scan found and set. */
struct AutoTimingScan
{
	int sound_ms, picture_ms;	/* measured delays */
	int timing;			/* the game's TIMING now (-8..8, 1/75 s) */
	int picture_delay_ms;		/* PICTURE DELAY now */
	int taps;			/* the tap test's taps; 0: the sound delay is the PC's estimate */
};

/* Measure the PC's delays and set PICTURE SYNC, PICTURE DELAY and TIMING
 * from them.  False when the host can't measure. */
bool dsf_auto_timing_scan( AutoTimingScan &out );

/* Every game pad read: learns from the player's steps while a song plays
 * and, when AUTO TIMING is on, corrects TIMING when the song ends. */
void dsf_auto_timing_frame();

#endif
