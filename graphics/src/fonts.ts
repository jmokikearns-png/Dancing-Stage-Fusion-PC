import {continueRender, delayRender} from 'remotion';
import exo800i from '@fontsource/exo-2/files/exo-2-latin-800-italic.woff2';
import exo600i from '@fontsource/exo-2/files/exo-2-latin-600-italic.woff2';
import inter500 from '@fontsource/inter/files/inter-latin-500-normal.woff2';
import inter700 from '@fontsource/inter/files/inter-latin-700-normal.woff2';
import exo700 from '@fontsource/exo-2/files/exo-2-latin-700-normal.woff2';
import lobster from '@fontsource/lobster/files/lobster-latin-400-normal.woff2';

/* Load the token fonts before the first frame renders. */
const faces: Array<[string, string, string, string]> = [
	['Exo 2', exo800i, '800', 'italic'],
	['Exo 2', exo600i, '600', 'italic'],
	['Inter', inter500, '500', 'normal'],
	['Inter', inter700, '700', 'normal'],
	['Exo 2', exo700, '700', 'normal'],
	['Lobster', lobster, '400', 'normal'],
];
const handle = delayRender('fonts');
Promise.all(
	faces.map(([family, url, weight, style]) =>
		new FontFace(family, `url(${url})`, {weight, style}).load().then((f) => {
			document.fonts.add(f);
		}),
	),
).then(() => continueRender(handle));
