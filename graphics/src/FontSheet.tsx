import React from 'react';
import './fonts';
import {colors, fonts} from './tokens';

/* A StepMania bitmap font page: 16x16 cells, character code = row * 16 +
 * column (Latin-1), each glyph centred on its advance width in its cell with
 * its baseline at `baseline` (StepMania crops each cell to the width, from
 * both sides).  Rendered at `scale` x the logical cell size; the file name's
 * "(res WxH)" hint gives StepMania the logical size. */
export type FontSheetProps = {
	family: string;
	weight: number;
	size: number;		/* logical px */
	cell: number;		/* logical px */
	baseline: number;	/* logical px from the cell's top */
	scale: number;
	outline: number;	/* logical px; 0 = none */
	widths: number[];	/* 256 advance widths, logical px */
	chars: number[];	/* the code point in each cell (Windows-1252 order); 0 = empty */
};

export const FontSheet: React.FC<FontSheetProps> = ({family, weight, size, cell, baseline, scale, outline, widths, chars}) => {
	const c = cell * scale;
	return (
		<svg width={16 * c} height={16 * c} xmlns="http://www.w3.org/2000/svg">
			{widths.map((w, code) => {
				if (w <= 0 || !chars[code] || code === 32 || code === 0xA0) return null;
				const x = (code % 16) * c + (c - w * scale) / 2;
				const y = Math.floor(code / 16) * c + baseline * scale;
				return (
					<text key={code} x={x} y={y} fontFamily={family} fontWeight={weight} fontSize={size * scale}
						fill={colors.white} stroke={outline ? colors.maroonDark : 'none'} strokeWidth={outline * 2 * scale}
						strokeLinejoin="round" paintOrder="stroke" style={{fontKerning: 'none'}}>
						{String.fromCodePoint(chars[code])}
					</text>
				);
			})}
		</svg>
	);
};

export const fontSheetFamily = fonts.heading;
