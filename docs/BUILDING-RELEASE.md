# Making a release

How the 1.0 release was built, step by step, so it can be rebuilt on a fresh
Linux machine. The paths are the ones the scripts use by default:
`W=/home/user/work` for tools and builds, and this repository at
`/home/user/dancing-stage-fusion-pc` (`REPO`).

What was checked when these notes were written:

- `scripts/setup_build_env.sh toolchains` downloads every toolchain file,
  checks it against the file 1.0 was built with, and unpacks the compilers
  and the Windows Python file for file identical to the 1.0 build machine's.
- `patches/psxrecomp/apply.sh` turns the base commit into exactly the
  PSXRecomp tree 1.0 was built from (`git diff` against it: nothing).
- The other steps are the commands and settings recorded in the 1.0 build
  folders; run them in order.

## 1. Tools

System packages (Debian/Ubuntu names):

```
git curl unzip zstd cmake ninja-build python3 python3-pil zip mingw-w64
```

Then:

```
scripts/setup_build_env.sh
```

It sets up, under `$W`: the conda-forge compilers (`tc/linux`, `tc/win`), the
Windows Python and wheels for the song tools (`tc/pywin`, `tc/wheels*`),
PSXRecomp with this project's patches (`tools/recomp-src/psxrecomp`), the
game project folder (`analysis/recomp-feasibility/dsf-project`) and the
engine's libraries (`builds/deps-x86_64`, from
`scripts/windows-build/build-deps-mingw.sh`). Steps can be run on their own:
`setup_build_env.sh toolchains`, `psxrecomp` or `deps`.

## 2. The game's code (from your disc, never committed)

Put your disc image at
`/home/user/work/original/disc/Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).bin` / `.cue`
(the `disc` line of `game-project/game.toml`).

The easiest way to get the recompiled code is the release's own Setup:
unzip a release, put the disc image in its `disc` folder and run
`Setup.exe` on Windows with `DSF_SETUP_KEEP=1` set. Its
`setup/work` folder then keeps `disc_exe/`, `seeds/` and `generated/`: copy
those three into `$W/analysis/recomp-feasibility/dsf-project/`.

(Setup does exactly this, `tools/setup/setup.c`: the game program from the
disc at LBA 24, the seeds from its `jal` targets and entry point, then
`psxrecomp-game --config game.toml --project-root <folder>`.)

## 3. The game core kit

```
cmake -S $W/analysis/recomp-feasibility/dsf-project \
      -B $W/analysis/recomp-feasibility/build-win64-ucrt -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=$REPO/scripts/win/toolchain-conda-ucrt.cmake \
      -DCMAKE_BUILD_TYPE=Release -DDSF_EMBED=ON -DPSX_EMBED=ON \
      -DCMAKE_C_STANDARD=17 -DCMAKE_CXX_STANDARD=17 -DPSX_ENABLE_VULKAN=OFF \
      -DDSF_PORT_DIR=$REPO
ninja -C $W/analysis/recomp-feasibility/build-win64-ucrt psx-runtime
```

`make_release.sh` takes the kit (everything but the game's own code) from
this build folder (`CB`).

## 4. The engine (StepMania)

```
cmake -S $REPO/engine -B $W/builds/dsfpc-win64 -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=$REPO/engine/cmake/toolchains/x86_64-w64-mingw32.cmake \
      -DCMAKE_BUILD_TYPE=Release -DSM_DEPS_PREFIX=$W/builds/deps-x86_64 \
      -DSM_STATIC_RUNTIME=ON -DSM_WITH_DSHOW=OFF -DSM_WITH_FFMPEG=OFF -DSM_WITH_D3D=OFF
ninja -C $W/builds/dsfpc-win64
```

## 5. The recompiler for Windows (Setup's `psxrecomp-game.exe`)

```
cmake -S $W/tools/recomp-src/psxrecomp/recompiler -B $W/builds/recompiler-win64 -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=$REPO/scripts/toolchain-x86_64-mingw-posix.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++"
ninja -C $W/builds/recompiler-win64
```

## 6. The Windows compiler for players, and the release

```
bash $REPO/scripts/dsfpc/make_compiler.sh      # tc/win -> builds/compiler-win64
bash $REPO/scripts/dsfpc/make_release.sh 1.1   # -> $W/deliverables/DancingStageFusionPC-1.1.zip
```

`make_release.sh` packages the engine, the theme and data, `Setup.exe`,
`Updater.exe` (`tools/updater`, built with the system MinGW and the engine's
zlib), a `VERSION` file, the kit, the compiler, the recompiler and the song tools, and refuses to package
anything from the game (disc images, `dsfcore.dll`, memory cards, game code).

## 7. Before publishing

- Unzip the release fresh, put a disc image in, run `Setup.exe`: its
  `dsfcore.dll` is byte for byte the same on every PC, so it should match
  the one from step 3's build of the same sources.
- Play a song, and run `Build New Songs.bat` with simfile packs in
  `simfiles`.
- Releases are published from GitHub's Releases page (tag `v1.1` and so on),
  with the zip attached: the updater picks the release's
  `DancingStageFusionPC-<version>.zip`, so keep that name.
