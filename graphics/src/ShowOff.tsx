import React from 'react';
import {AbsoluteFill, interpolate, OffthreadVideo, Sequence, staticFile, useCurrentFrame} from 'remotion';
import './fonts';
import {Caption} from './Caption';
import {OutroCard, TitleCard} from './Cards';
import {colors, fonts, headerGradient, panelGradient, sizes} from './tokens';
import {edit, Segment} from './edit';

export const FPS = 50;
const FADE = 10;
const sec = (s: number) => Math.round(s * FPS);

export const segmentFrames = (s: Segment) => sec(s.to - s.from);
export const totalFrames = () => sec(edit.intro) + edit.segments.reduce((n, s) => n + segmentFrames(s), 0) + sec(edit.outro);

/* The game window 1:1 on the left; the captions in a side panel, so nothing
 * in the game picture is ever covered. */
const GameSegment: React.FC<{seg: Segment}> = ({seg}) => {
	const frame = useCurrentFrame();
	const n = segmentFrames(seg);
	const fade = interpolate(frame, [0, FADE, n - FADE, n], [0, 1, 1, 0], {extrapolateLeft: 'clamp', extrapolateRight: 'clamp'});
	const panelX = sizes.gameW;
	return (
		<AbsoluteFill style={{background: colors.ink}}>
			<div style={{position: 'absolute', left: 0, top: 0, width: sizes.gameW, height: sizes.gameH, opacity: fade}}>
				<OffthreadVideo src={staticFile(seg.src)} startFrom={sec(seg.from)} endAt={sec(seg.to)}
					volume={(f) => interpolate(f, [0, FADE, n - FADE, n], [0, 1, 1, 0], {extrapolateLeft: 'clamp', extrapolateRight: 'clamp'})}
					style={{width: sizes.gameW, height: sizes.gameH}} />
			</div>
			<div style={{position: 'absolute', left: panelX, top: 0, width: sizes.frameW - panelX, height: sizes.frameH, background: panelGradient,
				borderLeft: `3px solid ${colors.orange}`, boxSizing: 'border-box', padding: '30px 24px'}}>
				<div style={{fontFamily: fonts.heading, fontWeight: 800, fontStyle: 'italic', fontSize: 25, lineHeight: 1.05, color: colors.titleYellow,
					textShadow: `0 3px 0 ${colors.red}`}}>DANCING STAGE<br />FUSION PC</div>
				<div style={{height: 3, background: headerGradient, margin: '18px 0 0', borderRadius: 2, width: 120}} />
				<div style={{position: 'absolute', left: 24, right: 24, bottom: 26, fontFamily: fonts.body, fontWeight: 500, fontSize: 14,
					lineHeight: 1.4, color: colors.mute}}>Win64 build<br />(no graphics card: software renderer)</div>
				<div style={{position: 'absolute', left: 24, right: 24, top: 190}}>
					{(seg.captions || []).map((c, i) => (
						<Sequence key={i} from={sec(c.at)} durationInFrames={sec(c.dur)} layout="none">
							<Caption title={c.title} sub={c.sub} durationInFrames={sec(c.dur)} />
						</Sequence>
					))}
				</div>
			</div>
		</AbsoluteFill>
	);
};

export const ShowOff: React.FC = () => {
	let at = sec(edit.intro);
	return (
		<AbsoluteFill style={{background: colors.ink}}>
			<Sequence durationInFrames={sec(edit.intro)}>
				<TitleCard durationInFrames={sec(edit.intro)} />
			</Sequence>
			{edit.segments.map((s, i) => {
				const from = at;
				at += segmentFrames(s);
				return (
					<Sequence key={i} from={from} durationInFrames={segmentFrames(s)}>
						<GameSegment seg={s} />
					</Sequence>
				);
			})}
			<Sequence from={at} durationInFrames={sec(edit.outro)}>
				<OutroCard durationInFrames={sec(edit.outro)} points={edit.outroPoints} note={edit.outroNote} />
			</Sequence>
		</AbsoluteFill>
	);
};
