import React from 'react';
import {AbsoluteFill, interpolate, spring, useCurrentFrame, useVideoConfig} from 'remotion';
import {colors, fonts, panelGradient} from './tokens';

const Backdrop: React.FC<{children: React.ReactNode}> = ({children}) => (
	<AbsoluteFill style={{background: panelGradient, alignItems: 'center', justifyContent: 'center'}}>
		<AbsoluteFill style={{background: `radial-gradient(ellipse at 50% 42%, rgba(245,154,27,0.55) 0%, rgba(216,53,42,0.25) 35%, rgba(0,0,0,0) 70%)`}} />
		<AbsoluteFill style={{alignItems: 'center', justifyContent: 'center'}}>{children}</AbsoluteFill>
	</AbsoluteFill>
);

const Title: React.FC<{size: number}> = ({size}) => (
	<div style={{fontFamily: fonts.heading, fontWeight: 800, fontStyle: 'italic', fontSize: size, color: colors.titleYellow,
		textShadow: `0 4px 0 ${colors.red}, 0 0 28px rgba(245,154,27,0.8)`, letterSpacing: 1, textAlign: 'center'}}>
		DANCING STAGE FUSION PC
	</div>
);

export const TitleCard: React.FC<{durationInFrames: number}> = ({durationInFrames}) => {
	const frame = useCurrentFrame();
	const {fps} = useVideoConfig();
	const s = spring({frame, fps, config: {damping: 14}});
	const out = interpolate(frame, [durationInFrames - 12, durationInFrames], [1, 0], {extrapolateLeft: 'clamp'});
	return (
		<Backdrop>
			<div style={{opacity: out, transform: `scale(${0.9 + 0.1 * s})`, textAlign: 'center'}}>
				<Title size={76} />
				<div style={{fontFamily: fonts.body, fontWeight: 700, fontSize: 28, color: colors.white, marginTop: 18, opacity: s}}>
					The PlayStation original, running inside StepMania 3.95
				</div>
				<div style={{fontFamily: fonts.body, fontWeight: 500, fontSize: 18, color: colors.mute, marginTop: 14, opacity: s}}>
					Fan project · built from my own disc · not affiliated with Konami
				</div>
			</div>
		</Backdrop>
	);
};

export const OutroCard: React.FC<{durationInFrames: number; points: string[]; note: string}> = ({durationInFrames, points, note}) => {
	const frame = useCurrentFrame();
	const {fps} = useVideoConfig();
	const fadeIn = interpolate(frame, [0, 12], [0, 1], {extrapolateRight: 'clamp'});
	return (
		<Backdrop>
			<div style={{opacity: fadeIn, textAlign: 'left'}}>
				<Title size={58} />
				<div style={{marginTop: 26}}>
					{points.map((p, i) => {
						const s = spring({frame: frame - 8 - i * 7, fps, config: {damping: 16}});
						return (
							<div key={i} style={{display: 'flex', alignItems: 'center', marginTop: 12, opacity: s, transform: `translateX(${(1 - s) * 24}px)`}}>
								<div style={{width: 14, height: 14, borderRadius: 3, background: colors.orange, transform: 'rotate(45deg)', marginRight: 18,
									boxShadow: `0 0 10px ${colors.orange}`}} />
								<span style={{fontFamily: fonts.body, fontWeight: 700, fontSize: 25, color: colors.white}}>{p}</span>
							</div>
						);
					})}
				</div>
				<div style={{fontFamily: fonts.body, fontWeight: 500, fontSize: 16, color: colors.mute, marginTop: 30, maxWidth: 900}}>{note}</div>
			</div>
		</Backdrop>
	);
};
