import React from 'react';
import {AbsoluteFill, Audio, interpolate, Sequence, spring, staticFile, useCurrentFrame} from 'remotion';
import '../fonts';
import './serpentine';
import {colors, fonts} from '../tokens';
import {LogoArt} from './cards';
import {CalmShot, FadeIn, Note} from './Hotfix';
import {Stripes, Title} from './parts';

/* The 1.1 trailer: 321STARS (DJ Simon, about 127.7 BPM, from the player's
 * disc, stream 3: "str_decode.py 3 stars.wav"). Calm, like the 1.0.1 one:
 * crossfades, no flashes. Footage: the 1.1 build (clips/v_*.mp4). */
export const RELEASE11_FPS = 50;
const SONG_FROM = 0.3;		/* the first beat */
const BAR = 4 * 60 / 127.7;
const f = (s: number) => Math.round(s * RELEASE11_FPS);
const bar = (n: number) => n * BAR;
const XF = 10;	/* crossfade, frames */
const clamp = {extrapolateLeft: 'clamp', extrapolateRight: 'clamp'} as const;
const END = bar(22);
export const release11Frames = () => f(END);

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
					<Title text="Version 1.1" size={78} />
				</div>
			</AbsoluteFill>
		</AbsoluteFill>
	);
};

const Ending: React.FC<{len: number}> = ({len}) => {
	const frame = useCurrentFrame();
	const a = spring({frame, fps: RELEASE11_FPS, config: {damping: 200}});
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
				<div style={{opacity: b, marginTop: 26}}><Title text="Version 1.1" size={64} /></div>
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

/* A shot's caption, like the 1.0.1 trailer's: top left, or bottom right
 * where the game's own pop-up is at the top of the picture. */
const Tag: React.FC<{text: string; sub?: string; len: number; at: 'top' | 'bottom'}> = ({text, sub, len, at}) => {
	const frame = useCurrentFrame();
	const a = interpolate(frame, [6, 22, len - 12, len], [0, 1, 1, 0], clamp);
	const x = interpolate(frame, [6, 26], [at === 'top' ? -30 : 30, 0], clamp);
	const place: React.CSSProperties = at === 'top' ? {left: 70, top: 56} : {right: 70, bottom: 92, textAlign: 'right'};
	return (
		<div style={{position: 'absolute', ...place, opacity: a, transform: `translateX(${x}px)`}}>
			<div style={{background: 'rgba(12,8,6,0.82)', padding: '12px 26px 14px', borderRadius: 10,
				[at === 'top' ? 'borderLeft' : 'borderRight']: `6px solid ${colors.orange}`, boxShadow: '0 10px 30px rgba(0,0,0,0.5)'}}>
				<Title text={text} size={50} style={at === 'top' ? undefined : {marginLeft: 'auto'}} />
				{sub && <div style={{fontFamily: fonts.heading, fontWeight: 600, fontStyle: 'italic', fontSize: 28, color: colors.white,
					marginTop: 10}}>{sub}</div>}
			</div>
		</div>
	);
};

type Hue = 'green' | 'orange' | 'red' | 'blue';
type Cut = {from: number; to: number; el: (len: number) => React.ReactNode};
const shot = (from: number, to: number, src: string, hue: Hue, tag?: [string, string?], at: 'top' | 'bottom' = 'top'): Cut =>
	({from, to, el: (len) => <>
		<CalmShot src={src} len={len} hue={hue} />
		{tag && <Tag text={tag[0]} sub={tag[1]} len={len} at={at} />}
	</>});

/* Each clip (clips/v_<name>.mp4) is cut from the recordings at the times in
 * edit11.json: at least its cut's length plus the crossfade. */
const CUTS: Cut[] = [
	{from: 0, to: bar(2), el: (len) => <Opening len={len} />},
	shot(bar(2), bar(4), 'clips/v_title.mp4', 'orange', ['ACHIEVEMENTS', '25 to earn, from your first clear to 1,000 kcal']),
	shot(bar(4), bar(6), 'clips/v_card.mp4', 'blue', ['YOUR RECORDS COUNT', "what's on your memory card unlocks at once"], 'bottom'),
	shot(bar(6), bar(8), 'clips/v_new.mp4', 'red', ['NEW!!', 'newly unlocked songs are marked, like on the Deluxe disc']),
	shot(bar(8), bar(10), 'clips/v_play1.mp4', 'green'),
	shot(bar(10), bar(13), 'clips/v_res1.mp4', 'orange', ['UNLOCKED AS YOU PLAY', 'each one pops up on the results screen'], 'bottom'),
	shot(bar(13), bar(14), 'clips/v_play2.mp4', 'red'),
	shot(bar(14), bar(16), 'clips/v_res2.mp4', 'blue'),
	shot(bar(16), bar(19), 'clips/v_list.mp4', 'green', ['TRACK YOUR PROGRESS', 'ACHIEVEMENTS on the title menu']),
	{from: bar(19), to: END, el: (len) => <Ending len={len} />},
];

export const Release11Trailer: React.FC = () => {
	const fade = (fr: number) => interpolate(fr, [f(END) - 75, f(END)], [1, 0], clamp);
	return (
		<AbsoluteFill style={{background: '#000'}}>
			<Audio src={staticFile('stars.wav')} startFrom={f(SONG_FROM)} volume={fade} />
			{CUTS.map((c, i) => {
				const from = Math.max(0, f(c.from) - (i ? XF : 0)), len = f(c.to) - from;
				return (
					<Sequence key={i} from={from} durationInFrames={len}>
						<FadeIn on={i > 0}>{c.el(len)}</FadeIn>
					</Sequence>
				);
			})}
			<Sequence from={f(bar(2))} durationInFrames={f(bar(19)) - f(bar(2))} name="note">
				<Note len={f(bar(19)) - f(bar(2))} />
			</Sequence>
		</AbsoluteFill>
	);
};

/* Clip lengths in seconds, for cutting them from the recordings. */
export const release11ClipSeconds = () => CUTS.slice(1, -1).map((c) => ({to: c.to, from: c.from, secs: c.to - c.from + XF / RELEASE11_FPS}));
