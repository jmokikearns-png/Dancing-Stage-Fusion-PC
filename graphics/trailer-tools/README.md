# The release trailer

The trailer (`src/trailer/`, composition `Trailer`, thumbnail
`TrailerThumbnail`) is cut to *A Stupid Barber*; the 1.0.1 trailer
(`TrailerHotfix`, `Hotfix.tsx`) to *KEEP ON MOVIN'* (`str_decode.py 13 keeponmovin.wav`) and uses footage of the
game. Neither is in this repository: they come from your own disc and your
own recordings, in a folder you pass to Remotion as `--public-dir`. That
folder holds:

| File | Where it comes from |
|---|---|
| `barber.wav` | `python3 str_decode.py 12 barber.wav`: the song (stream 12) decoded from your disc image (the path is at the top of the script) |
| `serpentine.ttf` | your own copy of the Serpentine font (not included) |
| `text/*.png` | `python3 gen_text.py`: the titles drawn in Serpentine |
| `logo_main.png`, `logo_pc.png` | `npx remotion still src/index.ts ThemeTitleLogoOwn logo_main.png` and `... ThemeTitleLogoPC logo_pc.png` |
| `clips/*.mp4` | 4.2 s cuts of screen recordings of the game window (960x720, 50 fps), at the times listed in `src/trailer/edit.ts` (`CLIPS`: recording, second); `clips/select.mp4` is the cold open |
| `m.mp4` | a recording of the StepMania splash and title menu (`MENU` in `edit.ts`) |
| `thumb/*.png` | stills from the recordings, for the thumbnail |

`sheet.py` makes contact sheets of extracted frames, for finding cut points.

Render:

```
npx remotion render src/index.ts Trailer trailer.mp4 --public-dir=PATH --crf=17 --audio-bitrate=320k
npx remotion still src/index.ts TrailerThumbnail thumbnail.png --public-dir=PATH
```

The trailer contains the game's music and pictures: it is for your own
videos, not for this repository.
