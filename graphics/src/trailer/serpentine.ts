import {continueRender, delayRender, staticFile} from 'remotion';

/* Serpentine (the player's own font file, in the public dir, not in the repository). */
const handle = delayRender('serpentine');
new FontFace('Serpentine', `url(${staticFile('serpentine.ttf')})`).load().then((face) => {
	document.fonts.add(face);
	continueRender(handle);
}).catch(() => continueRender(handle));
