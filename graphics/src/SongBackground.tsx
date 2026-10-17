import React, {useEffect, useState} from 'react';
import {AbsoluteFill, continueRender, delayRender, Img, staticFile} from 'remotion';
import blackOps from '@fontsource/black-ops-one/files/black-ops-one-latin-400-normal.woff2';
import anton from '@fontsource/anton/files/anton-latin-400-normal.woff2';

/* An extra song's gameplay background in the style the PS1 version gave its
 * licensed songs (Move Your Feet, Come Into My World, Sunlight): made-for-
 * the-game poster art instead of a video still.  This one is a fight poster
 * for a band in a boxing ring: spotlight rays on deep red, the player's own
 * photo of the band as a four-tone screen print, padded ring ropes, a knockout
 * starburst behind a stencil title, the artist on a slanted ribbon.
 *
 * 1280x960, four times the game's 320x240; build_extrasong.py --background
 * scales it down and puts it on 256 colours.  The photo comes from the
 * public dir (render with --public-dir pointing at the player's files); the
 * crop puts the band right of the title, and the middle rope over the
 * still's own rope (and the brand names printed on it). */
export type SongBackgroundProps = {
	photo: string;		/* in the public dir */
	photoCrop: {x: number; y: number; w: number; h: number};	/* of the photo, fills the frame */
	titleLines: string[];
	artist: string;
};

const W = 1280, H = 960;
const RED = '#c8102e', DEEP = '#4a0010', INK = '#14040a', GOLD = '#ffd23a', ORANGE = '#ff7a1a';

const useFonts = () => {
	const [handle] = useState(() => delayRender('background fonts'));
	useEffect(() => {
		Promise.all([
			new FontFace('BgTitle', `url(${blackOps})`).load(),
			new FontFace('BgArtist', `url(${anton})`).load(),
		]).then((faces) => {
			faces.forEach((f) => document.fonts.add(f));
			continueRender(handle);
		});
	}, [handle]);
};

/* The band as a four-tone screen print (ink, deep red, red, cream), like
 * the PS1 version's own photo backgrounds; a light dot screen over it. */
const ScreenPrintPhoto: React.FC<Pick<SongBackgroundProps, 'photo' | 'photoCrop'>> = ({photo, photoCrop: c}) => {
	const fade = 'radial-gradient(ellipse 70% 85% at 66% 46%, #000 55%, transparent 100%)';
	/* and off before the photo's own left edge, wherever the crop puts it */
	const left = Math.max(0, (-c.x / c.w) * W);
	const side = `linear-gradient(90deg, transparent ${left + 10}px, #000 ${left + 220}px)`;
	return (
		<AbsoluteFill style={{maskImage: side, WebkitMaskImage: side}}>
		<AbsoluteFill style={{maskImage: fade, WebkitMaskImage: fade}}>
			<svg width={W} height={H} viewBox={`${c.x} ${c.y} ${c.w} ${c.h}`} preserveAspectRatio="none" style={{position: 'absolute', inset: 0}}>
				<defs>
					<filter id="print" colorInterpolationFilters="sRGB">
						<feColorMatrix type="saturate" values="0" />
						<feComponentTransfer>
							<feFuncR type="linear" slope="1.55" intercept="-0.22" />
							<feFuncG type="linear" slope="1.55" intercept="-0.22" />
							<feFuncB type="linear" slope="1.55" intercept="-0.22" />
						</feComponentTransfer>
						<feComponentTransfer>
							<feFuncR type="discrete" tableValues="0.08 0.30 0.80 1.00" />
							<feFuncG type="discrete" tableValues="0.02 0.00 0.07 0.86" />
							<feFuncB type="discrete" tableValues="0.04 0.07 0.19 0.76" />
						</feComponentTransfer>
					</filter>
				</defs>
				<image href={staticFile(photo)} x={0} y={0} filter="url(#print)" />
			</svg>
			<AbsoluteFill style={{mixBlendMode: 'multiply', opacity: 0.35,
				background: 'radial-gradient(circle at center, #000 0%, #000 22%, transparent 34%)', backgroundSize: '12px 12px'}} />
		</AbsoluteFill>
		</AbsoluteFill>
	);
};

/* A padded ring rope from (x0, y0) to (x1, y1): dark rim, colour, a lighter
 * band along the top, a shine line, and the stitched seams of the sleeve. */
const Rope: React.FC<{y0: number; y1: number; sag: number; t: number; fill: string; light: string; id: string}> =
	({y0, y1, sag, t, fill, light, id}) => {
	const d = `M -40 ${y0} Q ${W / 2} ${(y0 + y1) / 2 + sag} ${W + 40} ${y1}`;
	return (
		<g>
			<defs>
				<linearGradient id={id} x1="0" y1="0" x2="0" y2="1">
					<stop offset="0" stopColor={light} />
					<stop offset="0.45" stopColor={fill} />
					<stop offset="1" stopColor={INK} />
				</linearGradient>
			</defs>
			<path d={d} stroke="rgba(0,0,0,0.45)" strokeWidth={t + 18} fill="none" transform="translate(0 16)" />
			<path d={d} stroke={INK} strokeWidth={t + 10} fill="none" />
			<path d={d} stroke={fill} strokeWidth={t} fill="none" />
			<path d={d} stroke={light} strokeWidth={t * 0.28} fill="none" transform={`translate(0 ${-t * 0.24})`} opacity={0.85} />
			<path d={d} stroke="#fff" strokeWidth={4} fill="none" transform={`translate(0 ${-t * 0.3})`} opacity={0.9} />
			<path d={d} stroke={INK} strokeWidth={3} strokeDasharray="10 9" fill="none" transform={`translate(0 ${t * 0.3})`} opacity={0.55} />
		</g>
	);
};

const starburst = (cx: number, cy: number, r0: number, r1: number, n: number, rot: number) => {
	const pts: string[] = [];
	for (let i = 0; i < 2 * n; i++) {
		const a = rot + (Math.PI * i) / n;
		const r = i % 2 ? r0 : r1 * (i % 4 === 0 ? 1 : 0.86);
		pts.push(`${(cx + Math.cos(a) * r).toFixed(1)},${(cy + Math.sin(a) * r * 0.8).toFixed(1)}`);
	}
	return pts.join(' ');
};

const sparkle = (x: number, y: number, s: number) =>
	`M ${x} ${y - s} Q ${x + s * 0.12} ${y - s * 0.12} ${x + s} ${y} Q ${x + s * 0.12} ${y + s * 0.12} ${x} ${y + s} ` +
	`Q ${x - s * 0.12} ${y + s * 0.12} ${x - s} ${y} Q ${x - s * 0.12} ${y - s * 0.12} ${x} ${y - s} Z`;

export const SongBackground: React.FC<SongBackgroundProps> = ({photo, photoCrop, titleLines, artist}) => {
	useFonts();
	const BX = 400, BY = 300;	/* the starburst's centre: behind the title */
	return (
		<AbsoluteFill style={{background: `radial-gradient(ellipse 80% 75% at 50% 30%, #ff3b4e 0%, ${RED} 38%, ${DEEP} 78%, ${INK} 100%)`}}>
			<ScreenPrintPhoto photo={photo} photoCrop={photoCrop} />
			{/* spotlight rays from above */}
			<AbsoluteFill style={{mixBlendMode: 'screen', background: 'repeating-conic-gradient(from -6deg at 50% -8%, rgba(255,190,140,0.22) 0deg 5deg, rgba(255,214,150,0) 5deg 15deg)',
				maskImage: 'radial-gradient(ellipse 90% 95% at 50% 0%, #000 30%, transparent 85%)',
				WebkitMaskImage: 'radial-gradient(ellipse 90% 95% at 50% 0%, #000 30%, transparent 85%)'}} />
			<svg width={W} height={H} viewBox={`0 0 ${W} ${H}`} style={{position: 'absolute', inset: 0}}>
				<defs>
					<radialGradient id="burst" cx="0.5" cy="0.45" r="0.6">
						<stop offset="0" stopColor="#fff6c0" />
						<stop offset="0.35" stopColor={GOLD} />
						<stop offset="1" stopColor={ORANGE} />
					</radialGradient>
				</defs>
				{/* speed lines out of the punch */}
				{Array.from({length: 22}, (_, i) => {
					const a = (i / 22) * Math.PI * 2 + 0.12;
					const r0 = 300 + (i % 3) * 30, r1 = 640 + (i % 4) * 60;
					return <line key={i} x1={BX + Math.cos(a) * r0} y1={BY + Math.sin(a) * r0 * 0.8}
						x2={BX + Math.cos(a) * r1} y2={BY + Math.sin(a) * r1 * 0.8}
						stroke="#fff3c8" strokeWidth={i % 2 ? 5 : 9} strokeLinecap="round" opacity={0.5} />;
				})}
				<polygon points={starburst(BX + 10, BY + 14, 200, 320, 14, -0.2)} fill={INK} />
				<polygon points={starburst(BX, BY, 190, 300, 14, -0.2)} fill="url(#burst)" stroke={INK} strokeWidth={10} strokeLinejoin="round" />
				<polygon points={starburst(BX, BY, 150, 236, 14, -0.2)} fill="none" stroke="#fff" strokeWidth={5} opacity={0.65} strokeLinejoin="round" />
				{/* the ring: the near rope lies over the video still's own rope */}
				<Rope id="ropeBack" y0={452} y1={486} sag={22} t={40} fill="#f2f2f2" light="#ffffff" />
				<Rope id="ropeMid" y0={600} y1={690} sag={34} t={108} fill={RED} light="#ff8a8a" />
				<Rope id="ropeFront" y0={850} y1={930} sag={40} t={70} fill="#1d3fbf" light="#7fa2ff" />
				{[[1150, 120, 44], [1220, 250, 24], [760, 90, 26], [1060, 820, 34], [70, 130, 22], [690, 560, 22]].map(([x, y, s], i) => (
					<path key={i} d={sparkle(x, y, s)} fill="#fff" opacity={0.95} />
				))}
			</svg>
			{/* title: stencil letters, gold to white, inked outline, red block shadow */}
			<div style={{position: 'absolute', left: BX - 420, top: BY - 190, width: 840, textAlign: 'center',
				transform: 'rotate(-7deg) skewX(-9deg)', fontFamily: 'BgTitle', lineHeight: 0.92}}>
				{titleLines.map((l, i) => (
					<div key={i} style={{position: 'relative', fontSize: i === 0 ? 196 : 170, height: i === 0 ? 186 : 160}}>
						{[12, 10, 8, 6, 4, 2].map((o) => (
							<span key={o} style={{position: 'absolute', left: 0, right: 0, transform: `translate(${o}px, ${o}px)`,
								color: o > 6 ? INK : '#8a0016', WebkitTextStroke: `16px ${o > 6 ? INK : '#8a0016'}`}}>{l}</span>
						))}
						<span style={{position: 'absolute', left: 0, right: 0, color: INK, WebkitTextStroke: `18px ${INK}`}}>{l}</span>
						<span style={{position: 'absolute', left: 0, right: 0,
							background: 'linear-gradient(180deg, #ffffff 0%, #fff4b0 38%, #ffc21a 62%, #ff7a00 100%)',
							WebkitBackgroundClip: 'text', backgroundClip: 'text', color: 'transparent'}}>{l}</span>
					</div>
				))}
			</div>
			{/* the artist on a slanted ribbon */}
			<div style={{position: 'absolute', left: 60, top: 520, width: 660, height: 96, transform: 'rotate(-7deg) skewX(-14deg)',
				background: `linear-gradient(180deg, #2a0710 0%, ${INK} 100%)`, borderTop: `8px solid ${GOLD}`, borderBottom: `8px solid ${RED}`,
				boxShadow: '10px 12px 0 rgba(0,0,0,0.5)'}}>
				<div style={{fontFamily: 'BgArtist', fontSize: 74, color: '#fff', letterSpacing: 6, textAlign: 'center', lineHeight: '80px',
					transform: 'skewX(4deg)', textShadow: `4px 4px 0 ${RED}`}}>{artist.toUpperCase()}</div>
			</div>
			<AbsoluteFill style={{background: 'radial-gradient(ellipse 85% 85% at 50% 45%, transparent 60%, rgba(10,0,4,0.65) 100%)'}} />
		</AbsoluteFill>
	);
};
