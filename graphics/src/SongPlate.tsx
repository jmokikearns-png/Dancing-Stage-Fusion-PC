import React, {useEffect, useState} from 'react';
import {AbsoluteFill, continueRender, delayRender} from 'remotion';
import rounded from '@fontsource/m-plus-rounded-1c/files/m-plus-rounded-1c-latin-800-normal.woff2';

/* A song's title plate in the style of the game's own (music select,
 * results, records): title over artist, centred, bold rounded letters,
 * white with a dark outline, on transparent.  360x40 in the game; rendered
 * at 2x.  port/tools/extrasong/build_extrasong.py maps it onto the stock
 * plates' palette. */
export type SongPlateProps = {title: string; artist: string};

export const SongPlate: React.FC<SongPlateProps> = ({title, artist}) => {
	const [handle] = useState(() => delayRender('plate font'));
	useEffect(() => {
		new FontFace('PlateRounded', `url(${rounded})`, {weight: '800'}).load().then((f) => {
			document.fonts.add(f);
			continueRender(handle);
		});
	}, [handle]);
	const line = (text: string, top: number, size: number): React.ReactNode => (
		<div style={{position: 'absolute', left: 0, right: 0, top, textAlign: 'center', fontFamily: 'PlateRounded',
			fontWeight: 800, fontSize: size, lineHeight: 1, color: '#FFFFFF', whiteSpace: 'nowrap',
			WebkitTextStroke: '5px #10124A', paintOrder: 'stroke fill', letterSpacing: 1}}>{text}</div>
	);
	return (
		<AbsoluteFill>
			{line(title, 4, 32)}
			{line(artist, 44, 30)}
		</AbsoluteFill>
	);
};
