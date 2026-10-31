import React from 'react';
import {interpolate, spring, useCurrentFrame, useVideoConfig} from 'remotion';
import {colors, fonts, headerGradient} from './tokens';

/* A caption card for the side panel, in the game's header colours. */
export const Caption: React.FC<{title: string; sub?: string; durationInFrames: number}> = ({title, sub, durationInFrames}) => {
	const frame = useCurrentFrame();
	const {fps} = useVideoConfig();
	const enter = spring({frame, fps, config: {damping: 18, stiffness: 140}});
	const exit = interpolate(frame, [durationInFrames - 10, durationInFrames], [1, 0], {extrapolateLeft: 'clamp', extrapolateRight: 'clamp'});
	return (
		<div style={{opacity: Math.min(enter, exit), transform: `translateX(${(1 - enter) * 40}px)`}}>
			<div style={{background: headerGradient, borderRadius: 12, padding: '10px 16px 12px', border: `2px solid ${colors.orangeLight}`,
				boxShadow: '0 6px 18px rgba(0,0,0,0.45)'}}>
				<div style={{fontFamily: fonts.heading, fontWeight: 800, fontStyle: 'italic', fontSize: 27, lineHeight: 1.12, color: colors.white,
					textShadow: `0 2px 0 ${colors.maroon}`}}>{title}</div>
			</div>
			{sub && (
				<div style={{fontFamily: fonts.body, fontWeight: 500, fontSize: 18, lineHeight: 1.42, color: colors.white, marginTop: 14, padding: '0 4px'}}>{sub}</div>
			)}
		</div>
	);
};
