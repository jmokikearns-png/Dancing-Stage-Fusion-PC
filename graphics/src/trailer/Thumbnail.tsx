import React from 'react';
import {AbsoluteFill, Img, staticFile} from 'remotion';
import '../fonts';
import {colors} from '../tokens';
import {LogoArt} from './cards';
import {Stripes, Title} from './parts';

/* The trailer's thumbnail, 1280x720: the logo, "NOW ON PC", and three songs
 * on their new backgrounds (stills from the recordings, thumb/ in the public dir). */
const Frame: React.FC<{src: string; x: number; y: number; w: number; rot: number; z: number}> = ({src, x, y, w, rot, z}) => (
	<div style={{position: 'absolute', left: x, top: y, width: w, height: w * 0.75, transform: `rotate(${rot}deg)`, zIndex: z,
		border: '7px solid #fff', borderRadius: 10, overflow: 'hidden', boxShadow: '0 18px 40px rgba(0,0,0,0.65), 0 0 0 4px #1a0503'}}>
		<Img src={staticFile(src)} style={{width: '100%', height: '100%', objectFit: 'cover'}} />
	</div>
);

export const Thumbnail: React.FC = () => (
	<AbsoluteFill>
		<Stripes hue="green" speed={0} />
		<AbsoluteFill style={{background: 'radial-gradient(ellipse 60% 70% at 28% 45%, rgba(255,200,60,0.28) 0%, rgba(0,0,0,0) 70%)'}} />
		<Frame src="thumb/gowest.png" x={735} y={40} w={470} rot={7} z={1} />
		<Frame src="thumb/mickey.png" x={790} y={330} w={440} rot={-5} z={2} />
		<Frame src="thumb/tough.png" x={560} y={250} w={420} rot={-11} z={3} />
		<div style={{position: 'absolute', left: 20, top: 70, zIndex: 5, transform: 'rotate(-4deg)', filter: 'drop-shadow(0 14px 22px rgba(0,0,0,0.7))'}}>
			<LogoArt k={1.6} frame={60} />
		</div>
		<div style={{position: 'absolute', left: 48, top: 400, zIndex: 6, transform: 'rotate(-4deg) skewX(-10deg)'}}>
			<div style={{background: `linear-gradient(180deg, ${colors.orangeLight} 0%, ${colors.orange} 40%, ${colors.red} 100%)`,
				padding: '14px 40px 20px', borderRadius: 12, border: '6px solid #1a0503', boxShadow: '0 12px 0 rgba(0,0,0,0.45)'}}>
				<div style={{transform: 'skewX(10deg)'}}><Title text="NOW ON PC" size={112} /></div>
			</div>
		</div>
		<div style={{position: 'absolute', left: 92, top: 585, zIndex: 6, transform: 'rotate(-4deg)'}}>
			<Title text="12 NEW SONGS" size={64} style={{filter: 'drop-shadow(0 6px 10px rgba(0,0,0,0.7))'}} />
		</div>
	</AbsoluteFill>
);
