import React from 'react';
import {AbsoluteFill, Audio, interpolate, OffthreadVideo, Sequence, staticFile, useCurrentFrame} from 'remotion';
import '../fonts';
import './serpentine';
import {BAB, barT, END_CARD, f, FPS} from './beat';
import {fonts} from '../tokens';
import {Console, EndCard, Intro, Logo, SetupConsole, SONGS_LINES} from './cards';
import {Flash, Shot, Slam, Stripes, Tag} from './parts';
import {COLD, COLD_SRC, cuts, MENU, PRE, Scene} from './edit';

const clamp = {extrapolateLeft: 'clamp', extrapolateRight: 'clamp'} as const;

export const trailerFrames = () => f(PRE) + f(MENU.endCardAt + END_CARD);

const SceneView: React.FC<{s: Scene; absStart: number; len: number}> = ({s, absStart, len}) => {
	switch (s.kind) {
		case 'intro': return <Intro beats={16} />;
		case 'logo': return <Logo len={len} />;
		case 'setup': return <SetupConsole len={len} />;
		case 'shot': return <Shot src={s.src!} from={s.at!} len={len} hue={s.hue} full={s.full} zoom={s.zoom} absStart={absStart} rate={s.rate} still={s.still} />;
		case 'songs': return <Console len={len} title="Build New Songs.bat" lines={SONGS_LINES} hue="green" size={27} />;
		case 'words': return <Stripes hue={s.hue || 'red'} speed={3} dim={0.25} />;
		default: return null;
	}
};

/* A small note over the gameplay: the choppiness is the recording's. */
const FootageNote: React.FC<{len: number}> = ({len}) => {
	const frame = useCurrentFrame();
	const o = interpolate(frame, [0, 15, len - 15, len], [0, 1, 1, 0], clamp);
	return (
		<AbsoluteFill style={{justifyContent: 'flex-end', alignItems: 'center', paddingBottom: 18, opacity: o}}>
			<div style={{background: 'rgba(8,6,6,0.78)', borderRadius: 8, padding: '7px 18px', fontFamily: fonts.body, fontWeight: 500,
				fontSize: 22, color: 'rgba(255,255,255,0.9)'}}>
				Note: the gameplay footage got choppy during recording and editing. The game itself is not this choppy.
			</div>
		</AbsoluteFill>
	);
};

/* The StepMania title menu, from the recording: it cuts in on the first
 * "beh"; the still menu is stretched so the "PC" stamp, played at its own
 * speed, lands on the last "bab". */
const MenuEnding: React.FC = () => {
	const frame = useCurrentFrame();
	const t = frame / FPS + BAB[0];
	const stampAt = BAB[4] - (MENU.pcLand - MENU.pcStart);	/* trailer time the stamp starts */
	const slowRate = (MENU.pcStart - MENU.cut) / (stampAt - BAB[0]);
	/* a small push-in on each syllable */
	let push = 0;
	BAB.forEach((b, i) => { if (t >= b) push = Math.max(push, (i === 4 ? 0.06 : 0.025) * Math.exp(-(t - b) * 9)); });
	const zoom = interpolate(t, [BAB[0], MENU.endCardAt], [1.08, 1.0], clamp) + push;
	const video = (from: number, rate: number) => (
		<OffthreadVideo src={staticFile(MENU.src)} startFrom={f(from)} playbackRate={rate} muted style={{width: '100%', height: '100%'}} />
	);
	return (
		<AbsoluteFill style={{background: '#000'}}>
			<Stripes hue="orange" speed={2} dim={0.2} />
			<AbsoluteFill style={{justifyContent: 'center', alignItems: 'center'}}>
				<div style={{width: 1440, height: 1080, transform: `scale(${zoom})`, overflow: 'hidden', position: 'relative'}}>
					<Sequence durationInFrames={f(stampAt - BAB[0])} layout="none">{video(MENU.cut, slowRate)}</Sequence>
					<Sequence from={f(stampAt - BAB[0])} layout="none">{video(MENU.pcStart, 1)}</Sequence>
				</div>
			</AbsoluteFill>
		</AbsoluteFill>
	);
};

/* The cold open: silent (the recorded game sound was choppy); the song starts after it. */
const ColdOpen: React.FC = () => {
	const frame = useCurrentFrame();
	let at = 0;
	const fadeIn = interpolate(frame, [0, 18], [0, 1], clamp);
	return (
		<AbsoluteFill style={{opacity: fadeIn}}>
			<Stripes hue="green" />
			<AbsoluteFill style={{justifyContent: 'center', alignItems: 'center'}}>
				<div style={{width: 1440, height: 1080, transform: 'scale(0.9)', overflow: 'hidden', borderRadius: 18, border: '6px solid #1a0503',
					boxShadow: '0 24px 60px rgba(0,0,0,0.7)', background: '#000', position: 'relative'}}>
					{COLD.map(([a, b], i) => {
						const from = at, len = f(b) - f(a);
						at += len;
						return (
							<Sequence key={i} from={from} durationInFrames={len} layout="none">
								<OffthreadVideo src={staticFile(COLD_SRC)} startFrom={f(a)} muted
									style={{width: '100%', height: '100%', position: 'absolute', inset: 0}} />
							</Sequence>
						);
					})}
				</div>
			</AbsoluteFill>
		</AbsoluteFill>
	);
};

export const Trailer: React.FC = () => (
	<AbsoluteFill style={{background: '#000'}}>
		<Sequence durationInFrames={f(PRE)} name="cold open"><ColdOpen /></Sequence>
		<Sequence from={f(PRE)} name="song"><Main /></Sequence>
	</AbsoluteFill>
);

const Main: React.FC = () => {
	const songEndFade = (fr: number) => interpolate(fr, [f(MENU.endCardAt + END_CARD - 1.5), f(MENU.endCardAt + END_CARD)], [1, 0], clamp);
	return (
		<AbsoluteFill style={{background: '#000'}}>
			<Audio src={staticFile('barber.wav')} volume={songEndFade} />
			{cuts.map((c, i) => {
				const from = f(c.from), len = f(c.to) - f(c.from);
				return (
					<Sequence key={i} from={from} durationInFrames={len} name={c.kind + (c.src ? ' ' + c.src : '')}>
						<SceneView s={c} absStart={from} len={len} />
						{c.slam && <Slam text={c.slam} sub={c.slamSub} size={c.slamSize} dur={len} delay={c.slamDelay ? f(c.slamDelay) : 0} y={c.slamY} />}
						{c.tag && <Tag text={c.tag} sub={c.tagSub} dur={len} delay={4} align={c.tagRight ? 'right' : 'left'} y={c.tagY} />}
						{/* no white flashes in the last stretch */}
					{c.flash !== false && c.from < barT(41) && <Flash len={7} strength={c.flash === 'big' ? 0.9 : 0.45} />}
					</Sequence>
				);
			})}
			<Sequence from={f(barT(12))} durationInFrames={f(barT(53)) - f(barT(12))} name="footage note">
				<FootageNote len={f(barT(53)) - f(barT(12))} />
			</Sequence>
			<Sequence from={f(BAB[0])} durationInFrames={f(MENU.endCardAt) - f(BAB[0]) + 10} name="StepMania menu">
				<MenuEnding />
			</Sequence>
			<Sequence from={f(MENU.endCardAt)} durationInFrames={f(END_CARD)} name="end card">
				<EndCard len={f(END_CARD)} />
			</Sequence>
		</AbsoluteFill>
	);
};

export {FPS};
