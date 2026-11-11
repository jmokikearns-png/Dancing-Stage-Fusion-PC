import React from 'react';
import {AbsoluteFill, Audio, interpolate, Sequence, spring, staticFile, useCurrentFrame} from 'remotion';
import '../fonts';
import './serpentine';
import {colors, fonts} from '../tokens';
import {Intro, Logo, LogoArt} from './cards';
import {Flash, Shot, Slam, Stripes, Tag, Title} from './parts';

/* The 1.1 trailer, cut like the 1.0 one: 321STARS (DJ Simon, from the
 * player's disc, stream 3: "str_decode.py 3 stars.wav"), 192 BPM.  The
 * opening count is six vocal hits on beats 4, 6, 8, 9, 10 and 11 and the
 * drums come in on beat 12 (3.745 s), the first downbeat: beat k is at
 * k * 60/192 - 0.005 s (fitted to the decoded stream), so cuts every 4 beats
 * land on the bars.  Shots are 12 beats, words 8, the montage 2 and then 1.  Footage: the 1.1
 * build (clips/v_*.mp4 and g_*.mp4, cut at the times in edit11.json).  The
 * game ran at about 1/1.4 of its speed while it was recorded, so its
 * footage plays at RATE.  No white flashes in the last stretch. */
export const RELEASE11_FPS = 50;
const BEAT = 60 / 192;
const PHASE = -0.005;
const RATE = 1.4;
const f = (s: number) => Math.round(s * RELEASE11_FPS);
const beatT = (k: number) => PHASE + k * BEAT;
const B = beatT;	/* cut points, in beats */
const clamp = {extrapolateLeft: 'clamp', extrapolateRight: 'clamp'} as const;
/* The song's end: a build (beats 188-204), the drop into its melodic last
 * section (beat 204, 63.7 s), then its own fade-out (from beat 252, 78.7 s;
 * silent from about 79.8 s): the end card rides the fade. */
const END_AT = B(236), END_LEN = B(258) - B(236);
export const release11Frames = () => f(END_AT + END_LEN);

type Hue = 'green' | 'orange' | 'red' | 'blue';
type Scene = {
	from: number; to: number;
	kind: 'intro' | 'logo' | 'shot' | 'words';
	src?: string; rate?: number; full?: boolean; hue?: Hue;
	slam?: string; slamSub?: string; slamSize?: number;
	tag?: string; tagSub?: string; tagRight?: boolean; tagY?: number;
	flash?: boolean | 'big';
};
const shot = (from: number, to: number, src: string, extra: Partial<Scene> = {}): Scene =>
	({from, to, kind: 'shot', src: `clips/${src}.mp4`, rate: RATE, ...extra});
const words = (from: number, to: number, slam: string, extra: Partial<Scene> = {}): Scene =>
	({from, to, kind: 'words', slam, ...extra});
/* where the game's own pop-up is at the top of the picture */
const LOW = {tagRight: true, tagY: 800};

const HUES: Hue[] = ['red', 'orange', 'blue', 'green'];
const cuts: Scene[] = [
	{from: 0, to: B(12), kind: 'intro', flash: false},
	{from: B(12), to: B(24), kind: 'logo', flash: 'big'},
	words(B(24), B(32), 'Version 1.1', {slamSize: 190, hue: 'green'}),
	words(B(32), B(40), 'ACHIEVEMENTS', {slamSize: 170, hue: 'orange'}),
	shot(B(40), B(52), 'v_title', {rate: 1, hue: 'green', flash: 'big', tag: 'ACHIEVEMENTS', tagSub: '25 to earn, on the title menu'}),
	shot(B(52), B(64), 'v_card', {hue: 'blue', tag: 'YOUR RECORDS COUNT', tagSub: "your memory card's clears unlock at once", ...LOW}),
	shot(B(64), B(76), 'g_h1', {hue: 'red', tag: 'EARN AS YOU PLAY', tagSub: 'clears, full combos, grades, combos'}),
	shot(B(76), B(88), 'v_res1', {hue: 'orange', tag: 'UNLOCKED!', tagSub: 'each one pops up on the results screen', ...LOW}),
	shot(B(88), B(100), 'g_k1', {hue: 'blue', tag: 'EVERY MODE COUNTS', tagSub: 'DOUBLE, WORKOUT and LESSON too', tagRight: true}),
	words(B(100), B(108), '25 TO EARN', {slamSize: 200, hue: 'red', flash: 'big'}),
	shot(B(108), B(120), 'v_list', {rate: 1, hue: 'green', tag: 'TRACK YOUR PROGRESS', tagSub: 'every one, earned or not'}),
	words(B(120), B(128), 'NEW!!', {slamSize: 260, hue: 'orange', flash: 'big'}),
	shot(B(128), B(140), 'v_new', {hue: 'red', tag: 'NEWLY UNLOCKED', tagSub: 'marked until you play them, like the Deluxe disc'}),
	shot(B(140), B(152), 'g_h2', {hue: 'green', tag: 'HEAVEN AND EARTH', tagSub: 'unlocked by PARANOIA survivor', tagRight: true}),
	shot(B(152), B(164), 'g_k2', {hue: 'orange', flash: 'big'}),
	shot(B(164), B(176), 'v_res2', {hue: 'blue', ...LOW}),
	shot(B(176), B(188), 'g_h3', {hue: 'red', tag: 'KEPT ON YOUR PC', tagSub: 'a new memory card takes nothing away'}),
	/* the build: two beats a shot, then one */
	...['g_h4', 'g_k3'].map((k, i) => shot(B(188 + 2 * i), B(190 + 2 * i), k, {hue: HUES[i % 4], flash: true})),
	...['g_h5', 'g_k4', 'g_h6', 'g_k5'].map((k, i) =>
		shot(B(192 + i), B(193 + i), k, {hue: HUES[(i + 2) % 4], full: i % 2 === 1, flash: false})),
	/* the end of the build: a word every two beats */
	...['PLAY.', 'EARN.', 'UNLOCK.', 'REPEAT.'].map((w, i): Scene =>
		words(B(196 + 2 * i), B(198 + 2 * i), w, {slamSize: [220, 230, 210, 220][i], hue: HUES[i], flash: false})),
	/* the drop into the melodic last section: a shot a phrase */
	shot(B(204), B(212), 'g_k6', {hue: 'orange', full: true, flash: false}),
	shot(B(212), B(220), 'g_h6', {hue: 'green', flash: false}),
	shot(B(220), B(228), 'g_k5', {hue: 'blue', flash: false}),
	shot(B(228), B(236), 'g_h5', {hue: 'red', full: true, flash: false}),
];

/* The opening count, a word a vocal hit (beats), each slammed in on its hit,
 * with an arrow hitting the receptors. */
const COUNT: Array<[number, string]> = [[4, '1'], [6, '2'], [8, '3'], [9, '2'], [10, '1'], [11, 'YEAH!']];
const COUNT_ARROWS = [[0], [3], [1], [2], [0, 3], [1, 2]];
const Count: React.FC = () => (
	<>
		{COUNT.map(([k, w], i) => {
			const from = f(B(k)), to = i + 1 < COUNT.length ? f(B(COUNT[i + 1][0])) : f(B(12));
			return (
				<Sequence key={i} from={from} durationInFrames={to - from} layout="none">
					<Slam text={w} size={w === 'YEAH!' ? 230 : 300} y={150} dur={to - from} />
				</Sequence>
			);
		})}
	</>
);

const SceneView: React.FC<{s: Scene; absStart: number; len: number}> = ({s, absStart, len}) => {
	switch (s.kind) {
		case 'intro': return <Intro beat={BEAT} phase={PHASE} hits={COUNT.map(([k], i) => [k, COUNT_ARROWS[i]] as [number, number[]])} />;
		case 'logo': return <Logo len={len} />;
		case 'shot': return <Shot src={s.src!} from={0} len={len} hue={s.hue} full={s.full} absStart={absStart} rate={s.rate}
			beat={BEAT} phase={PHASE} />;
		case 'words': return <Stripes hue={s.hue || 'red'} speed={3} dim={0.25} />;
		default: return null;
	}
};

const FootageNote: React.FC<{len: number}> = ({len}) => {
	const frame = useCurrentFrame();
	const o = interpolate(frame, [0, 15, len - 15, len], [0, 1, 1, 0], clamp);
	return (
		<AbsoluteFill style={{justifyContent: 'flex-end', alignItems: 'center', paddingBottom: 18, opacity: o}}>
			<div style={{background: 'rgba(8,6,6,0.78)', borderRadius: 8, padding: '7px 18px', fontFamily: fonts.body, fontWeight: 500,
				fontSize: 22, color: 'rgba(255,255,255,0.9)'}}>
				Note: the gameplay footage got choppy during recording and editing. The game itself is not this choppy.
			</div>
		</AbsoluteFill>
	);
};

/* The end card: the logo, "Version 1.1", where to get it. */
const EndCard11: React.FC<{len: number}> = ({len}) => {
	const frame = useCurrentFrame();
	const fps = RELEASE11_FPS;
	const a = spring({frame, fps, config: {damping: 12, stiffness: 200, mass: 0.7}});
	const v = spring({frame: frame - 8, fps, config: {damping: 11, stiffness: 240, mass: 0.6}});
	const b = spring({frame: frame - 18, fps, config: {damping: 14, stiffness: 160}});
	const c = spring({frame: frame - 34, fps, config: {damping: 18, stiffness: 120}});
	const out = Math.min(interpolate(frame, [len - 25, len], [1, 0], clamp), interpolate(frame, [0, 10], [0, 1], clamp));
	return (
		<AbsoluteFill style={{opacity: out}}>
			<Stripes hue="green" speed={0.5} dim={0.15} />
			<AbsoluteFill style={{justifyContent: 'center', alignItems: 'center', flexDirection: 'column'}}>
				<div style={{transform: `scale(${1.12 - 0.12 * a})`, opacity: Math.min(1, a * 2.5)}}>
					<div style={{filter: 'drop-shadow(0 16px 24px rgba(0,0,0,0.6))'}}><LogoArt k={2.1} frame={frame + 30} /></div>
				</div>
				<div style={{marginTop: 18, transform: `scale(${1.6 - 0.6 * v})`, opacity: Math.min(1, v * 2)}}>
					<Title text="Version 1.1" size={78} />
				</div>
				<div style={{marginTop: 22, transform: `translateY(${(1 - b) * 120}px) skewX(-10deg)`, opacity: b}}>
					<div style={{background: `linear-gradient(180deg, ${colors.orangeLight} 0%, ${colors.orange} 40%, ${colors.red} 100%)`,
						padding: '16px 56px 22px', borderRadius: 10, border: '5px solid #1a0503', boxShadow: '0 12px 0 rgba(0,0,0,0.45)'}}>
						<div style={{transform: 'skewX(10deg)'}}><Title text="Now available on GitHub" size={80} /></div>
					</div>
				</div>
				<div style={{marginTop: 36, opacity: c, fontFamily: fonts.heading, fontWeight: 700, fontSize: 40, color: colors.white,
					textShadow: '0 3px 0 #000'}}>github.com/jmokikearns-png/Dancing-Stage-Fusion-PC</div>
				<div style={{position: 'absolute', bottom: 46, opacity: c * 0.8, fontFamily: fonts.body, fontWeight: 500, fontSize: 24,
					color: colors.mute, textAlign: 'center'}}>
					Free fan project. Not affiliated with Konami. You need your own copy of the game.</div>
			</AbsoluteFill>
		</AbsoluteFill>
	);
};

export const Release11Trailer: React.FC = () => {
	const total = f(END_AT + END_LEN);
	const fade = (fr: number) => interpolate(fr, [total - 10, total], [1, 0], clamp);	/* the song fades itself */
	return (
		<AbsoluteFill style={{background: '#000'}}>
			<Audio src={staticFile('stars.wav')} volume={fade} />
			{cuts.map((c, i) => {
				const from = f(c.from), len = f(c.to) - f(c.from);
				return (
					<Sequence key={i} from={from} durationInFrames={len} name={c.kind + (c.src ? ' ' + c.src : c.slam ? ' ' + c.slam : '')}>
						<SceneView s={c} absStart={from} len={len} />
						{c.slam && <Slam text={c.slam} sub={c.slamSub} size={c.slamSize} dur={len} />}
						{c.tag && <Tag text={c.tag} sub={c.tagSub} dur={len} delay={4} align={c.tagRight ? 'right' : 'left'} y={c.tagY} />}
						{c.flash && <Flash len={7} strength={c.flash === 'big' ? 0.9 : 0.45} />}
					</Sequence>
				);
			})}
			<Count />
			<Sequence from={f(B(40))} durationInFrames={f(B(236)) - f(B(40))} name="footage note">
				<FootageNote len={f(B(236)) - f(B(40))} />
			</Sequence>
			<Sequence from={f(END_AT)} durationInFrames={f(END_LEN)} name="end card">
				<EndCard11 len={f(END_LEN)} />
			</Sequence>
		</AbsoluteFill>
	);
};
