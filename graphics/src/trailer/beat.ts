/* The trailer's clock: A Stupid Barber, 132 BPM, beats at 0.01 s + k * 60/132
 * (fitted to the decoded stream's onsets), 50 frames a second. */
export const FPS = 50;
export const BPM = 132;
export const BEAT = 60 / BPM;
export const PHASE = 0.01;
export const BAR = 4 * BEAT;

export const f = (s: number) => Math.round(s * FPS);
export const beatT = (k: number) => PHASE + k * BEAT;
export const barT = (n: number) => PHASE + n * BAR;
export const beatF = (k: number) => f(beatT(k));
export const barF = (n: number) => f(barT(n));

/* The last "beh um bab um bab": five syllables, the strongest onsets of the
 * song's last bars (decoded audio, 300 Hz - 3 kHz spectral flux). */
export const BAB = [98.19, 98.42, 98.63, 98.98, 99.43];
export const SONG_END = 101.4;
export const END_CARD = 6.5;	/* the "Now available" card, after the menu */
