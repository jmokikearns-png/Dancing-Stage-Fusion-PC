/* Dancing Stage Fusion PC: the one set of design tokens for every new
 * graphic the port adds (video captions, title cards, StepMania theme art,
 * in-game headers).  Colours are sampled from the game's own OPTIONS screens
 * and the DSF PC theme; fonts are free (SIL OFL) so the results can ship. */

export const colors = {
	/* the game's OPTIONS screens */
	orange: '#F59A1B',
	orangeLight: '#FFC23D',
	red: '#D8352A',
	maroon: '#5C1212',
	maroonDark: '#2A0606',
	valueGreen: '#3BE04F',
	labelBlue: '#2F55E8',
	/* the DSF PC StepMania theme */
	titleYellow: '#F5C842',
	ink: '#0B0B0F',
	white: '#FFFFFF',
	mute: 'rgba(255,255,255,0.72)',
};

export const fonts = {
	heading: "'Exo 2'",	/* 800 italic: close to the game's italic header lettering */
	body: "'Inter'",
};

export const sizes = {
	frameW: 1280,
	frameH: 720,
	gameW: 960,	/* the game window, captured 1:1 */
	gameH: 720,
	radius: 14,
	pad: 28,
};

/* The game's own header: orange to red, with a bright top edge. */
export const headerGradient = `linear-gradient(180deg, ${colors.orangeLight} 0%, ${colors.orange} 38%, ${colors.red} 100%)`;
export const panelGradient = `linear-gradient(180deg, ${colors.maroon} 0%, ${colors.maroonDark} 100%)`;
