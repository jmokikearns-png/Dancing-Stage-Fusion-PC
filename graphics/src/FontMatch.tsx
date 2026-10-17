import React, {useEffect, useState} from 'react';
import {AbsoluteFill, continueRender, delayRender, Img, staticFile} from 'remotion';
import {colors} from './tokens';

/* A look-up sheet for picking the free font nearest the game logo's
 * lettering (which is drawn art, not a font): the logo's own "Fusion" next
 * to the same word in each candidate, with the logo's layering, at the same
 * height.  Needs the player's logo in the public dir (dsf-logo.png). */
const CANDIDATES = ['knewave', 'shrikhand', 'bangers', 'lobster', 'kaushan-script', 'leckerli-one', 'chewy', 'titan-one',
	'sigmar-one', 'luckiest-guy', 'racing-sans-one', 'sriracha', 'yellowtail', 'caprasimo'];
const fontUrl = (id: string) => require(`@fontsource/${id}/files/${id}-latin-400-normal.woff2`);

export const LogoWord: React.FC<{text: string; family: string; size: number; skew?: number; rotate?: number; weight?: number; style?: string; s?: number}> =
	({text, family, size, skew = 0, rotate = 0, weight = 400, style = 'normal', s = 1}) => {
	const base: React.CSSProperties = {position: 'absolute', left: 0, top: 0, lineHeight: 1, paintOrder: 'stroke fill'};
	const layer = (w: number, c: string): React.CSSProperties => ({...base, WebkitTextStroke: `${w * s}px ${c}`, color: c});
	return (
		<div style={{fontFamily: family, fontSize: size, fontWeight: weight, fontStyle: style, whiteSpace: 'nowrap',
			transform: `rotate(${rotate}deg) skewX(${skew}deg)`, transformOrigin: 'left center', position: 'relative'}}>
			<span style={{visibility: 'hidden', lineHeight: 1}}>{text}</span>
			<span style={{...layer(13, colors.logoGreen), filter: `drop-shadow(0 0 ${2.5 * s}px ${colors.logoGreen})`}}>{text}</span>
			<span style={layer(9, colors.logoRim)}>{text}</span>
			<span style={layer(4.5, colors.logoInk)}>{text}</span>
			{/* background before the clip: the shorthand would reset it */}
			<span style={{...base, background: `linear-gradient(180deg, ${colors.logoFillTop} 18%, ${colors.logoFillMid} 52%, ${colors.logoFillBottom} 88%)`,
				WebkitBackgroundClip: 'text', backgroundClip: 'text', color: 'transparent'}}>{text}</span>
		</div>
	);
};

export const FontMatch: React.FC<{text?: string; ids?: string[]; withLogo?: boolean}> = ({text = 'Fusion', ids = CANDIDATES, withLogo = false}) => {
	const [handle] = useState(() => delayRender('candidate fonts'));
	useEffect(() => {
		Promise.all(ids.map((id) => new FontFace('M-' + id, `url(${fontUrl(id)})`).load().then((f) => document.fonts.add(f))))
			.then(() => continueRender(handle));
	}, [handle]);
	const cell = (label: string, body: React.ReactNode, i: number) => (
		<div key={label} style={{position: 'absolute', left: (i % 3) * 620, top: Math.floor(i / 3) * 200, width: 620, height: 200,
			background: colors.orange, borderRight: '2px solid #000', borderBottom: '2px solid #000', overflow: 'hidden'}}>
			<div style={{position: 'absolute', left: 10, top: 6, font: '700 22px Inter', color: '#000'}}>{label}</div>
			<div style={{position: 'absolute', left: 60, top: 50}}>{body}</div>
		</div>
	);
	return (
		<AbsoluteFill style={{background: '#222'}}>
			{cell('LOGO', <div style={{width: 420, height: 170, overflow: 'hidden', position: 'relative', top: -40, left: -40}}>
				<Img src={staticFile('dsf-logo.png')} style={{position: 'absolute', left: -140 * 2, top: -56 * 2, width: 400 * 2}} />
			</div>, 0)}
			{ids.map((id, i) => cell(id, withLogo
				? <div style={{position: 'relative', top: -50, left: -50, width: 600, height: 200}}>
					<Img src={staticFile('dsf-logo.png')} style={{position: 'absolute', left: -62, top: 0, width: 400 * 1.1}} />
					<div style={{position: 'absolute', left: (316 - 62) * 1.1, top: (103 - 28) * 1.1}}>
						<LogoWord text={text} family={'M-' + id} size={64} skew={-4} rotate={-7} s={0.85} /></div></div>
				: <LogoWord text={text} family={'M-' + id} size={110} skew={-6} rotate={-7} s={1.6} />, i + 1))}
		</AbsoluteFill>
	);
};
