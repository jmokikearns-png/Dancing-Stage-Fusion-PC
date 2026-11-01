#!/usr/bin/env bash
# Apply this project's changes to a PSXRecomp checkout at the base commit:
#   apply.sh PATH/TO/psxrecomp
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
cd "${1:?usage: apply.sh PATH/TO/psxrecomp}"
for p in "$here"/0*; do
  case "$p" in
    *.patch) git am -q --keep-cr "$p" ;;
    *.sh)    bash "$p" ;;
  esac
done
git log --oneline -1
