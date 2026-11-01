#!/usr/bin/env bash
# Patch 0010, as a script: the recompiler's generated comments name the
# project rules generically instead of the upstream project's notes file.
# Run inside the PSXRecomp checkout, after 0009 (apply.sh does it in order).
set -euo pipefail
f=recompiler/src/full_function_emitter.cpp
sed -E -i \
  -e 's/\(([A-Z]+)\.md (\\xC2\\xA7"")/(project rules \2/' \
  -e 's/\([A-Z]+\.md Rule 18\)/(project rule 18)/g' "$f"
git -c user.name="DSF PC Port" -c user.email=jmokikearns@gmail.com \
  commit -q --date="Sat Oct 31 00:35:44 2026 +0000" -m "Emitter: generated comments name the project rules generically" -- "$f"
