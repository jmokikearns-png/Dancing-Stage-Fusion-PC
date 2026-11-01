#!/usr/bin/env bash
# Set up the build tools on a fresh Linux machine, in the layout the build
# scripts use (W=/home/user/work, as on the machine that built 1.0):
#   $W/tc/linux             conda-forge GCC 16 for x86_64-w64-mingw32, Linux-hosted
#                           (builds the game core, Setup.exe and the song tools)
#   $W/tc/win/Library       the same compiler, Windows-hosted (make_compiler.sh trims
#                           it into the release's setup\compiler)
#   $W/tc/pywin             Windows Python 3.12 for the release's song tools
#   $W/tc/wheels-songtools  numpy 2.0.2 and Pillow 13.0.0 for it (win_amd64, cp312)
#   $W/tc/wheels            imageio-ffmpeg 0.7.0 (its static ffmpeg.exe)
#   $W/tools/recomp-src/psxrecomp   PSXRecomp at the base commit, with patches/psxrecomp
#   $W/builds/deps-x86_64   the engine's libraries (scripts/windows-build/build-deps-mingw.sh)
# Every download is checked against the SHA-256 of the file 1.0 was built with.
#
#   setup_build_env.sh [toolchains] [psxrecomp] [deps]     (default: all three)
#
# Also needed from the system (Debian/Ubuntu names): git curl unzip zstd cmake
# ninja-build python3 python3-pil zip mingw-w64 (for the engine, the engine's
# libraries and the Windows recompiler).
set -euo pipefail
W=${W:-/home/user/work}
REPO=$(cd "$(dirname "$0")/.." && pwd)
PSXRECOMP_BASE=2a4824fcb897cb635438958f0c8cffb5a27df656
STEPS=${*:-toolchains psxrecomp deps}

for t in git curl unzip zstd tar sha256sum python3; do
  command -v "$t" >/dev/null || { echo "missing: $t" >&2; exit 1; }
done

fetch() { # url dest sha256
  if [ -f "$2" ] && echo "$3  $2" | sha256sum -c --status; then return; fi
  curl -fsSL --retry 4 -o "$2.part" "$1"
  echo "$3  $2.part" | sha256sum -c --status || { echo "checksum mismatch: $1" >&2; exit 1; }
  mv "$2.part" "$2"
}

# conda packages: target, channel subdir, file, sha256
CONDA=(
  "linux linux-64 binutils_impl_win-64-2.46.1-default_h4f5feae_103.conda 9f0a305e27efb9c55facaec277e2fa4f35fab02b96e9c19b96c08835e9434d40"
  "linux linux-64 gcc_impl_win-64-16.2.0-h25bbac7_7.conda c1651c781f5d336fcab643898c83eeb31cf95132668f13c87c3429b4d037fa32"
  "linux linux-64 gxx_impl_win-64-16.2.0-hb4564c5_7.conda 9e1a57515b219f3540708ba1d17a9ad80f2930c3b9f010d63112839d137163d1"
  "linux linux-64 ld_impl_win-64-2.46.1-default_h03da7db_103.conda 0249068547d61888ad893b9ea12dac7fef43513a7ca5987775134770aea028a9"
  "linux noarch libgcc-devel_win-64-16.2.0-h39e0259_7.conda e32a4f86f672aec5a31fb1a767b7267fc95d893a5895d86bf97bc9196cfb2afc"
  "linux noarch libstdcxx-devel_win-64-16.2.0-h18cb41e_7.conda c1131fa7d733335476591a47a16c888567e698b49d0447cedd03d111852588b7"
  "linux noarch mingw-w64-ucrt-x86_64-crt-git-12.0.0.r4.gg4f2fc60ca-h707e725_10.conda 59b3033b4a72fffcdffa9b95b3798e746d73f805d8413f07dcc3d97561e0f305"
  "linux noarch mingw-w64-ucrt-x86_64-headers-git-12.0.0.r4.gg4f2fc60ca-h707e725_10.conda 61e3e1bc4ef38ea296e33f88454b8c122f8fdea02f9f25eefcfb299c421fcc56"
  "linux noarch mingw-w64-ucrt-x86_64-windows-default-manifest-6.4-hd6d0fc0_7.conda 06f3c2d25169da1c91d29cd46e70a08401a5b703ef13b2f1b2be85f88cf2c1e4"
  "linux noarch mingw-w64-ucrt-x86_64-winpthreads-git-12.0.0.r4.gg4f2fc60ca-h707e725_10.conda ec9d92ec0f5f12d53b73b1612fe123d0d734ce39f75e8acf294cc34a90d87024"
  "win win-64 binutils_impl_win-64-2.46.1-bootstrap_h173839c_3.conda 71a36768949cd3e0fdad797cbb9c90d274124acbec2f21af0374c3862111a938"
  "win win-64 gcc_impl_win-64-16.2.0-h94e87e1_7.conda 795d89968b7e67745360ff57ebc84dcae51dac2cbd4b4861be2523dbab97b71c"
  "win win-64 gxx_impl_win-64-16.2.0-hdbe55fc_7.conda a9ee571df9ca3ec7ac8ec85916a12615e7c5ab0740d17f930abfb8730528935c"
  "win win-64 ld_impl_win-64-2.46.1-bootstrap_h8c62646_3.conda dae53f827313fb5806a6c042ecfbcaa67eea68813530dc979cd5cd07c6381e96"
  "win win-64 libgcc-16.2.0-h110b43a_7.conda 59feb6d16cba13a70f22dae70a44670eb2cc45e652e9a34a014814c2ca871132"
  "win noarch libgcc-devel_win-64-16.2.0-h254a5e0_107.conda ff9a753514d9632ce9ab55e6872c875d15fea24514b999c9078df0151d6a26db"
  "win win-64 libstdcxx-16.2.0-hae5796f_7.conda 5984f81dcb289446fb3cdb0117c0680bded57eaa98da193f705400ec94fc71ae"
  "win noarch libstdcxx-devel_win-64-16.2.0-h230208c_107.conda 1345b0c9187a7900370e2078b3568b6444d1c8de241f5d5a97e110fdc2a5f14a"
  "win win-64 libwinpthread-12.0.0.r688.g6b2176247.1-h57928b3_0.conda bab5558663cb0e964988b859abd69d0f2380b6ea9f140495ab543e51af13635d"
  "win noarch mingw-w64-ucrt-x86_64-crt-git-12.0.0.r4.gg4f2fc60ca-hd8ed1ab_10.conda de3e42149b498c16bfb485b7729f4ca0fe392be576a2a10ff702d661799b1df3"
  "win noarch mingw-w64-ucrt-x86_64-headers-git-12.0.0.r4.gg4f2fc60ca-hd8ed1ab_10.conda 1add86481f35163215e7076e6f06f22aa9f1f9345a5fff5cb07bc846c13fbec7"
  "win noarch mingw-w64-ucrt-x86_64-windows-default-manifest-6.4-he206cdd_7.conda 5b0df4e0ba8487ffd59f60c34c5dbb9e001ecd2c5d2c66ba88eada40bfa3ecb8"
  "win noarch mingw-w64-ucrt-x86_64-winpthreads-git-12.0.0.r4.gg4f2fc60ca-hd8ed1ab_10.conda 828abb111286940473c4c665fc8ab300d28920f5af83b32295e8bf2256a8f342"
  "pywin win-64 bzip2-1.0.8-h0ad9c76_10.conda 04767466ee9227c9c57ab2c6503e0149177d34111c7418d2f420297acb1eb229"
  "pywin noarch ca-certificates-2026.10.23-h4c7d964_0.conda f1d280d2618de987204102090e83733089c092dc2dd45686fd3dcbe809034f6a"
  "pywin win-64 libexpat-2.9.0-hac47afa_0.conda d4a8e5aac8999a90cd71aa6f9f5c40d8594ed2b89cd7f898f997767ecc6cd52d"
  "pywin win-64 libffi-3.7.0-h3d046cb_1.conda 613e8077c5cca5df7fe84ade7d5050301bee3c6c4c450ffe61d34a349ab4f11c"
  "pywin win-64 liblzma-5.8.3-hfd05255_1.conda d36c4a1e1f80fd08e18a407e03622ff2f34dfdd022da6488ad19603dea19e6d5"
  "pywin win-64 libpython-3.12.16-hfb4101e_0_cpython.conda dd208ae5026d15eb90763f6f41b26c502a52c66f77687fa7a8682b7ef33ffd5c"
  "pywin win-64 libsqlite-3.54.1-hf5d6505_101.conda 1f7ddd7d9383cf6ab6ee5a3a84e07a679a8539edc0ddd05fe476ff08142bf742"
  "pywin win-64 libzlib-1.3.2-hfd05255_3.conda 0629c2cc0404d3bb29d6baa7b4ba62da80797015e86de050db81ea5a07050527"
  "pywin win-64 openssl-3.6.5-hf411b9b_0.conda bc44625e0c6d83f3e02b5f6adc01e0e3ca7001d3d94e24959591a57f5a011b9a"
  "pywin win-64 python-3.12.16-hb12b558_0_cpython.conda c51326f96259254d1f977d131b3bd9a366021f4c96cd3724e9c74ec56fc4983e"
  "pywin noarch tzdata-2026e-h5d301ff_0.conda 2b7ca858879f183ce2dcc65b300f48a0133d5430550f35b23d80d405a0788dbf"
  "pywin win-64 vc14_runtime-14.51.36247-habf1de7_42.conda 8dfbfd68ebf53249d11e528cbb80ad5a2460b0c5b4636eed82ee0d3950827a3d"
  "pywin win-64 vcomp14-14.51.36247-habf1de7_42.conda b3558ed059d6272ef919ea959311af73c35b5fe43c789cd43f71b398671fec49"
)
# wheels from PyPI: folder, file, sha256
WHEELS=(
  "wheels-songtools numpy-2.0.2-cp312-cp312-win_amd64.whl cfd41e13fdc257aa5778496b8caa5e856dc4896d4ccf01841daee1d96465467a"
  "wheels-songtools pillow-13.0.0-cp312-cp312-win_amd64.whl ff3fe9eb621fbb9db4c4be33ab946f6bac5f98ab50ca8d3260503653656c961b"
  "wheels imageio_ffmpeg-0.7.0-py3-none-win_amd64.whl 7a22b45f60321d4172bdf2bc61f716bce4c81b85727bd59c7c7e371055a6a440"
)

toolchains() {
  local dl=$W/tc/downloads
  mkdir -p "$dl"
  for e in "${CONDA[@]}"; do
    read -r target sub file sha <<<"$e"
    fetch "https://conda.anaconda.org/conda-forge/$sub/$file" "$dl/$file" "$sha"
    # ONLY=linux|pywin|...: unpack only those (the downloads are all checked anyway)
    if [ -n "${ONLY:-}" ] && ! [[ "$target" =~ ^($ONLY)$ ]]; then echo "  $target: $file (checked)"; continue; fi
    mkdir -p "$W/tc/$target"
    for part in pkg info; do
      unzip -p "$dl/$file" "$part-*.tar.zst" | zstd -dc | tar -x -C "$W/tc/$target"
    done
    echo "  $target: $file"
  done
  for e in "${WHEELS[@]}"; do
    read -r folder file sha <<<"$e"
    name=${file%%-*}; ver=$(echo "$file" | cut -d- -f2)
    url=$(python3 -c 'import json,sys,urllib.request
d = json.load(urllib.request.urlopen("https://pypi.org/pypi/%s/%s/json" % (sys.argv[1], sys.argv[2])))
print(next(u["url"] for u in d["urls"] if u["filename"] == sys.argv[3]))' "$name" "$ver" "$file")
    mkdir -p "$W/tc/$folder"
    fetch "$url" "$W/tc/$folder/$file" "$sha"
    echo "  $folder: $file"
  done
}

psxrecomp() {
  local p=$W/tools/recomp-src/psxrecomp
  if [ ! -d "$p/.git" ]; then
    git clone --quiet https://github.com/mstan/psxrecomp "$p"
    git -C "$p" checkout --quiet -b dsfpc-embed "$PSXRECOMP_BASE"
    GIT_COMMITTER_NAME="DSF PC Port" GIT_COMMITTER_EMAIL=jmokikearns@gmail.com \
      bash "$REPO/patches/psxrecomp/apply.sh" "$p"
  fi
  # the core's CMake project expects the game project beside PSXRecomp
  local g=$W/analysis/recomp-feasibility/dsf-project
  mkdir -p "$g"
  cp "$REPO/game-project/CMakeLists.txt" "$REPO/game-project/game.toml" "$g/"
  [ -e "$g/psxrecomp" ] || ln -s "$p" "$g/psxrecomp"
}

deps() {
  bash "$REPO/scripts/windows-build/build-deps-mingw.sh" x86_64
}

for s in $STEPS; do
  echo "== $s"; "$s"
done
echo "done. Next: docs/BUILDING-RELEASE.md"
