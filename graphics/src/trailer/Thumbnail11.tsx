import React from 'react';
import {AbsoluteFill, Img, staticFile} from 'remotion';
import '../fonts';
import {colors} from '../tokens';
import {LogoArt} from './cards';
import {Stripes, Title} from './parts';

/* The 1.1 trailer's thumbnail, 1280x720 (stills of the 1.1 footage in thumb/). */
const Frame: React.FC<{src: string; x: number; y: number; w: number; rot: number}> = ({src, x, y, w, rot}) => (
	<div style={{position: 'absolute', left: x, top: y, width: w, height: w * 0.75, transform: `rotate(${rot}deg)`,
		border: '7px solid #fff', borderRadius: 10, overflow: 'hidden', boxShadow: '0 18px 40px rgba(0,0,0,0.65), 0 0 0 4px #1a0503'}}>
		<Img src={staticFile(src)} style={{width: '100%', height: '100%', objectFit: 'cover'}} />
	</div>
);

export const Thumbnail11: React.FC = () => (
	<AbsoluteFill>
		<Stripes hue="orange" speed={0} />
		<AbsoluteFill style={{background: 'radial-gradient(ellipse 60% 70% at 28% 40%, rgba(255,200,90,0.25) 0%, rgba(0,0,0,0) 70%)'}} />
		<Frame src="thumb/v11_new.png" x={790} y={70} w={430} rot={6} />
		<Frame src="thumb/v11_res.png" x={700} y={300} w={430} rot={-6} />
		<div style={{position: 'absolute', left: 30, top: 50, transform: 'rotate(-3deg)', filter: 'drop-shadow(0 14px 22px rgba(0,0,0,0.7))'}}>
			<LogoArt k={1.5} frame={60} />
		</div>
		<div style={{position: 'absolute', left: 60, top: 340, transform: 'rotate(-3deg) skewX(-10deg)'}}>
			<div style={{background: `linear-gradient(180deg, ${colors.orangeLight} 0%, ${colors.orange} 40%, ${colors.red} 100%)`,
				padding: '12px 36px 18px', borderRadius: 12, border: '6px solid #1a0503', boxShadow: '0 12px 0 rgba(0,0,0,0.45)'}}>
				<div style={{transform: 'skewX(10deg)'}}><Title text="Version 1.1" size={92} /></div>
			</div>
		</div>
		<div style={{position: 'absolute', left: 74, top: 505, transform: 'rotate(-3deg)', filter: 'drop-shadow(0 6px 10px rgba(0,0,0,0.7))'}}>
			<Title text="ACHIEVEMENTS" size={52} />
			<div style={{height: 10}} />
			<Title text="+ NEW!! MARKER" size={52} />
		</div>
	</AbsoluteFill>
);
