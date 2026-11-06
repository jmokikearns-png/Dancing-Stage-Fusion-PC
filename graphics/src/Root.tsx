import React from 'react';
import {Composition, Still} from 'remotion';
import {FontSheet, FontSheetProps} from './FontSheet';
import exo2Bold24 from '../fonts/exo2-700-24.json';
import {DsfLogoProps, LOGO_DEFAULTS, logoCanvas, PC_BOX, ThemeBackground, ThemeFooter, ThemeHeader, ThemeOptionsPage, ThemeSplash, ThemeTitleLogo, ThemeTitleLogoDSF, ThemeTitleLogoOwn, ThemeSplashNotice, ThemeInfoCursor} from './ThemeArt';
import {FPS, ShowOff, totalFrames} from './ShowOff';
import {sizes} from './tokens';
import {FontMatch} from './FontMatch';
import {SongPlate, SongPlateProps} from './SongPlate';
import {SongBackground, SongBackgroundProps} from './SongBackground';
import {BgGoWest, BgHeavenEarth, BgKidsInAmerica, BgMickey} from './SongBackgrounds';
import {Trailer, trailerFrames} from './trailer/Trailer';
import {Thumbnail} from './trailer/Thumbnail';
import {HotfixTrailer, HOTFIX_FPS, hotfixFrames} from './trailer/Hotfix';
import {FPS as TRAILER_FPS} from './trailer/beat';

export const Root: React.FC = () => (
	<>
		<Composition id="ShowOff" component={ShowOff} durationInFrames={totalFrames()} fps={FPS}
			width={sizes.frameW} height={sizes.frameH} />
		{/* the release trailer: needs its footage, song and font in the public dir (--public-dir, see trailer-tools/README.md) */}
		<Still id="TrailerThumbnail" component={Thumbnail} width={1280} height={720} />
		<Composition id="TrailerHotfix" component={HotfixTrailer} durationInFrames={hotfixFrames()} fps={HOTFIX_FPS} width={1920} height={1080} />
		<Composition id="Trailer" component={Trailer} durationInFrames={trailerFrames()} fps={TRAILER_FPS} width={1920} height={1080} />
		{/* StepMania theme: "Common Normal" in the heading font (fonts/build_font.py) */}
		<Still id="FontCommonNormal" component={FontSheet as React.FC<Record<string, unknown>>} width={1024} height={1024}
			defaultProps={{family: "'Exo 2'", weight: 700, size: 24, cell: 32, baseline: 24, scale: 2, outline: 1.25,
				widths: exo2Bold24.widths, chars: exo2Bold24.chars} satisfies FontSheetProps} />
		{/* StepMania theme graphics (theme/build_theme_art.sh), 2x the 640x480 layout */}
		<Still id="ThemeBackground" component={ThemeBackground} width={1280} height={960} />
		<Still id="ThemeTitleBackground" component={ThemeBackground} width={1280} height={960} defaultProps={{stars: 'title' as const}} />
		<Still id="ThemeHeader" component={ThemeHeader} width={1280} height={88} />
		<Still id="ThemeFooter" component={ThemeFooter} width={1280} height={64} />
		<Still id="ThemeOptionsPage" component={ThemeOptionsPage} width={1280} height={960} />
		<Still id="ThemeTitleLogo" component={ThemeTitleLogo} width={1280} height={240} />
		<Still id="ThemeSplashNotice" component={ThemeSplashNotice} width={1280} height={960} />
		<Still id="ThemeTitleLogoOwn" component={ThemeTitleLogoOwn} width={374 * 3} height={176 * 3} />
		{/* only with the player's own copy of the game's logo in the public dir */}
		{(['all', 'logo'] as const).map((part) => (
			<Still key={part} id={part === 'all' ? 'ThemeTitleLogoDSF' : 'ThemeTitleLogoDSFArt'} component={ThemeTitleLogoDSF as React.FC<Record<string, unknown>>}
				width={logoCanvas(LOGO_DEFAULTS).w} height={logoCanvas(LOGO_DEFAULTS).h}
				defaultProps={{...LOGO_DEFAULTS, part} satisfies DsfLogoProps} />
		))}
		<Still id="ThemeTitleLogoPC" component={ThemeTitleLogoDSF as React.FC<Record<string, unknown>>}
			width={PC_BOX.w * LOGO_DEFAULTS.scale} height={PC_BOX.h * LOGO_DEFAULTS.scale}
			defaultProps={{...LOGO_DEFAULTS, part: 'pc'} satisfies DsfLogoProps} />
		<Still id="FontMatch" component={FontMatch} width={1860} height={1000} />
		<Still id="FontMatchPC" component={FontMatch} width={1860} height={600}
			defaultProps={{text: 'PC', withLogo: true, ids: ['lobster', 'kaushan-script', 'leckerli-one', 'knewave', 'sriracha']}} />
		<Still id="ThemeInfoCursor" component={ThemeInfoCursor} width={320} height={56} />
		{/* an extra song's title plate (port/tools/extrasong) */}
		<Still id="SongPlate" component={SongPlate as React.FC<Record<string, unknown>>} width={720} height={80}
			defaultProps={{title: 'Title', artist: 'Artist'} satisfies SongPlateProps} />
		{/* an extra song's gameplay background, PS1-version style; the photo is the player's, in the public dir */}
		<Still id="SongBackground" component={SongBackground as React.FC<Record<string, unknown>>} width={1280} height={960}
			defaultProps={{photo: 'photo.png', photoCrop: {x: -190, y: 92, w: 470, h: 352}, titleLines: ['TOUGH', 'ENOUGH'],
				artist: 'Vanilla Ninja'} satisfies SongBackgroundProps} />
		<Still id="SongBackgroundLogos" component={SongBackground as React.FC<Record<string, unknown>>} width={1280} height={960}
			defaultProps={{photo: 'photo.png', photoCrop: {x: -190, y: 92, w: 470, h: 352}, titleLines: ['TOUGH', 'ENOUGH'],
				artist: 'Vanilla Ninja', titleLogo: {src: 'logo_stamp.png', w: 520, h: 292},
				artistLogo: {src: 'logo_script.png', w: 820, h: 190}} satisfies SongBackgroundProps} />
		<Still id="BgToughEnough-under" component={SongBackground as React.FC<Record<string, unknown>>} width={1280} height={960}
			defaultProps={{photo: 'photo.png', photoCrop: {x: -190, y: 92, w: 470, h: 352}, titleLines: ['TOUGH', 'ENOUGH'],
				artist: 'Vanilla Ninja', titleLogo: {src: 'logo_stamp.png', w: 520, h: 292},
				artistLogo: {src: 'logo_script.png', w: 820, h: 190}, layer: 'under'} satisfies SongBackgroundProps} />
		<Still id="BgToughEnough-rays" component={SongBackground as React.FC<Record<string, unknown>>} width={1280} height={960}
			defaultProps={{photo: 'photo.png', photoCrop: {x: -190, y: 92, w: 470, h: 352}, titleLines: ['TOUGH', 'ENOUGH'],
				artist: 'Vanilla Ninja', titleLogo: {src: 'logo_stamp.png', w: 520, h: 292},
				artistLogo: {src: 'logo_script.png', w: 820, h: 190}, layer: 'rays'} satisfies SongBackgroundProps} />
		<Still id="BgToughEnough-over" component={SongBackground as React.FC<Record<string, unknown>>} width={1280} height={960}
			defaultProps={{photo: 'photo.png', photoCrop: {x: -190, y: 92, w: 470, h: 352}, titleLines: ['TOUGH', 'ENOUGH'],
				artist: 'Vanilla Ninja', titleLogo: {src: 'logo_stamp.png', w: 520, h: 292},
				artistLogo: {src: 'logo_script.png', w: 820, h: 190}, layer: 'over'} satisfies SongBackgroundProps} />
		{/* gameplay backgrounds without the artists' photos, PS1-version style */}
		<Still id="BgKidsInAmerica" component={BgKidsInAmerica} width={1280} height={960} />
		<Still id="BgKidsInAmericaBase" component={BgKidsInAmerica} width={1280} height={960} defaultProps={{base: true}} />
		<Still id="BgGoWest" component={BgGoWest} width={1280} height={960} />
		<Still id="BgGoWestBase" component={BgGoWest} width={1280} height={960} defaultProps={{base: true}} />
		<Still id="BgHeavenEarth" component={BgHeavenEarth} width={1280} height={960} />
		<Still id="BgHeavenEarthBase" component={BgHeavenEarth} width={1280} height={960} defaultProps={{base: true}} />
		<Still id="BgMickey" component={BgMickey} width={1280} height={960} />
		<Still id="BgMickeyBase" component={BgMickey} width={1280} height={960} defaultProps={{base: true}} />
		<Still id="ThemeSplash" component={ThemeSplash} width={468} height={62} />
	</>
);
