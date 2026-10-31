/* The show-off cut: source ranges of the recording (seconds) and captions
 * (seconds from the start of each range).  The recording itself is
 * disc-derived and stays outside the repository (--public-dir). */
export type CaptionDef = {at: number; dur: number; title: string; sub?: string};
export type Segment = {src: string; from: number; to: number; captions?: CaptionDef[]};

const REC = 'rec3.mp4';	/* take 3, scripts/dsfpc/record_demo.py */

export const edit: {intro: number; outro: number; segments: Segment[]; outroPoints: string[]; outroNote: string} = {
	intro: 4.5,
	outro: 8,
	segments: [
		{src: REC, from: 18.5, to: 24.3, captions: [
			{at: 0.5, dur: 5, title: 'StepMania 3.95 is the host', sub: 'The 2005 engine (CVS snapshot) with a Dancing Stage Fusion PC theme'}]},
		{src: REC, from: 24.8, to: 34.0, captions: [
			{at: 2.4, dur: 6.4, title: "StepMania's own key & dance-pad mapping", sub: 'Keyboard, dance mats and joysticks, for both players'}]},
		{src: REC, from: 46.8, to: 64.5, captions: [
			{at: 0.6, dur: 7, title: 'The original PS1 game boots inside StepMania', sub: 'Recompiled from my own disc. No Konami data is shipped.'},
			{at: 9.5, dur: 6.5, title: 'Its own screens, untouched', sub: 'Memory card, caution screen, title: exactly as on the PlayStation'}]},
		{src: REC, from: 65.2, to: 69.0},
		{src: REC, from: 71.6, to: 92.3, captions: [
			{at: 4.4, dur: 7, title: "StepMania's settings in the game's own OPTIONS", sub: 'New rows drawn with the game’s own font, cursor, sounds and help text'},
			{at: 12.0, dur: 8, title: 'Changes go straight to StepMania', sub: 'Volume, audio buffer and picture sync, through StepMania’s own option code'}]},
		{src: REC, from: 98.0, to: 117.6, captions: [
			{at: 1.0, dur: 7, title: 'A new page in GRAPHIC OPTIONS', sub: 'Window or full screen, resolution, refresh rate, vsync, renderer'},
			{at: 9.5, dur: 8.5, title: 'In all five of the game’s languages', sub: 'English, French, German, Italian and Spanish, like the rest of the menus'}]},
		{src: REC, from: 140.6, to: 147.5},
		{src: REC, from: 154.0, to: 174.0, captions: [
			{at: 1.0, dur: 7, title: 'Full speed, paced by the sound card', sub: 'The game runs as fast as the PC plays its audio, so it can’t drift from the music'}]},
		{src: REC, from: 190.6, to: 246.4, captions: [
			{at: 3.0, dur: 7, title: 'The original gameplay', sub: 'Step charts, timing, judgements and dancers from the PS1 game'},
			{at: 30.0, dur: 7, title: 'Attract-mode DEMO', sub: '“Come Into My World”, Kylie Minogue'}]},
	],
	outroPoints: [
		'The original PS1 game, recompiled from your own disc',
		'Running inside StepMania 3.95 (2005 CVS engine)',
		'Keyboard & dance pads through StepMania’s input mapping',
		'PC settings added to the game’s own OPTIONS menus',
		'Timing paced by the sound card: no drift from the music',
	],
	outroNote: 'Recorded under Wine on a Linux machine with no graphics card, so the game is shown with its software renderer at native resolution. ' +
		'With a graphics card its 3D renders at up to 1440p. Fan project, not affiliated with Konami. No game data is distributed.',
};
