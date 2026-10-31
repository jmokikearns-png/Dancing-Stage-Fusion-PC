import React, {useEffect, useState} from 'react';
import {AbsoluteFill, continueRender, delayRender, Img, staticFile} from 'remotion';
import './fonts';
import {colors, fonts, headerGradient, panelGradient} from './tokens';
import layout from './logoLayout.json';

/* Graphics of the DSF PC StepMania theme, in the look of the game's own
 * OPTIONS screens (orange body with starbursts, red header, maroon panels),
 * drawn from the design tokens.  Rendered at 2x; the theme's file names
 * carry "(res WxH)" with the 640x480 layout size.  All shapes are our own:
 * nothing is copied from the game's artwork. */

/* An 8-pointed starburst. */
const Star: React.FC<{x: number; y: number; r: number; rot?: number; color: string; opacity: number}> = ({x, y, r, rot = 0, color, opacity}) => {
	const pts: string[] = [];
	for (let i = 0; i < 16; i++) {
		const a = (Math.PI * 2 * i) / 16 + (rot * Math.PI) / 180;
		const rr = i % 2 === 0 ? r : r * 0.42;
		pts.push(`${(x + Math.cos(a) * rr).toFixed(1)},${(y + Math.sin(a) * rr).toFixed(1)}`);
	}
	return <polygon points={pts.join(' ')} fill={color} opacity={opacity} />;
};

/* The screens' backdrop: orange body, soft vertical bands, starbursts. */
export const ThemeBackground: React.FC<{stars?: 'menu' | 'title'}> = ({stars = 'menu'}) => {
	const bands = [90, 260, 420, 610, 760, 980, 1150];
	const starList = stars === 'title'
		? [{x: 640, y: 380, r: 520, rot: 8, o: 0.22}, {x: 170, y: 760, r: 190, rot: 20, o: 0.3}, {x: 1130, y: 180, r: 160, rot: -10, o: 0.3}, {x: 1080, y: 820, r: 110, rot: 4, o: 0.35}]
		: [{x: 210, y: 300, r: 230, rot: 12, o: 0.28}, {x: 1060, y: 620, r: 280, rot: -6, o: 0.25}, {x: 760, y: 170, r: 110, rot: 30, o: 0.32}, {x: 420, y: 800, r: 140, rot: 2, o: 0.3}, {x: 1180, y: 160, r: 70, rot: 15, o: 0.35}];
	return (
		<AbsoluteFill style={{background: `linear-gradient(180deg, #F9AE2A 0%, ${colors.orange} 45%, #E2730E 100%)`}}>
			<svg width="1280" height="960" style={{position: 'absolute'}}>
				{bands.map((x, i) => (
					<rect key={i} x={x} y={0} width={60 + (i % 3) * 40} height={960} fill={i % 2 ? '#FFD066' : '#D9650B'} opacity={0.18} />
				))}
				{starList.map((s, i) => <Star key={i} x={s.x} y={s.y} r={s.r} rot={s.rot} color="#FFC94A" opacity={s.o} />)}
			</svg>
			<AbsoluteFill style={{background: 'radial-gradient(ellipse at 50% 45%, rgba(0,0,0,0) 55%, rgba(120,30,0,0.35) 100%)'}} />
		</AbsoluteFill>
	);
};

/* The bar along the top of every menu screen (the screen's name is drawn on
 * its centre plate by StepMania). */
export const ThemeHeader: React.FC = () => (
	<AbsoluteFill style={{background: `linear-gradient(180deg, ${colors.red} 0%, #A8221B 100%)`}}>
		<div style={{position: 'absolute', left: 0, right: 0, top: 0, height: 6, background: colors.orangeLight}} />
		<div style={{position: 'absolute', left: 0, right: 0, bottom: 0, height: 5, background: colors.maroonDark}} />
		<svg width="1280" height="88" style={{position: 'absolute'}}>
			<polygon points="400,6 880,6 846,83 434,83" fill={colors.maroon} opacity={0.75} />
			{[0, 1, 2].map((i) => (
				<g key={i} fill={colors.orange} opacity={0.9 - i * 0.25}>
					<polygon points={`${370 - i * 34},22 ${388 - i * 34},44 ${370 - i * 34},66 ${352 - i * 34},66 ${370 - i * 34},44 ${352 - i * 34},22`} />
					<polygon points={`${910 + i * 34},22 ${892 + i * 34},44 ${910 + i * 34},66 ${928 + i * 34},66 ${910 + i * 34},44 ${928 + i * 34},22`} />
				</g>
			))}
		</svg>
	</AbsoluteFill>
);

/* The bar along the bottom (help text is drawn on it). */
export const ThemeFooter: React.FC = () => (
	<AbsoluteFill style={{background: panelGradient}}>
		<div style={{position: 'absolute', left: 0, right: 0, top: 0, height: 5, background: colors.red}} />
	</AbsoluteFill>
);

/* The options screens' frame: a dark panel behind the rows. */
export const ThemeOptionsPage: React.FC = () => (
	<AbsoluteFill>
		<div style={{position: 'absolute', left: 84, top: 104, width: 1116, height: 800, borderRadius: 26, overflow: 'hidden',
			background: 'rgba(42,6,6,0.84)', border: `4px solid ${colors.orange}`, boxSizing: 'border-box',
			boxShadow: '0 10px 30px rgba(60,10,0,0.45)'}}>
			<div style={{height: 14, background: headerGradient}} />
		</div>
	</AbsoluteFill>
);

/* The title screen's logo. */
export const ThemeTitleLogo: React.FC = () => (
	<AbsoluteFill style={{alignItems: 'center', justifyContent: 'center'}}>
		<div style={{fontFamily: fonts.heading, fontWeight: 800, fontStyle: 'italic', fontSize: 86, color: colors.titleYellow,
			textShadow: `0 7px 0 ${colors.red}, 0 0 34px rgba(245,154,27,0.9), 0 0 6px rgba(42,6,6,0.9)`, letterSpacing: 2,
			WebkitTextStroke: `3px ${colors.maroonDark}`, paintOrder: 'stroke fill', whiteSpace: 'nowrap'}}>
			DANCING STAGE FUSION PC
		</div>
	</AbsoluteFill>
);

/* The title screen's logo: our own design (none of the game's logo is
 * used or traced). "Dancing Stage" over a big "Fusion" in Lobster with the
 * banded look of our "PC" (green glow, yellow rim, dark ink, green fill),
 * tilted, over a slanted ink plate with a yellow rim. Same canvas as the theme's logo frame (374x176 at `scale`), so
 * "PC" (ThemeTitleLogoPC) stamps on at the same place beside "Fusion". */
export const ThemeTitleLogoOwn: React.FC<{scale?: number}> = ({scale = 3}) => {
	const s = scale, W = 374, H = 176;
	const plate = [[16, 52], [304, 30], [316, 134], [30, 158]];
	const pts = plate.map(([x, y]) => `${x * s},${y * s}`).join(' ');
	return (
		<AbsoluteFill>
			<svg width={W * s} height={H * s} style={{position: 'absolute'}}>
				<defs>
					<linearGradient id="plate" x1="0" y1="0" x2="0" y2="1">
						<stop offset="0%" stopColor="#0B5A4A" />
						<stop offset="100%" stopColor={colors.logoInk} />
					</linearGradient>
				</defs>
				<polygon points={pts} fill={colors.logoGreen} stroke={colors.logoGreen} strokeWidth={14 * s} strokeLinejoin="round" />
				<polygon points={pts} fill={colors.logoRim} stroke={colors.logoRim} strokeWidth={7 * s} strokeLinejoin="round" />
				<polygon points={pts} fill="url(#plate)" stroke={colors.logoInk} strokeWidth={2 * s} strokeLinejoin="round" />
			</svg>
			<div style={{position: 'absolute', left: 26 * s, top: 38 * s, transform: `rotate(-6deg) skewX(-6deg)`, transformOrigin: 'left top'}}>
				<LogoText text="Dancing Stage" size={38} s={s} />
			</div>
			<div style={{position: 'absolute', left: 36 * s, top: 76 * s, transform: `rotate(-6deg) skewX(-6deg)`, transformOrigin: 'left top'}}>
				<LogoText text="Fusion" size={64} s={s} />
			</div>
		</AbsoluteFill>
	);
};

/* The splash before the title menu (ScreenPS1Splash): our logo and a note
 * from the author on what Setup did, for players whose antivirus is wary. */
export const ThemeSplashNotice: React.FC = () => {
	const line: React.CSSProperties = {fontFamily: fonts.heading, fontWeight: 700, color: '#FFF4D6', fontSize: 30, lineHeight: 1.35,
		textShadow: `0 2px 0 ${colors.maroonDark}`, textAlign: 'center', margin: '0 0 22px 0'};
	return (
		<AbsoluteFill>
			<ThemeBackground stars="title" />
			<div style={{position: 'absolute', left: 120, top: 34, width: 1040, height: 400, display: 'flex', justifyContent: 'center'}}>
				<div style={{transform: 'scale(0.62)', transformOrigin: 'top center', width: 374 * 3, height: 176 * 3, position: 'relative'}}>
					<ThemeTitleLogoOwn />
					<div style={{position: 'absolute', left: (374 / 2 + 70) * 3 - 189, top: (176 / 2 + 14) * 3 - 156, width: 378, height: 312,
						display: 'flex', alignItems: 'center', justifyContent: 'center'}}>
						<div style={{transform: 'translateY(24px) rotate(-7deg) skewX(-4deg)'}}><LogoText text="PC" size={64} s={3} /></div>
					</div>
				</div>
			</div>
			<div style={{position: 'absolute', left: 110, top: 400, width: 1060, padding: '34px 44px 26px', boxSizing: 'border-box', borderRadius: 26,
				background: 'rgba(42,6,6,0.84)', border: `4px solid ${colors.orange}`, boxShadow: '0 10px 30px rgba(60,10,0,0.45)'}}>
				<p style={{...line, color: colors.titleYellow, fontSize: 36, fontWeight: 800}}>A free, open-source fan project</p>
				<p style={line}>Setup.exe built the game on your PC, from your own disc, with the compiler that comes with it.
					Antivirus software can be wary of new, unsigned programs that build other programs. That is all it is.</p>
				<p style={{...line, color: colors.titleYellow, fontSize: 26, whiteSpace: 'nowrap'}}>Source code: github.com/jmokikearns-png/Dancing-Stage-Fusion-PC</p>
				<p style={{...line, fontStyle: 'italic', margin: 0}}>- CrashReporter</p>
			</div>
			<div style={{position: 'absolute', left: 0, right: 0, bottom: 26, textAlign: 'center', fontFamily: fonts.heading, fontWeight: 700,
				fontSize: 24, color: 'rgba(255,244,214,0.85)', textShadow: `0 2px 0 ${colors.maroonDark}`}}>Press Enter to continue</div>
		</AbsoluteFill>
	);
};

/* The loading window's banner (468x62, shown 1:1 by StepMania). */
export const ThemeSplash: React.FC = () => (
	<AbsoluteFill style={{background: panelGradient, alignItems: 'center', justifyContent: 'center', borderBottom: `3px solid ${colors.orange}`}}>
		<div style={{position: 'absolute', left: 0, right: 0, top: 0, height: 4, background: headerGradient}} />
		<div style={{fontFamily: fonts.heading, fontWeight: 800, fontStyle: 'italic', fontSize: 31, color: colors.titleYellow,
			textShadow: `0 2px 0 ${colors.red}`, letterSpacing: 0.5}}>DANCING STAGE FUSION PC</div>
	</AbsoluteFill>
);

/* The title screen's logo when the player supplies the game's own logo
 * (theme/build_theme_art.sh LOGO=...; it is never part of this project):
 * their logo, cropped to its artwork, with "PC" after "Fusion" in the font
 * nearest the logo's lettering and the logo's own layering (green edge,
 * yellow rim, dark ink, green fill).  Two layers, so the theme can animate
 * them: the logo, and "PC" in its own small canvas.  Sizes are in the
 * logo's own pixels (400x175 art: "Fusion" spans y 64-130, ends at x 327);
 * the logo file can be any multiple of that (upscaled). */
export type DsfLogoProps = {
	src: string;
	logoW: number; logoH: number;
	cropX: number; cropW: number;	/* the part of the logo kept */
	padR: number;			/* room after it for "PC" */
	pcX: number; pcY: number;	/* "PC": left edge, vertical centre */
	pcSize: number;
	scale: number;
	part: 'all' | 'logo' | 'pc';
	vector?: string;	/* upscale/vectorize_logo.py's layers, in the public dir: redraw the logo clean */
};
const L = layout;
export const LOGO_DEFAULTS: DsfLogoProps = {
	src: 'dsf-logo.png', logoW: L.logoW, logoH: L.logoH, cropX: L.cropX, cropW: L.cropW, padR: L.padR,
	pcX: L.pcX, pcY: L.pcY, pcSize: L.pcSize, scale: L.scale, part: 'all', vector: 'dsf-logo-vector.json',
};

type LogoVector = {w: number; h: number; fill_texture: string; layers: Array<{name: string; transform: string; d: string}>};

/* The logo redrawn from its traced layers: crisp flat edge, rim and ink at
 * any size, and the letters' own shading (vectorize_logo.py's smooth fill
 * texture) clipped to the traced fill. */
const VectorLogo: React.FC<{file: string; width: number; height: number}> = ({file, width, height}) => {
	const [v, setV] = useState<LogoVector | null>(null);
	const [handle] = useState(() => delayRender('logo vector'));
	useEffect(() => {
		fetch(staticFile(file)).then((r) => r.json()).then((j: LogoVector) => { setV(j); continueRender(handle); });
	}, [file, handle]);
	if (!v)
		return null;
	const layer = (name: string) => v.layers.find((l) => l.name === name)!;
	const path = (name: string, fill: string, extra: React.SVGProps<SVGGElement> = {}) => (
		<g transform={layer(name).transform} {...extra}><path d={layer(name).d} fill={fill} /></g>
	);
	const k = v.w / 400;	/* the vector's pixels per logo pixel */
	return (
		<svg width={width} height={height} viewBox={`0 0 ${v.w} ${v.h}`} style={{position: 'absolute', left: 0, top: 0}}>
			<defs>
				<filter id="glow" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation={1.6 * k} /></filter>
				{/* a clip path takes shapes only, not groups */}
				<clipPath id="fillclip"><path transform={layer('fill').transform} d={layer('fill').d} /></clipPath>
				<linearGradient id="edgegrad" x1="0" y1="0" x2="0" y2="1">
					<stop offset="0" stopColor="#6FD727" /><stop offset="1" stopColor={colors.logoGreen} />
				</linearGradient>
			</defs>
			{path('edge', colors.logoGreen, {filter: 'url(#glow)', opacity: 0.9})}
			{path('edge', 'url(#edgegrad)')}
			{path('rim', colors.logoRim)}
			{path('ink', colors.logoInk)}
			<g clipPath="url(#fillclip)">
				<image href={staticFile(v.fill_texture)} x={0} y={0} width={v.w} height={v.h} />
			</g>
		</svg>
	);
};
export const PC_BOX = {w: L.pcBoxW, h: L.pcBoxH};
/* StepMania wants even frame sizes: the canvas height is rounded up */
export const logoCanvas = (p: DsfLogoProps) => ({w: (p.cropW + p.padR) * p.scale, h: (p.logoH + (p.logoH % 2)) * p.scale});
/* the centre of "PC"'s canvas, from the centre of the logo's (1x pixels) */
export const pcOffset = (p: DsfLogoProps) => ({x: p.pcX - p.cropX + PC_BOX.w / 2 - 12 - (p.cropW + p.padR) / 2, y: p.pcY - (p.logoH + (p.logoH % 2)) / 2});	/* also theme/build_theme_art.sh */

export const LogoText: React.FC<{text: string; size: number; s: number}> = ({text, size, s}) => {
	const base: React.CSSProperties = {position: 'absolute', left: 0, top: 0, lineHeight: 1, paintOrder: 'stroke fill'};
	const layer = (w: number, c: string): React.CSSProperties => ({...base, WebkitTextStroke: `${w * s}px ${c}`, color: c});
	return (
		<div style={{fontFamily: fonts.logo, fontSize: size * s, whiteSpace: 'nowrap', position: 'relative'}}>
			<span style={{visibility: 'hidden', lineHeight: 1}}>{text}</span>
			{/* band widths as the logo's: green 6, yellow 3.5, ink 2.5 logo pixels (a stroke is half outside) */}
			<span style={{...layer(24, colors.logoGreen), filter: `drop-shadow(0 0 ${1.6 * s}px ${colors.logoGreen})`}}>{text}</span>
			<span style={layer(12, colors.logoRim)}>{text}</span>
			<span style={layer(5, colors.logoInk)}>{text}</span>
			{/* background before the clip: the shorthand would reset it */}
			<span style={{...base, background: `linear-gradient(180deg, ${colors.logoFillTop} 14%, ${colors.logoFillMid} 50%, ${colors.logoFillBottom} 90%)`,
				WebkitBackgroundClip: 'text', backgroundClip: 'text', color: 'transparent'}}>{text}</span>
		</div>
	);
};

export const ThemeTitleLogoDSF: React.FC<DsfLogoProps> = (p) => {
	const s = p.scale;
	const pc = (
		/* the glyphs sit high in their line box: centre them in "PC"'s canvas */
		<div style={{transform: `translateY(${8 * s}px) rotate(-7deg) skewX(-4deg)`}}><LogoText text="PC" size={p.pcSize} s={s} /></div>
	);
	if (p.part === 'pc')
		return <AbsoluteFill style={{alignItems: 'center', justifyContent: 'center'}}>{pc}</AbsoluteFill>;
	const o = pcOffset(p);
	return (
		<AbsoluteFill>
			{p.vector
				? <div style={{position: 'absolute', left: -p.cropX * s, top: 0}}><VectorLogo file={p.vector} width={p.logoW * s} height={p.logoH * s} /></div>
				: <Img src={staticFile(p.src)} style={{position: 'absolute', left: -p.cropX * s, top: 0, width: p.logoW * s, height: p.logoH * s}} />}
			{p.part === 'all' && (
				<div style={{position: 'absolute', width: PC_BOX.w * s, height: PC_BOX.h * s, display: 'flex', alignItems: 'center', justifyContent: 'center',
					left: ((p.cropW + p.padR) / 2 + o.x - PC_BOX.w / 2) * s, top: ((p.logoH + (p.logoH % 2)) / 2 + o.y - PC_BOX.h / 2) * s}}>{pc}</div>
			)}
		</AbsoluteFill>
	);
};

/* INFO's topic cursor (ScreenPS1Info): the options screens' highlight, an
 * orange bar with a bright top edge, behind the chosen topic. */
export const ThemeInfoCursor: React.FC = () => (
	<AbsoluteFill style={{padding: 4}}>
		<div style={{flex: 1, borderRadius: 10, background: headerGradient, border: `3px solid ${colors.maroonDark}`,
			boxShadow: `0 0 10px ${colors.orangeLight}`}} />
	</AbsoluteFill>
);
