import React from 'react';
import {Composition} from 'remotion';
import {FPS, ShowOff, totalFrames} from './ShowOff';
import {sizes} from './tokens';

export const Root: React.FC = () => (
	<>
		<Composition id="ShowOff" component={ShowOff} durationInFrames={totalFrames()} fps={FPS}
			width={sizes.frameW} height={sizes.frameH} />
	</>
);
