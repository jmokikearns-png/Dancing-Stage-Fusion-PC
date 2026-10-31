import React, {useEffect, useState} from 'react';
import {AbsoluteFill, continueRender, delayRender, Img, staticFile} from 'remotion';
import anton from '@fontsource/anton/files/anton-latin-400-normal.woff2';
import blackOps from '@fontsource/black-ops-one/files/black-ops-one-latin-400-normal.woff2';
import exo2 from '@fontsource/exo-2/files/exo-2-latin-900-italic.woff2';
import titan from '@fontsource/titan-one/files/titan-one-latin-400-normal.woff2';
import chewy from '@fontsource/chewy/files/chewy-latin-400-normal.woff2';

/* Gameplay backgrounds for songs whose own art is a video still of the
 * artists, drawn as the PS1 version drew its licensed songs' backgrounds
 * (Move Your Feet, Sunlight, I Believe in a Thing Called Love): made-for-the-
 * game poster art, the title in big lettering, no photographs.  Colours and
 * lettering follow each song's banner: the titles are the banners' own
 * lettering, cut out of the player's banners (port/tools/extrasong/
 * banner_lettering.py) into lettering/ in the public dir, so render with
 * --public-dir pointing at the player's art.
 *
 * 1280x960, four times the game's 320x240; build_extrasong.py scales them
 * down and puts them on 256 colours, so shapes are bold and flat. */

const W = 1280, H = 960;

const useFonts = () => {
	const [handle] = useState(() => delayRender('background fonts'));
	useEffect(() => {
		Promise.all([
			new FontFace('BgAnton', `url(${anton})`).load(),
			new FontFace('BgStencil', `url(${blackOps})`).load(),
			new FontFace('BgChrome', `url(${exo2})`, {style: 'italic', weight: '900'}).load(),
			new FontFace('BgTitan', `url(${titan})`).load(),
			new FontFace('BgChewy', `url(${chewy})`).load(),
		]).then((faces) => {
			faces.forEach((f) => document.fonts.add(f));
			continueRender(handle);
		});
	}, [handle]);
};

/* A seeded random sequence, so every render is the same picture. */
const rng = (seed: number) => () => {
	seed = (seed * 1664525 + 1013904223) >>> 0;
	return seed / 4294967296;
};

const star = (cx: number, cy: number, r: number, ri: number, rot = -Math.PI / 2, n = 5) =>
	Array.from({length: 2 * n}, (_, i) => {
		const a = rot + (Math.PI * i) / n, rr = i % 2 ? ri : r;
		return `${(cx + Math.cos(a) * rr).toFixed(1)},${(cy + Math.sin(a) * rr).toFixed(1)}`;
	}).join(' ');

/* Lettering: a block shadow, outline rings (outermost first) and a fill,
 * as SVG text so the strokes are exact. */
type Ring = [number, string];
const Lettering: React.FC<{x: number; y: number; text: string; font: string; size: number; fill: string;
	rings: Ring[]; shadow?: [number, number, string]; anchor?: 'start' | 'middle' | 'end'; spacing?: number;
	transform?: string; style?: string}> = ({x, y, text, font, size, fill, rings, shadow, anchor = 'middle', spacing = 0, transform, style}) => {
	const common = {x, y, fontFamily: font, fontSize: size, textAnchor: anchor, letterSpacing: spacing,
		strokeLinejoin: 'round' as const, fontStyle: style};
	const widest = rings.length ? rings[0][0] : 0;
	return (
		<g transform={transform}>
			{shadow && <text {...common} transform={`translate(${shadow[0]} ${shadow[1]})`} fill={shadow[2]}
				stroke={shadow[2]} strokeWidth={widest}>{text}</text>}
			{rings.map(([w, c], i) => <text key={i} {...common} fill={c} stroke={c} strokeWidth={w}>{text}</text>)}
			<text {...common} fill={fill}>{text}</text>
		</g>
	);
};

/* A banner's own lettering, cut out (colours and outlines as on the banner),
 * at a width, with the drop shadow the banners' backgrounds give it. */
const Banner: React.FC<{src: string; x: number; y: number; w: number; rot?: number; shadow?: string}> = ({src, x, y, w, rot = 0, shadow = 'rgba(0,0,0,0.55)'}) => (
	<Img src={staticFile(`lettering/${src}`)} style={{position: 'absolute', left: x, top: y, width: w, transform: `rotate(${rot}deg)`,
		filter: `drop-shadow(10px 12px 0 ${shadow})`}} />
);

const Halftone: React.FC<{opacity: number; size?: number; color?: string; mask?: string}> = ({opacity, size = 14, color = '#000', mask}) => (
	<AbsoluteFill style={{opacity, mixBlendMode: 'multiply',
		background: `radial-gradient(circle at center, ${color} 0%, ${color} 24%, transparent 36%)`, backgroundSize: `${size}px ${size}px`,
		maskImage: mask, WebkitMaskImage: mask}} />
);

/* ---- Kids in America (Kim Wilde, 1981): an American night drive, new-wave
 * style; chrome lettering split into red and cyan, the banner's red ellipse ---- */
export const BgKidsInAmerica: React.FC = () => {
	useFonts();
	const HZ = 610;	/* horizon */
	const r = rng(7);
	const sky = Array.from({length: 70}, () => [r() * W, r() * (HZ - 120), 1.5 + r() * 2.5]);
	const towers: Array<[number, number, number]> = [];
	for (let x = -20; x < W; ) {
		const w = 46 + r() * 70, h = 60 + r() * (x > 360 && x < 920 ? 70 : 230);
		towers.push([x, w, h]); x += w + 4;
	}
	return (
		<AbsoluteFill style={{background: 'linear-gradient(180deg, #05051f 0%, #1b0b4a 34%, #6a1470 58%, #ff4f6d 63.5%, #160024 63.6%, #0a0016 100%)'}}>
			<svg width={W} height={H} viewBox={`0 0 ${W} ${H}`} style={{position: 'absolute', inset: 0}}>
				<defs>
					<linearGradient id="kSun" x1="0" y1="0" x2="0" y2="1">
						<stop offset="0" stopColor="#fff36a" /><stop offset="0.55" stopColor="#ff9a3c" /><stop offset="1" stopColor="#ff2f7a" />
					</linearGradient>
					<clipPath id="kAbove"><rect x={0} y={0} width={W} height={HZ} /></clipPath>
					<linearGradient id="kChrome" x1="0" y1="0" x2="0" y2="1">
						<stop offset="0" stopColor="#ffffff" /><stop offset="0.42" stopColor="#c9e6ff" /><stop offset="0.5" stopColor="#1d3a7a" />
						<stop offset="0.62" stopColor="#7fb2ff" /><stop offset="1" stopColor="#ffffff" />
					</linearGradient>
					<radialGradient id="kEllipse" cx="0.4" cy="0.35" r="0.75">
						<stop offset="0" stopColor="#ffb07a" /><stop offset="0.45" stopColor="#ff4a1f" /><stop offset="1" stopColor="#9a0d12" />
					</radialGradient>
				</defs>
				{sky.map(([x, y, s], i) => <circle key={i} cx={x} cy={y} r={s} fill="#fff" opacity={0.55 + (i % 3) * 0.15} />)}
				{[[150, 90, 22], [1120, 140, 18], [980, 60, 12], [80, 300, 12]].map(([x, y, s], i) => (
					<polygon key={i} points={star(x, y, s, s * 0.42)} fill="#fff" />
				))}
				{/* a striped sun going down behind the city */}
				<g clipPath="url(#kAbove)">
					<circle cx={640} cy={HZ - 10} r={215} fill="url(#kSun)" />
					{Array.from({length: 7}, (_, i) => (
						<rect key={i} x={400} y={HZ - 120 + i * 18} width={480} height={4 + i * 2} fill="#6a1470" />
					))}
					{towers.map(([x, w, h], i) => (
						<g key={i}>
							<rect x={x} y={HZ - h} width={w} height={h} fill={i % 2 ? '#13042c' : '#1d0838'} />
							{Array.from({length: Math.floor(h / 26)}, (_, j) => Array.from({length: Math.floor(w / 18)}, (_, k) =>
								(i * 7 + j * 3 + k * 5) % 4 === 0 ? <rect key={`${j}-${k}`} x={x + 6 + k * 18} y={HZ - h + 10 + j * 26} width={8} height={12}
									fill={(i + j) % 3 ? '#ffd86a' : '#7ff6ff'} opacity={0.85} /> : null))}
						</g>
					))}
				</g>
				{/* the neon road grid */}
				{Array.from({length: 23}, (_, i) => {
					const x = (i - 11) * 150;
					return <line key={i} x1={640 + x * 0.06} y1={HZ} x2={640 + x * 2.6} y2={H} stroke="#ff3df0" strokeWidth={3} opacity={0.8} />;
				})}
				{Array.from({length: 9}, (_, i) => {
					const y = HZ + Math.pow(i / 8, 2.1) * (H - HZ);
					return <line key={i} x1={0} y1={y} x2={W} y2={y} stroke="#ff3df0" strokeWidth={2 + i * 0.6} opacity={0.75} />;
				})}
				<rect x={0} y={HZ - 2} width={W} height={5} fill="#ff9ad8" />
				{/* the banner's red ellipse behind the title */}
				<ellipse cx={640} cy={190} rx={560} ry={140} fill="#0b0320" transform="translate(12 14)" />
				<ellipse cx={640} cy={190} rx={560} ry={140} fill="url(#kEllipse)" stroke="#0b0320" strokeWidth={10} />
				<ellipse cx={600} cy={140} rx={420} ry={52} fill="#fff" opacity={0.18} />
			</svg>
			<Banner src="kia.png" x={150} y={58} w={980} shadow="rgba(11,3,32,0.85)" />
			<Halftone opacity={0.22} size={10} mask="linear-gradient(180deg, transparent 55%, #000 100%)" />
			<AbsoluteFill style={{background: 'radial-gradient(ellipse 85% 85% at 50% 45%, transparent 62%, rgba(4,0,16,0.6) 100%)'}} />
		</AbsoluteFill>
	);
};

/* ---- Go West (Pet Shop Boys, 1993): a constructivist poster; red rays,
 * the star, the onion domes, everything pointing west ---- */
export const BgGoWest: React.FC = () => {
	useFonts();
	const CREAM = '#f3e6c6', RED = '#c8102e', INK = '#1a0b0b', GOLD = '#ffc21a';
	const SX = 1010, SY = 300;	/* the star */
	const dome = (x: number, base: number, w: number, h: number, a: string, b: string, k: number) => {
		const top = base - h;
		const d = `M ${x - w / 2} ${base} C ${x - w * 0.72} ${base - h * 0.42}, ${x - w * 0.08} ${top + h * 0.3}, ${x} ${top} ` +
			`C ${x + w * 0.08} ${top + h * 0.3}, ${x + w * 0.72} ${base - h * 0.42}, ${x + w / 2} ${base} Z`;
		return (
			<g key={k}>
				<clipPath id={`gwDome${k}`}><path d={d} /></clipPath>
				<path d={d} fill={a} />
				<g clipPath={`url(#gwDome${k})`}>
					{Array.from({length: 9}, (_, i) => (
						<path key={i} d={`M ${x - w + i * w * 0.32} ${base} Q ${x - w * 0.2 + i * w * 0.2} ${base - h * 0.55} ${x + i * w * 0.05 - w * 0.1} ${top}`}
							stroke={b} strokeWidth={w * 0.11} fill="none" />
					))}
				</g>
				<path d={d} fill="none" stroke={INK} strokeWidth={7} />
				<line x1={x} y1={top} x2={x} y2={top - h * 0.22} stroke={GOLD} strokeWidth={8} />
				<polygon points={star(x, top - h * 0.26, 16, 7)} fill={GOLD} stroke={INK} strokeWidth={3} />
			</g>
		);
	};
	return (
		<AbsoluteFill style={{background: CREAM}}>
			<svg width={W} height={H} viewBox={`0 0 ${W} ${H}`} style={{position: 'absolute', inset: 0}}>
				{/* rays out of the star */}
				{Array.from({length: 18}, (_, i) => {
					const a0 = (i / 18) * Math.PI * 2, a1 = a0 + Math.PI / 18;
					const R = 1800;
					return <polygon key={i} points={`${SX},${SY} ${SX + Math.cos(a0) * R},${SY + Math.sin(a0) * R} ${SX + Math.cos(a1) * R},${SY + Math.sin(a1) * R}`}
						fill={RED} opacity={0.9} />;
				})}
				<circle cx={SX} cy={SY} r={250} fill={CREAM} />
				<circle cx={SX} cy={SY} r={250} fill="none" stroke={INK} strokeWidth={12} />
				<circle cx={SX} cy={SY} r={224} fill="none" stroke={RED} strokeWidth={10} />
				<polygon points={star(SX + 14, SY + 16, 205, 82)} fill={INK} />
				<polygon points={star(SX, SY, 205, 82)} fill={RED} stroke={INK} strokeWidth={12} strokeLinejoin="round" />
				{/* the cathedral's domes along the bottom */}
				<rect x={560} y={760} width={720} height={200} fill={INK} />
				{[[640, 70, 120, 150], [770, 90, 150, 200], [935, 80, 200, 290], [1100, 90, 150, 210], [1230, 70, 110, 150]].map(([x, tw, w, h], i) => (
					<g key={i}>
						<rect x={x - tw / 2} y={775 - (i === 2 ? 60 : 20)} width={tw} height={200} fill={i % 2 ? '#7a1010' : '#a01818'} stroke={INK} strokeWidth={6} />
						{dome(x, 775 - (i === 2 ? 60 : 20), w, h, ['#1f7a3a', '#ffc21a', RED, '#1d4fa8', '#1f7a3a'][i],
							['#f3e6c6', RED, '#f3e6c6', '#ffc21a', RED][i], i)}
					</g>
				))}
				{/* the black band and its arrow, pointing west */}
				<polygon points={`-40,620 ${W + 40},470 ${W + 40},610 -40,760`} fill={INK} />
				<polygon points="60,690 250,560 250,620 560,580 560,700 250,740 250,800" fill={GOLD} stroke={INK} strokeWidth={10} strokeLinejoin="round"
					transform="rotate(-7 300 680)" />
			</svg>
			{/* the banner's wordmark, slanted upward over the rays */}
			<div style={{position: 'absolute', left: 16, top: 150, width: 920, height: 190, background: CREAM, border: `12px solid ${INK}`,
				transform: 'rotate(-7deg)', boxShadow: `16px 18px 0 ${RED}`}} />
			<Banner src="gowest_title.png" x={46} y={196} w={860} rot={-7} shadow="rgba(26,11,11,0.6)" />
			<Banner src="gowest_artist.png" x={590} y={555} w={660} rot={-7.6} shadow="rgba(0,0,0,0.6)" />
			<Halftone opacity={0.18} size={12} color="#5a0a0a" />
			<AbsoluteFill style={{background: 'radial-gradient(ellipse 90% 90% at 50% 45%, transparent 65%, rgba(40,8,8,0.45) 100%)'}} />
		</AbsoluteFill>
	);
};

/* ---- Heaven and Earth (Pop!, 2004): the video's polka-dot set without the
 * dancers, clouds and light above; the banner's navy title and yellow POP! ---- */
export const BgHeavenEarth: React.FC = () => {
	useFonts();
	const DOTS = ['#ff4f9a', '#ffcf2a', '#2fc6ff', '#7ad63a', '#ff8a2a', '#a66cff', '#ff3b3b', '#25d3a8'];
	const WALL = 600;	/* wall meets floor */
	const r = rng(11);
	const wall: Array<[number, number, string]> = [];
	for (let row = 0; row < 7; row++)
		for (let col = 0; col < 14; col++)
			wall.push([col * 96 + (row % 2) * 48 + 24, row * 92 + 40, DOTS[Math.floor(r() * DOTS.length)]]);
	const floor: Array<[number, number, number, string]> = [];
	for (let row = 0; row < 6; row++) {
		const t = row / 5, y = WALL + 28 + Math.pow(t, 1.6) * (H - WALL - 10), s = 0.45 + t * 1.1;
		for (let col = -8; col <= 8; col++)
			floor.push([640 + (col + (row % 2) * 0.5) * 110 * s, y, s, DOTS[Math.floor(r() * DOTS.length)]]);
	}
	const cloud = (x: number, y: number, s: number, k: number) => (
		<g key={k} transform={`translate(${x} ${y}) scale(${s})`}>
			{[[0, 0, 90], [90, -40, 110], [200, -10, 95], [290, 20, 70], [140, 40, 90], [40, 50, 70]].map(([cx, cy, rr], i) => (
				<circle key={i} cx={cx} cy={cy} r={rr} fill="#fff" stroke="#bcd9ff" strokeWidth={6} />
			))}
			{[[0, 0, 84], [90, -40, 104], [200, -10, 89], [290, 20, 64], [140, 40, 84], [40, 50, 64]].map(([cx, cy, rr], i) => (
				<circle key={`f${i}`} cx={cx} cy={cy} r={rr} fill="#fff" />
			))}
		</g>
	);
	return (
		<AbsoluteFill style={{background: '#ffffff'}}>
			<svg width={W} height={H} viewBox={`0 0 ${W} ${H}`} style={{position: 'absolute', inset: 0}}>
				<defs>
					<linearGradient id="heWall" x1="0" y1="0" x2="0" y2="1">
						<stop offset="0" stopColor="#dff1ff" /><stop offset="0.5" stopColor="#ffffff" /><stop offset="1" stopColor="#f2f2f2" />
					</linearGradient>
					<radialGradient id="heGlow" cx="0.5" cy="0" r="0.8">
						<stop offset="0" stopColor="#fff6c0" stopOpacity="1" /><stop offset="1" stopColor="#fff6c0" stopOpacity="0" />
					</radialGradient>
				</defs>
				<rect x={0} y={0} width={W} height={WALL} fill="url(#heWall)" />
				{wall.map(([x, y, c], i) => <circle key={i} cx={x} cy={y} r={30} fill={c} />)}
				{/* the floor, its dots shrinking into the distance */}
				<rect x={0} y={WALL} width={W} height={H - WALL} fill="#fafafa" />
				<rect x={0} y={WALL - 4} width={W} height={10} fill="#d6d6e6" />
				{floor.map(([x, y, s, c], i) => <ellipse key={i} cx={x} cy={y} rx={34 * s} ry={11 * s} fill={c} />)}
				{/* heaven: light and clouds coming down over the set */}
				<rect x={0} y={0} width={W} height={520} fill="url(#heGlow)" />
				{Array.from({length: 12}, (_, i) => {
					const a = Math.PI * (0.12 + (i / 11) * 0.76);
					return <polygon key={i} points={`640,-60 ${640 + Math.cos(a - 0.04) * 1100},${-60 + Math.sin(a - 0.04) * 1100} ${640 + Math.cos(a + 0.04) * 1100},${-60 + Math.sin(a + 0.04) * 1100}`}
						fill="#fff3a0" opacity={0.35} />;
				})}
				{cloud(-60, 70, 0.95, 0)}
				{cloud(980, 50, 0.9, 1)}
				{cloud(420, -40, 0.7, 2)}
				{/* POP! bursting out at the side */}
				<polygon points={star(1030, 760, 210, 132, -0.3, 12)} fill="#ff4f9a" stroke="#0d1650" strokeWidth={10} strokeLinejoin="round" />
			</svg>
			{/* the banner's HEAVEN AND EARTH and POP! on a white card */}
			<div style={{position: 'absolute', left: 40, top: 330, width: 1200, height: 190, background: '#ffffff', borderRadius: 30,
				border: '10px solid #0d1650', boxShadow: '14px 16px 0 rgba(13,22,80,0.55)'}} />
			<Banner src="he_title.png" x={70} y={366} w={1140} shadow="rgba(13,22,80,0.35)" />
			<Banner src="he_pop.png" x={810} y={672} w={440} rot={-8} shadow="rgba(13,22,80,0.6)" />
			<AbsoluteFill style={{background: 'radial-gradient(ellipse 90% 90% at 50% 45%, transparent 70%, rgba(13,22,80,0.25) 100%)'}} />
		</AbsoluteFill>
	);
};

/* ---- Mickey (Toni Basil, 1981): a cheerleading routine without the
 * cheerleader; pom-poms, a megaphone, stars and the banner's lettering ---- */
export const BgMickey: React.FC = () => {
	useFonts();
	const INK = '#2a0626';
	const pompom = (x: number, y: number, R: number, a: string, b: string, k: number) => {
		const r = rng(30 + k);
		return (
			<g key={k}>
				<rect x={x - 16} y={y + R * 0.2} width={32} height={R * 1.05} rx={14} fill={INK} transform={`rotate(${k ? 25 : -20} ${x} ${y})`} />
				<rect x={x - 10} y={y + R * 0.2} width={20} height={R} rx={10} fill="#1d4fa8" transform={`rotate(${k ? 25 : -20} ${x} ${y})`} />
				<circle cx={x + 10} cy={y + 12} r={R} fill="rgba(42,6,38,0.45)" />
				<circle cx={x} cy={y} r={R * 0.92} fill={a} />
				{Array.from({length: 260}, (_, i) => {
					const ang = r() * Math.PI * 2, len = R * (0.7 + r() * 0.3), c = r() * 0.35;
					return <path key={i} d={`M ${x} ${y} Q ${x + Math.cos(ang + c) * len * 0.6} ${y + Math.sin(ang + c) * len * 0.6} ${x + Math.cos(ang) * len} ${y + Math.sin(ang) * len}`}
						stroke={i % 3 ? a : b} strokeWidth={16} strokeLinecap="round" fill="none" />;
				})}
				{Array.from({length: 60}, (_, i) => {
					const ang = r() * Math.PI * 2, len = R * (0.75 + r() * 0.25);
					return <line key={`s${i}`} x1={x + Math.cos(ang) * len * 0.5} y1={y + Math.sin(ang) * len * 0.5} x2={x + Math.cos(ang) * len} y2={y + Math.sin(ang) * len}
						stroke={INK} strokeWidth={3} opacity={0.35} strokeLinecap="round" />;
				})}
			</g>
		);
	};
	return (
		<AbsoluteFill style={{background: 'radial-gradient(ellipse 85% 80% at 45% 40%, #ff9ac6 0%, #ff3d6e 45%, #c4103c 80%, #6a0628 100%)'}}>
			<svg width={W} height={H} viewBox={`0 0 ${W} ${H}`} style={{position: 'absolute', inset: 0}}>
				{/* the banner's rainbow swooshes */}
				{[['#ffe14a', 0], ['#ff8a2a', 34], ['#7ad63a', 68], ['#2fc6ff', 102], ['#a66cff', 136]].map(([c, o], i) => (
					<path key={i} d={`M -80 ${760 - (o as number)} Q 520 ${300 - (o as number)} ${W + 80} ${520 - (o as number)}`}
						stroke={c as string} strokeWidth={30} fill="none" opacity={0.85} />
				))}
				{[[180, 150, 110], [1120, 120, 80], [960, 360, 60], [90, 520, 70], [600, 90, 50], [740, 600, 90], [330, 820, 60]].map(([x, y, s], i) => (
					<polygon key={i} points={star(x, y, s, s * 0.45, -Math.PI / 2 + i * 0.3)} fill="#ffd0e4" opacity={0.75} />
				))}
				{/* the megaphone, shouting at the title */}
				<g transform="rotate(-18 250 420)">
					<polygon points="90,400 150,370 420,250 420,590 150,470 90,440" fill="#ffffff" stroke={INK} strokeWidth={12} strokeLinejoin="round" />
					<ellipse cx={420} cy={420} rx={46} ry={170} fill="#ff3d6e" stroke={INK} strokeWidth={12} />
					<rect x={160} y={392} width={200} height={56} fill="#1d4fa8" />
					<text x={260} y={438} fontFamily="BgTitan" fontSize={52} fill="#ffe14a" textAnchor="middle">GO!</text>
					{[[520, 300, 600, 240], [540, 420, 640, 420], [520, 540, 600, 600]].map(([x1, y1, x2, y2], i) => (
						<line key={i} x1={x1} y1={y1} x2={x2} y2={y2} stroke="#fff" strokeWidth={16} strokeLinecap="round" />
					))}
				</g>
				{pompom(1060, 250, 170, '#ffe14a', '#ffffff', 0)}
				{pompom(200, 780, 150, '#ffffff', '#2fc6ff', 1)}
			</svg>
			{/* the banner's Mickey / Toni Basil, bottom right as on the stock background */}
			<Banner src="mickey_title.png" x={340} y={540} w={880} rot={-6} shadow="rgba(42,6,38,0.7)" />
			<Banner src="mickey_artist.png" x={520} y={770} w={620} rot={-6} shadow="rgba(42,6,38,0.7)" />
			<Halftone opacity={0.2} size={12} color="#6a0628" />
			<AbsoluteFill style={{background: 'radial-gradient(ellipse 88% 88% at 50% 45%, transparent 64%, rgba(60,0,24,0.55) 100%)'}} />
		</AbsoluteFill>
	);
};
