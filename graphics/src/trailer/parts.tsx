import React from 'react';
import {AbsoluteFill, Img, interpolate, OffthreadVideo, spring, staticFile, useCurrentFrame, useVideoConfig} from 'remotion';
import {colors, fonts} from '../tokens';
import {BEAT, FPS, PHASE} from './beat';

export const DISPLAY = "'Serpentine'";

/* A title drawn in Serpentine beforehand (text/<slug>.png, 200 px type; see
 * the trailer notes): FreeType draws the font as it is. */
export const slug = (t: string) => t.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
export const Title: React.FC<{text: string; size: number; style?: React.CSSProperties}> = ({text, size, style}) => (
	<Img src={staticFile(`text/${slug(text)}.png`)} style={{zoom: size / 200, display: 'block', ...style}} />
);

const clamp = {extrapolateLeft: 'clamp', extrapolateRight: 'clamp'} as const;

/* How far into the current beat this frame is (0..1), for pulses. */
export const beatPhase = (absFrame: number) => {
	const t = absFrame / FPS - PHASE;
	return ((t / BEAT) % 1 + 1) % 1;
};

/* Moving stripes in the game's menu colours: the backdrop between shots. */
export const Stripes: React.FC<{hue?: 'green' | 'orange' | 'red' | 'blue'; speed?: number; dim?: number}> = ({hue = 'green', speed = 1, dim = 0}) => {
	const frame = useCurrentFrame();
	const pal = {
		green: ['#0c3d14', '#145c1e'],
		orange: ['#8a3a06', '#b8520c'],
		red: ['#5c1212', '#7d1a18'],
		blue: ['#0e1d5c', '#16297d'],
	}[hue];
	const x = (frame * 3 * speed) % 160;
	return (
		<AbsoluteFill style={{background: `repeating-linear-gradient(-30deg, ${pal[0]} 0px, ${pal[0]} 40px, ${pal[1]} 40px, ${pal[1]} 80px)`,
			backgroundPosition: `${x}px 0px`}}>
			<AbsoluteFill style={{background: 'radial-gradient(ellipse at 50% 50%, rgba(0,0,0,0) 30%, rgba(0,0,0,0.75) 100%)'}} />
			{dim > 0 && <AbsoluteFill style={{background: `rgba(0,0,0,${dim})`}} />}
		</AbsoluteFill>
	);
};

/* Big kinetic word: slams in (scale 1.6 -> 1), white with an orange-red
 * fill edge and a thick dark outline, like the game's own headers. */
export const Slam: React.FC<{text: string; size?: number; y?: number; delay?: number; color?: string; sub?: string; dur?: number}> =
	({text, size = 150, y = 0, delay = 0, color = colors.white, sub, dur}) => {
	const frame = useCurrentFrame() - delay;
	const {fps} = useVideoConfig();
	if (frame < 0) return null;
	const s = spring({frame, fps, config: {damping: 13, stiffness: 260, mass: 0.6}});
	const out = dur ? interpolate(frame, [dur - 6, dur], [1, 0], clamp) : 1;
	return (
		<AbsoluteFill style={{justifyContent: 'center', alignItems: 'center', transform: `translateY(${y}px)`, opacity: out}}>
			<div style={{transform: `scale(${1.7 - 0.7 * s}) rotate(${(1 - s) * -4}deg)`, opacity: Math.min(1, s * 2), textAlign: 'center'}}>
				<Title text={text} size={size} style={{margin: '0 auto', filter: 'drop-shadow(0 14px 18px rgba(0,0,0,0.6))'}} />
				{sub && <div style={{fontFamily: fonts.heading, fontWeight: 800, fontStyle: 'italic', fontSize: size * 0.3, color: colors.orangeLight,
					marginTop: size * 0.18, textShadow: '0 3px 0 #000, 0 0 14px rgba(0,0,0,0.7)', whiteSpace: 'pre'}}>{sub}</div>}
			</div>
		</AbsoluteFill>
	);
};

/* A strip of text that wipes in from the left, for feature labels over footage. */
export const Tag: React.FC<{text: string; sub?: string; x?: number; y?: number; delay?: number; dur: number; align?: 'left' | 'right'}> =
	({text, sub, x = 70, y = 70, delay = 0, dur, align = 'left'}) => {
	const frame = useCurrentFrame() - delay;
	const {fps} = useVideoConfig();
	if (frame < 0 || frame > dur) return null;
	const s = spring({frame, fps, config: {damping: 16, stiffness: 200}});
	const out = interpolate(frame, [dur - 8, dur], [0, 1], clamp);
	const dir = align === 'left' ? -1 : 1;
	return (
		<div style={{position: 'absolute', top: y, [align]: x, transform: `translateX(${dir * ((1 - s) * 700 + out * 900)}px) skewX(-12deg)`}}>
			<div style={{background: `linear-gradient(180deg, ${colors.orangeLight} 0%, ${colors.orange} 40%, ${colors.red} 100%)`,
				padding: '10px 34px 14px', borderRadius: 8, border: '4px solid #1a0503', boxShadow: '0 10px 0 rgba(0,0,0,0.45)'}}>
				<div style={{transform: 'skewX(12deg)'}}><Title text={text} size={62} /></div>
			</div>
			{sub && <div style={{fontFamily: fonts.heading, fontWeight: 800, fontStyle: 'italic', fontSize: 30, color: colors.white,
				marginTop: 14, padding: '6px 18px', background: 'rgba(10,6,4,0.82)', borderRadius: 6, display: 'inline-block',
				transform: 'skewX(12deg)', whiteSpace: 'pre'}}>{sub}</div>}
		</div>
	);
};

/* White flash on a hit. */
export const Flash: React.FC<{len?: number; strength?: number}> = ({len = 8, strength = 0.85}) => {
	const frame = useCurrentFrame();
	const o = interpolate(frame, [0, len], [strength, 0], clamp);
	return o > 0 ? <AbsoluteFill style={{background: '#fff', opacity: o}} /> : null;
};

/* A piece of footage, 4:3, framed on the stripes; zooms a little on the beat. */
export const Shot: React.FC<{src: string; from: number; len: number; still?: boolean; hue?: 'green' | 'orange' | 'red' | 'blue';
	full?: boolean; zoom?: number; absStart: number; rate?: number}> =
	({src, from, len, still, hue = 'green', full, zoom = 1, absStart, rate = 1}) => {
	const frame = useCurrentFrame();
	const enter = spring({frame, fps: FPS, config: {damping: 15, stiffness: 220}});
	const pulse = 1 + 0.018 * Math.pow(1 - beatPhase(absStart + frame), 3);
	const drift = interpolate(frame, [0, len], [1, 1.05]);
	const w = full ? 1920 : 1440, h = full ? 1080 : 1080;
	const scale = (full ? 1.0 : 0.9) * zoom * pulse * drift * (0.92 + 0.08 * enter);
	return (
		<AbsoluteFill>
			{!full && <Stripes hue={hue} />}
			<AbsoluteFill style={{justifyContent: 'center', alignItems: 'center'}}>
				<div style={{width: w, height: h, transform: `scale(${scale})`, overflow: 'hidden', borderRadius: full ? 0 : 18,
					border: full ? undefined : '6px solid #1a0503', boxShadow: full ? undefined : '0 24px 60px rgba(0,0,0,0.7)', background: '#000'}}>
					{still
						? <Img src={staticFile(src)} style={{width: '100%', height: '100%', objectFit: 'cover', imageRendering: 'auto'}} />
						: <OffthreadVideo src={staticFile(src)} startFrom={Math.round(from * FPS)} muted playbackRate={rate}
							style={{width: '100%', height: '100%', objectFit: 'cover'}} />}
				</div>
			</AbsoluteFill>
		</AbsoluteFill>
	);
};
