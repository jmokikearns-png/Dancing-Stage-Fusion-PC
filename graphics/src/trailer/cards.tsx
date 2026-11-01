import React from 'react';
import {AbsoluteFill, Img, interpolate, spring, staticFile, useCurrentFrame} from 'remotion';
import {colors, fonts} from '../tokens';
import {BEAT, FPS} from './beat';
import {Stripes, Title} from './parts';

const clamp = {extrapolateLeft: 'clamp', extrapolateRight: 'clamp'} as const;

/* Step arrows drawn in the style of the game's (not taken from it): a
 * notched chevron head, chevron marks on the stem, a white rim and a dark
 * edge.  Pointing up; rotated for the other panels. */
const ARROW = 'M50 5 L95 50 L95 68 L64 40 L64 95 L36 95 L36 40 L5 68 L5 50 Z';
const ROT = [-90, 180, 0, 90];	/* left, down, up, right */
export const GameArrow: React.FC<{dir: number; size: number; kind: 'note' | 'receptor'; lit?: number; id: string}> = ({dir, size, kind, lit = 0, id}) => {
	const note = kind === 'note';
	const top = note ? '#8fd0ff' : '#d9dde3', mid = note ? '#5a63ff' : '#8a9099', bot = note ? '#b03cff' : '#3a3f47';
	return (
		<svg width={size} height={size} viewBox="-6 -6 112 112" style={{transform: `rotate(${ROT[dir]}deg)`, overflow: 'visible',
			filter: lit > 0.01 ? `drop-shadow(0 0 ${18 * lit}px rgba(255,240,140,${lit}))` : undefined}}>
			<defs>
				<linearGradient id={id} x1="0" y1="0" x2="0" y2="1">
					<stop offset="0" stopColor={top} /><stop offset="0.5" stopColor={mid} /><stop offset="1" stopColor={bot} />
				</linearGradient>
			</defs>
			<path d={ARROW} fill="none" stroke="#14101c" strokeWidth={15} strokeLinejoin="round" />
			<path d={ARROW} fill={`url(#${id})`} stroke={note ? '#ffffff' : '#c8ccd2'} strokeWidth={6} strokeLinejoin="round" />
			{/* the chevron marks */}
			<path d="M38 66 L50 56 L62 66 M38 82 L50 72 L62 82" fill="none" stroke={note ? '#ffffff' : '#1f2329'} strokeWidth={5}
				strokeLinecap="round" strokeLinejoin="round" opacity={note ? 0.9 : 0.8} />
			{lit > 0.01 && <path d={ARROW} fill="#fff6b0" opacity={0.85 * lit} />}
		</svg>
	);
};

/* Intro: the receptors fade in, then step arrows scroll up and hit them on
 * the beat, from the music's first beat. */
const HITS: Array<[number, number[]]> = [[1, [0]], [2, [1]], [3, [2]], [4, [3]], [5, [0, 3]], [6, [1]], [7, [2]]];
export const Intro: React.FC<{beats?: number}> = () => {
	const frame = useCurrentFrame();
	const t = frame / FPS;
	const SIZE = 170, GAP = 30, REC_Y = 250, SPEED = 1150;	/* px a second */
	const fade = interpolate(t, [0, 0.45], [0, 1], clamp);
	const x0 = (1920 - (4 * SIZE + 3 * GAP)) / 2;
	const beatTime = (k: number) => 0.01 + k * BEAT;
	return (
		<AbsoluteFill style={{background: '#050507', opacity: fade}}>
			{[0, 1, 2, 3].map((dir) => {
				const last = HITS.filter(([k, ds]) => ds.includes(dir) && beatTime(k) <= t).map(([k]) => beatTime(k)).pop();
				const lit = last === undefined ? 0 : Math.max(0, 1 - (t - last) * 4);
				return (
					<div key={'r' + dir} style={{position: 'absolute', left: x0 + dir * (SIZE + GAP), top: REC_Y, transform: `scale(${1 + 0.12 * lit})`}}>
						<GameArrow dir={dir} size={SIZE} kind="receptor" lit={lit} id={'rec' + dir} />
					</div>
				);
			})}
			{HITS.flatMap(([k, ds]) => ds.map((dir) => {
				const dt = beatTime(k) - t;	/* seconds until the hit */
				if (dt < -0.25 || dt > 1.2) return null;
				const y = REC_Y + Math.max(0, dt) * SPEED;
				const after = Math.max(0, -dt) / 0.25;	/* the hit: grows and fades */
				return (
					<div key={`n${k}-${dir}`} style={{position: 'absolute', left: x0 + dir * (SIZE + GAP), top: y,
						transform: `scale(${1 + 0.5 * after})`, opacity: 1 - after}}>
						<GameArrow dir={dir} size={SIZE} kind="note" id={`note${k}-${dir}`} />
					</div>
				);
			}))}
		</AbsoluteFill>
	);
};

/* Setup.exe's own messages (tools/setup/setup.c), typed out. */
const SETUP_LINES = [
	'Dancing Stage Fusion PC - Setup',
	'Builds the game from YOUR copy of Dancing Stage Fusion (Europe).',
	'',
	'Your disc: disc\\Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).bin',
	'  Checking your disc image... 100%',
	'  It is the PAL Dancing Stage Fusion (SLES-04163).',
	'  The program now has the game\'s icon.',
	'Recompiling the game program (about a minute)...',
	'Compiling the game (43 files, 4 at a time; a few minutes)...',
	'Linking dsfcore.dll...',
	'',
	'Done. Start the game with DancingStageFusionPC.exe.',
];
/* The new-songs builder's own output lines (tools/extrasong/build_extrasong.py, a real run). */
export const SONGS_LINES = [
	'Build New Songs',
	'',
	'Tough Enough                       new background BgToughEnough',
	'Go West                            new background BgGoWest',
	'Kids in America                    new background BgKidsInAmerica',
	'Heaven and Earth                   new background BgHeavenEarth',
	'stock song 17: new background BgMickey in 5 language entries',
	'Kids in America                     64 measures, play to 64 (lead 0), 7 charts',
	'12 songs, media 36159 sectors from LBA 105486, 48 pictures',
	'',
	'Done: the new songs are in the game.',
];

export const Console: React.FC<{len: number; title: string; lines: string[]; hue?: 'blue' | 'green' | 'red' | 'orange'; size?: number}> =
	({len, title, lines, hue = 'blue', size = 29}) => {
	const frame = useCurrentFrame();
	const enter = spring({frame, fps: FPS, config: {damping: 16, stiffness: 180}});
	const chars = interpolate(frame, [4, len * 0.8], [0, lines.join('\n').length], clamp);
	let left = chars;
	const shown = lines.map((l) => {
		const n = Math.max(0, Math.min(l.length, left));
		left -= l.length + 1;
		return left >= -l.length - 1 ? l.slice(0, Math.floor(n)) : null;
	});
	const last = lines.length - 1;
	return (
		<AbsoluteFill>
			<Stripes hue={hue} />
			<AbsoluteFill style={{justifyContent: 'center', alignItems: 'center'}}>
				<div style={{width: 1560, transform: `translateY(${(1 - enter) * 200}px) rotate(-2deg) scale(${interpolate(frame, [0, len], [1, 1.04])})`, opacity: enter,
					borderRadius: 14, overflow: 'hidden', boxShadow: '0 30px 80px rgba(0,0,0,0.7)', border: '3px solid #2b2f3a'}}>
					<div style={{background: '#e9ecf2', height: 56, display: 'flex', alignItems: 'center', padding: '0 22px',
						fontFamily: fonts.body, fontWeight: 500, fontSize: 24, color: '#222'}}>{title}</div>
					<div style={{background: '#0c0c0c', padding: '26px 30px', height: 640, fontFamily: "'Courier New', monospace", fontSize: size,
						lineHeight: 1.45, color: '#d6d6d6', whiteSpace: 'pre'}}>
						{shown.map((l, i) => l === null ? null : <div key={i} style={{color: i === 0 || i === last ? '#fff' : undefined,
							fontWeight: i === 0 || i === last ? 700 : 400}}>{l || ' '}</div>)}
						<span style={{opacity: Math.floor(frame / 12) % 2 ? 1 : 0}}>_</span>
					</div>
				</div>
			</AbsoluteFill>
		</AbsoluteFill>
	);
};

export const SetupConsole: React.FC<{len: number}> = ({len}) => <Console len={len} title="Setup.exe" lines={SETUP_LINES} />;

/* The end card. */
export const EndCard: React.FC<{len: number}> = ({len}) => {
	const frame = useCurrentFrame();
	const a = spring({frame, fps: FPS, config: {damping: 12, stiffness: 200, mass: 0.7}});
	const b = spring({frame: frame - 14, fps: FPS, config: {damping: 14, stiffness: 160}});
	const c = spring({frame: frame - 30, fps: FPS, config: {damping: 18, stiffness: 120}});
	const out = Math.min(interpolate(frame, [len - 25, len], [1, 0], clamp), interpolate(frame, [0, 10], [0, 1], clamp));
	return (
		<AbsoluteFill style={{opacity: out}}>
			<Stripes hue="green" speed={0.5} dim={0.15} />
			<AbsoluteFill style={{justifyContent: 'center', alignItems: 'center', flexDirection: 'column'}}>
				<div style={{transform: `scale(${1.12 - 0.12 * a})`, opacity: Math.min(1, a * 2.5), textAlign: 'center'}}>
					<div style={{filter: 'drop-shadow(0 16px 24px rgba(0,0,0,0.6))'}}><LogoArt k={2.3} frame={frame + 30} /></div>
				</div>
				<div style={{marginTop: 30, transform: `translateY(${(1 - b) * 120}px) skewX(-10deg)`, opacity: b}}>
					<div style={{background: `linear-gradient(180deg, ${colors.orangeLight} 0%, ${colors.orange} 40%, ${colors.red} 100%)`,
						padding: '16px 56px 22px', borderRadius: 10, border: '5px solid #1a0503', boxShadow: '0 12px 0 rgba(0,0,0,0.45)'}}>
						<div style={{transform: 'skewX(10deg)'}}><Title text="Now available on GitHub" size={84} /></div>
					</div>
				</div>
				<div style={{marginTop: 40, opacity: c, fontFamily: fonts.heading, fontWeight: 700, fontSize: 40, color: colors.white,
					textShadow: '0 3px 0 #000'}}>github.com/jmokikearns-png/Dancing-Stage-Fusion-PC</div>
				<div style={{position: 'absolute', bottom: 46, opacity: c * 0.8, fontFamily: fonts.body, fontWeight: 500, fontSize: 24,
					color: colors.mute, textAlign: 'center'}}>
					Free fan project. Not affiliated with Konami. You need your own copy of the game.</div>
			</AbsoluteFill>
		</AbsoluteFill>
	);
};

/* The title logo as the StepMania theme lays it out: "PC" centred at
 * (70, 14) from the centre of the 374x176 logo; k = scale. */
export const LogoArt: React.FC<{k: number; frame: number; pcDelay?: number}> = ({k, frame, pcDelay = 12}) => {
	const pc = spring({frame: frame - pcDelay, fps: FPS, config: {damping: 9, stiffness: 260, mass: 0.5}});
	return (
		<div style={{position: 'relative', width: 374 * k, height: 176 * k}}>
			<Img src={staticFile('logo_main.png')} style={{position: 'absolute', inset: 0, width: '100%', height: '100%'}} />
			<Img src={staticFile('logo_pc.png')} style={{position: 'absolute', left: (187 + 70 - 63) * k, top: (88 + 14 - 52) * k,
				width: 126 * k, height: 104 * k, transform: `scale(${2 - pc}) rotate(${(1 - pc) * 30}deg)`, opacity: Math.max(0, Math.min(1, pc * 2))}} />
		</div>
	);
};

export const Logo: React.FC<{len: number}> = ({len}) => {
	const frame = useCurrentFrame();
	const a = spring({frame, fps: FPS, config: {damping: 10, stiffness: 220, mass: 0.6}});
	const drift = interpolate(frame, [0, len], [1, 1.06]);
	return (
		<AbsoluteFill>
			<Stripes hue="orange" speed={2} />
			<AbsoluteFill style={{justifyContent: 'center', alignItems: 'center'}}>
				<div style={{transform: `scale(${(1.6 - 0.6 * a) * drift})`, opacity: Math.min(1, a * 1.5)}}>
					<LogoArt k={3} frame={frame} />
				</div>
			</AbsoluteFill>
		</AbsoluteFill>
	);
};
