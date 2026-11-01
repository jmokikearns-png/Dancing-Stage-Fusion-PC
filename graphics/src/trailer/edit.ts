/* The trailer's cut list, on the song's bars (beat.ts).  Footage: the
 * release build recorded under a test harness (a: Rapper's Delight,
 * PARANOIA MAX, AM-3P; b: Heaven and Earth, Kids in America, Tough Enough;
 * c: Go West, Mickey, PARANOIA survivor; o: the OPTIONS pages; m: the
 * StepMania splash and title menu).  Times in the clips are seconds. */
import {BAB, barT, beatT} from './beat';

export type Scene = {
	from: number; to: number;
	kind: 'intro' | 'logo' | 'setup' | 'songs' | 'shot' | 'words';
	src?: string; at?: number; rate?: number; still?: boolean; full?: boolean; zoom?: number;
	hue?: 'green' | 'orange' | 'red' | 'blue';
	slam?: string; slamSub?: string; slamSize?: number; slamDelay?: number; slamY?: number;
	tag?: string; tagSub?: string; tagRight?: boolean; tagY?: number;
	flash?: boolean | 'big';
};

const B = barT;
const shot = (from: number, to: number, src: string, at: number, extra: Partial<Scene> = {}): Scene =>
	({from, to, kind: 'shot', src, at, ...extra});

/* The cold open, before the song: A Stupid Barber chosen on the music
 * select, with the game's own sound.  clips/select.mp4 = recording s from
 * 63.0 s.  Its sound leads its picture by AV_LEAD (the preview stops 0.39 s
 * before the picture fades on the confirm; the receptors' beat pulse agrees).
 * The song starts at 11.01 s in its sound = 11.40 s in its picture. */
export const COLD_SRC = 'clips/select.mp4';
export const AV_LEAD = 0.39;
export const COLD: Array<[number, number]> = [[0.6, 5.0], [7.0, 7.75], [10.6, 11.40]];	/* picture times */
export const PRE = COLD.reduce((n, [a, b]) => n + (b - a), 0);
export const SONG_IN_COLD = 11.40;

export const MENU = {
	src: 'm.mp4',
	cut: 13.46,	/* the title menu cuts in */
	pcStart: 13.84,	/* the "PC" stamp starts */
	pcLand: 13.97,	/* ... and lands */
	endCardAt: 100.6,
};

const CLIPS = {
	splash: ['m.mp4', 8.4],
	title: ['a.mp4', 46.5],
	selectScroll: ['a.mp4', 62.6],
	rapper: ['a.mp4', 118.0],
	rapper2: ['a.mp4', 150.0],
	cleared: ['a.mp4', 185.0],
	paranoiaMax: ['a.mp4', 236.0],
	paranoiaMax2: ['a.mp4', 262.0],
	am3p: ['a.mp4', 372.0],
	am3p2: ['a.mp4', 420.0],
	soundPage: ['o.mp4', 75.8],	/* AUDIO BUFFER -> PICTURE SYNC */
	autoTiming: ['o.mp4', 81.5],
	displayPage: ['o.mp4', 118.9],	/* GAME RESOLUTION -> RENDERER */
	smoothRow: ['o.mp4', 122.9],
	selectNew: ['d.mp4', 211.3],	/* the list: Freak Like Me -> Go West */
	tough: ['d.mp4', 79.6],
	tough2: ['d.mp4', 131.0],
	goWest: ['d.mp4', 222.2],
	goWest2: ['d.mp4', 268.0],
	mickey: ['d.mp4', 359.2],
	mickey2: ['d.mp4', 402.0],
	kids: ['c.mp4', 373.8],
	kids2: ['c.mp4', 420.0],
	healing: ['c.mp4', 250.0],
	rapperHard: ['c.mp4', 140.0],
	iBelieve: ['b.mp4', 150.0],
	superstar: ['e.mp4', 430.0],
	heavenSelect: ['e.mp4', 213.6],
	heaven: ['e.mp4', 223.4],
	heaven2: ['e.mp4', 268.0],
	survivor: ['e.mp4', 101.0],
	survivor2: ['e.mp4', 152.0],
} as const;
/* Each clip is cut beforehand from its recording (CLIPS: file, second) to
 * clips/<name>.mp4, 4.2 s long, so the bundle stays small. */
const c = (k: keyof typeof CLIPS, from: number, to: number, extra: Partial<Scene> = {}) =>
	shot(from, to, `clips/${k}.mp4`, 0, extra);

export const cuts: Scene[] = [
	shot(0, B(2), COLD_SRC, SONG_IN_COLD, {hue: 'green', flash: false}),	/* the game's own stage start, on the song */
	{from: B(2), to: B(3), kind: 'words', slam: 'A MASOCHISTIC\nEXPERIMENT', slamSize: 120},
	{from: B(3), to: B(4), kind: 'words', slam: '22 YEARS\nIN THE MAKING', slamSize: 120, hue: 'orange'},
	{from: B(4), to: B(6), kind: 'logo', flash: 'big'},
	{from: B(6), to: B(8), kind: 'setup', tag: 'BUILT FROM YOUR OWN DISC', tagSub: 'Setup.exe recompiles it on your PC', tagY: 40},
	c('splash', B(8), B(10), {hue: 'orange', tag: 'FREE & OPEN SOURCE', tagSub: 'nothing of the game is included'}),
	c('title', B(10), B(12), {hue: 'green', tag: 'THE ORIGINAL GAME', tagSub: 'recompiled to native PC code'}),
	c('rapper', B(12), B(14), {hue: 'red', tag: 'INSIDE STEPMANIA', tagSub: 'the 2005 engine, its input and sound'}),
	c('paranoiaMax', B(14), B(16), {hue: 'blue', tag: 'THE ORIGINAL GAMEPLAY', tagSub: 'charts, timing and scoring', tagRight: true}),
	c('am3p', B(16), B(18), {hue: 'green', tag: 'SMOOTH ARROWS', tagSub: 'every frame at 50 FPS'}),
	c('paranoiaMax2', B(18), B(20), {hue: 'red', tag: 'KEYBOARD & DANCE PADS', tagSub: "through StepMania's own mappings", tagRight: true}),
	{from: B(20), to: B(21), kind: 'words', slam: '12 NEW SONGS', slamSize: 170, flash: 'big'},
	c('selectNew', B(21), B(22), {hue: 'green'}),
	c('tough', B(22), B(24), {hue: 'red', tag: 'TOUGH ENOUGH', tagSub: 'Vanilla Ninja - a new background'}),
	c('goWest', B(24), B(26), {hue: 'orange', tag: 'GO WEST', tagSub: 'Pet Shop Boys', tagRight: true}),
	c('kids', B(26), B(28), {hue: 'blue', tag: 'KIDS IN AMERICA', tagSub: 'Kim Wilde'}),
	c('heavenSelect', B(28), B(29), {hue: 'green', tag: 'UNLOCK IT', tagSub: 'clear PARANOIA survivor on DIFFICULT', tagRight: true}),
	c('heaven', B(29), B(31), {hue: 'orange', tag: 'HEAVEN AND EARTH', tagSub: 'Pop!'}),
	c('mickey', B(31), B(33), {hue: 'red', tag: 'MICKEY', tagSub: 'a new background for a song of the disc', tagRight: true}),
	c('soundPage', B(33), B(35), {hue: 'orange', tag: 'PC SETTINGS', tagSub: "in the game's own OPTIONS"}),
	c('autoTiming', B(35), B(37), {hue: 'red', tag: 'AUTO TIMING', tagSub: 'tap along: it measures your delay', tagRight: true}),
	c('displayPage', B(37), B(39), {hue: 'blue', tag: 'UP TO 1440p', tagSub: 'with a graphics card'}),
	{from: B(39), to: B(41), kind: 'songs', tag: 'BUILD NEW SONGS', tagSub: 'from your own simfile packs', tagY: 40},
	c('survivor', B(41), B(43), {hue: 'green', tag: 'PARANOIA SURVIVOR', tagSub: 'and more'}),
	c('rapper2', B(43), B(45), {hue: 'red'}),
	/* faster: two beats a shot */
	...(['tough2', 'goWest2', 'kids2', 'heaven2', 'mickey2', 'am3p2', 'survivor2', 'healing'] as const).map((k, i) =>
		c(k, beatT(180 + 2 * i), beatT(182 + 2 * i), {hue: (['red', 'orange', 'blue', 'green'] as const)[i % 4], flash: true})),
	/* faster still: a beat a shot */
	...(['rapperHard', 'tough', 'goWest', 'kids', 'heaven', 'mickey', 'survivor', 'am3p'] as const).map((k, i) =>
		c(k, beatT(196 + i), beatT(197 + i), {hue: (['red', 'orange', 'blue', 'green'] as const)[i % 4], full: i % 2 === 1})),
	...(['paranoiaMax2', 'kids2', 'tough2', 'heaven2', 'goWest2', 'mickey2', 'iBelieve', 'survivor2'] as const).map((k, i) =>
		c(k, beatT(204 + i), beatT(205 + i), {hue: (['blue', 'green', 'red', 'orange'] as const)[i % 4], full: i % 2 === 0})),
	/* the build-up: a word a beat */
	/* (the box art: "The original and still the best") */
	...['THE ORIGINAL', 'AND', 'EVEN', 'BETTER.'].map((w, i): Scene =>
		({from: beatT(212 + i), to: i === 3 ? BAB[0] : beatT(213 + i), kind: 'words', slam: w, slamSize: [175, 240, 250, 250][i], flash: i === 3 ? 'big' : true,
			hue: (['red', 'orange', 'red', 'green'] as const)[i]})),
];
