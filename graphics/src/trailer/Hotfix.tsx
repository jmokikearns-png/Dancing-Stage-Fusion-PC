import React from 'react';
import {AbsoluteFill, Audio, interpolate, OffthreadVideo, Sequence, spring, staticFile, useCurrentFrame} from 'remotion';
import '../fonts';
import './serpentine';
import {colors, fonts} from '../tokens';
import {Console, LogoArt} from './cards';
import {Stripes, Title} from './parts';

/* The 1.0.1 trailer: KEEP ON MOVIN' (N.M.R, 132 BPM, from the player's
 * disc, stream 13), from its first bar at 1.84 s. Calm: crossfades, no
 * flashes or slams. Footage: the 1.0.1 build (clips/h_*.mp4). */
export const HOTFIX_FPS = 50;
const SONG_FROM = 1.84;
const BAR = 4 * 60 / 132;
const f = (s: number) => Math.round(s * HOTFIX_FPS);
const bar = (n: number) => n * BAR;
const XF = 10;	/* crossfade, frames */
const clamp = {extrapolateLeft: 'clamp', extrapolateRight: 'clamp'} as const;
const END = bar(21);
export const hotfixFrames = () => f(END);

const UPDATER_LINES = [
	'Dancing Stage Fusion PC - Updater',
	'',
	'Installed: 1.0',
	'On GitHub: 1.0.1',
	'',
	'DancingStageFusionPC-1.0.1.zip',
	'  Downloading... 100%',
	'  Unpacking...',
	'  Installing...',
	'  4226 files updated.',
	'  Checking your disc image... 100%',
	"  The program now has the game's icon.",
	'',
	'Updated to 1.0.1.',
];

const CalmTag: React.FC<{text: string; sub?: string; len: number}> = ({text, sub, len}) => {
	const frame = useCurrentFrame();
	const a = interpolate(frame, [6, 22, len - 12, len], [0, 1, 1, 0], clamp);
	const x = interpolate(frame, [6, 26], [-30, 0], clamp);
	return (
		<div style={{position: 'absolute', left: 70, top: 56, opacity: a, transform: `translateX(${x}px)`}}>
			<div style={{background: 'rgba(12,8,6,0.82)', padding: '12px 26px 14px', borderRadius: 10, borderLeft: `6px solid ${colors.orange}`,
				boxShadow: '0 10px 30px rgba(0,0,0,0.5)'}}>
				<Title text={text} size={50} />
				{sub && <div style={{fontFamily: fonts.heading, fontWeight: 600, fontStyle: 'italic', fontSize: 28, color: colors.white,
					marginTop: 10}}>{sub}</div>}
			</div>
		</div>
	);
};

const CalmShot: React.FC<{src: string; len: number; hue?: 'green' | 'orange' | 'red' | 'blue'}> = ({src, len, hue = 'green'}) => {
	const frame = useCurrentFrame();
	const drift = interpolate(frame, [0, len], [1.0, 1.035]);
	return (
		<AbsoluteFill>
			<Stripes hue={hue} speed={0.4} dim={0.1} />
			<AbsoluteFill style={{justifyContent: 'center', alignItems: 'center'}}>
				<div style={{width: 1440, height: 1080, transform: `scale(${0.88 * drift})`, overflow: 'hidden', borderRadius: 16,
					border: '5px solid #1a0503', boxShadow: '0 24px 60px rgba(0,0,0,0.6)', background: '#000'}}>
					<OffthreadVideo src={staticFile(src)} muted style={{width: '100%', height: '100%'}} />
				</div>
			</AbsoluteFill>
		</AbsoluteFill>
	);
};

const Opening: React.FC<{len: number}> = ({len}) => {
	const frame = useCurrentFrame();
	const a = interpolate(frame, [0, 40], [0, 1], clamp);
	const s = interpolate(frame, [0, len], [1.04, 1.0]);
	const v = interpolate(frame, [45, 70], [0, 1], clamp);
	return (
		<AbsoluteFill>
			<Stripes hue="green" speed={0.4} dim={0.15} />
			<AbsoluteFill style={{justifyContent: 'center', alignItems: 'center', flexDirection: 'column'}}>
				<div style={{opacity: a, transform: `scale(${s})`, filter: 'drop-shadow(0 16px 24px rgba(0,0,0,0.6))'}}>
					<LogoArt k={2.6} frame={60} />
				</div>
				<div style={{opacity: v, marginTop: 34, transform: `translateY(${(1 - v) * 16}px)`}}>
					<Title text="Version 1.0.1" size={78} />
				</div>
			</AbsoluteFill>
		</AbsoluteFill>
	);
};

const Ending: React.FC<{len: number}> = ({len}) => {
	const frame = useCurrentFrame();
	const a = spring({frame, fps: HOTFIX_FPS, config: {damping: 200}});
	const b = interpolate(frame, [20, 40], [0, 1], clamp);
	const c = interpolate(frame, [35, 55], [0, 1], clamp);
	const out = interpolate(frame, [len - 30, len], [1, 0], clamp);
	return (
		<AbsoluteFill style={{opacity: out}}>
			<Stripes hue="green" speed={0.4} dim={0.15} />
			<AbsoluteFill style={{justifyContent: 'center', alignItems: 'center', flexDirection: 'column'}}>
				<div style={{opacity: a, transform: `scale(${1.03 - 0.03 * a})`, filter: 'drop-shadow(0 16px 24px rgba(0,0,0,0.6))'}}>
					<LogoArt k={2.2} frame={60} />
				</div>
				<div style={{opacity: b, marginTop: 26}}><Title text="Version 1.0.1" size={64} /></div>
				<div style={{opacity: b, marginTop: 22, transform: 'skewX(-10deg)'}}>
					<div style={{background: `linear-gradient(180deg, ${colors.orangeLight} 0%, ${colors.orange} 40%, ${colors.red} 100%)`,
						padding: '14px 46px 18px', borderRadius: 10, border: '5px solid #1a0503', boxShadow: '0 10px 0 rgba(0,0,0,0.4)'}}>
						<div style={{transform: 'skewX(10deg)'}}><Title text="Now available on GitHub" size={72} /></div>
					</div>
				</div>
				<div style={{marginTop: 34, opacity: c, fontFamily: fonts.heading, fontWeight: 700, fontSize: 38, color: colors.white,
					textShadow: '0 3px 0 #000'}}>github.com/jmokikearns-png/Dancing-Stage-Fusion-PC</div>
			</AbsoluteFill>
		</AbsoluteFill>
	);
};

const Note: React.FC<{len: number}> = ({len}) => {
	const frame = useCurrentFrame();
	const o = interpolate(frame, [0, 15, len - 15, len], [0, 1, 1, 0], clamp);
	return (
		<AbsoluteFill style={{justifyContent: 'flex-end', alignItems: 'center', paddingBottom: 16, opacity: o}}>
			<div style={{background: 'rgba(8,6,6,0.75)', borderRadius: 8, padding: '6px 16px', fontFamily: fonts.body, fontWeight: 500,
				fontSize: 21, color: 'rgba(255,255,255,0.88)'}}>
				Note: recording and editing make the footage less smooth than the game itself.
			</div>
		</AbsoluteFill>
	);
};

type Cut = {from: number; to: number; el: (len: number) => React.ReactNode};
const shot = (from: number, to: number, src: string, hue: 'green' | 'orange' | 'red' | 'blue', tag?: [string, string?]): Cut =>
	({from, to, el: (len) => <>
		<CalmShot src={src} len={len} hue={hue} />
		{tag && <CalmTag text={tag[0]} sub={tag[1]} len={len} />}
	</>});

const CUTS: Cut[] = [
	{from: 0, to: bar(2), el: (len) => <Opening len={len} />},
	shot(bar(2), bar(4), 'clips/h_survA.mp4', 'red', ['SMOOTHER PLAYBACK', 'every picture on time, one every 20 ms']),
	shot(bar(4), bar(6), 'clips/h_survB.mp4', 'blue', ['SMOOTHER PLAYBACK', "SMOOTH ARROWS' in-between frames stay on screen"]),
	shot(bar(6), bar(8), 'clips/h_toughA.mp4', 'red'),
	shot(bar(8), bar(11), 'clips/h_offer.mp4', 'orange', ['UPDATES FROM INSIDE THE GAME', 'new versions are offered on the title menu']),
	{from: bar(11), to: bar(14), el: (len) => <>
		<Console len={len} title="DancingStageFusionPC-Updater.exe" lines={UPDATER_LINES} hue="blue" size={27} />
		<CalmTag text="ON 1.0? RUN THE UPDATER ONCE" sub="your songs, saves and settings stay" len={len} />
	</>},
	shot(bar(14), bar(15), 'clips/h_toughB.mp4', 'green'),
	shot(bar(15), bar(16), 'clips/h_survC.mp4', 'orange'),
	shot(bar(16), bar(17), 'clips/h_toughC.mp4', 'red'),
	{from: bar(17), to: END, el: (len) => <Ending len={len} />},
];

export const HotfixTrailer: React.FC = () => {
	const fade = (fr: number) => interpolate(fr, [f(END) - 75, f(END)], [1, 0], clamp);
	return (
		<AbsoluteFill style={{background: '#000'}}>
			<Audio src={staticFile('keeponmovin.wav')} startFrom={f(SONG_FROM)} volume={fade} />
			{CUTS.map((c, i) => {
				const from = Math.max(0, f(c.from) - (i ? XF : 0)), len = f(c.to) - from;
				return (
					<Sequence key={i} from={from} durationInFrames={len}>
						<FadeIn on={i > 0}>{c.el(len)}</FadeIn>
					</Sequence>
				);
			})}
			<Sequence from={f(bar(2))} durationInFrames={f(bar(17)) - f(bar(2))} name="note">
				<Note len={f(bar(17)) - f(bar(2))} />
			</Sequence>
		</AbsoluteFill>
	);
};

const FadeIn: React.FC<{on: boolean; children: React.ReactNode}> = ({on, children}) => {
	const frame = useCurrentFrame();
	return <AbsoluteFill style={{opacity: on ? interpolate(frame, [0, XF], [0, 1], clamp) : 1}}>{children}</AbsoluteFill>;
};
