#!/usr/bin/env bash
# Fetch and build the pinned tools for gran-turismo-decomp.
# Idempotent; writes only under tools/ and .venv/; never uses sudo.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$REPO/tools/src"
BIN="$REPO/tools/bin"
GHIDRA_DIR="$REPO/tools/ghidra"

PSPDECRYPT_URL=https://github.com/John-K/pspdecrypt.git
PSPDECRYPT_REV=c156627db7634d395c380c0a9589130f603307fc
PSPRECOMP_URL=https://github.com/jessicanataliagta/PSPRecomp.git
PSPRECOMP_REV=f6e7d415c7f447b934cc3865a31eb725f353d659
GHIDRA_URL=https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.1.3_build/ghidra_12.1.3_PUBLIC_20260817.zip
GHIDRA_SHA256=93a5d11a9ad510622acaaf908c556a7b9b764d338e78a7567f3689bf5081fd54
GHIDRA_HOME="$GHIDRA_DIR/ghidra_12.1.3_PUBLIC"
ALLEGREX_URL=https://github.com/kotcrab/ghidra-allegrex/releases/download/v21.4/ghidra_12.1.3_PUBLIC_20260825_ghidra-allegrex.zip
ALLEGREX_SHA256=62e829fa9e6ed6f2bd343763fb339a3f8812b02a4e0ed5a9409b14007b408243

with_ghidra=0
for arg in "$@"; do
  case "$arg" in
    --with-ghidra) with_ghidra=1 ;;
    -h|--help) echo "usage: tools/setup.sh [--with-ghidra]"; exit 0 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

step() { printf '==> %s\n' "$*"; }
need() { command -v "$1" >/dev/null || { echo "missing required command: $1" >&2; exit 1; }; }

# checkout URL REV DIR — shallow fetch of exactly REV; no-op if DIR is already at REV
checkout() {
  local url=$1 rev=$2 dir=$3
  if [ -d "$dir/.git" ] && [ "$(git -C "$dir" rev-parse HEAD)" = "$rev" ]; then
    echo "    up to date"; return 0
  fi
  rm -rf "$dir"; mkdir -p "$dir"
  git -C "$dir" init --quiet
  git -C "$dir" fetch --quiet --depth 1 "$url" "$rev"
  git -C "$dir" checkout --quiet FETCH_HEAD
}

# fetch_verified URL SHA256 DEST — download once, refuse a wrong hash
fetch_verified() {
  local url=$1 sum=$2 dest=$3
  if [ -f "$dest" ] && echo "$sum  $dest" | sha256sum -c --status; then
    echo "    cached $(basename "$dest")"; return 0
  fi
  curl -fsSL --retry 3 -o "$dest.part" "$url"
  if ! echo "$sum  $dest.part" | sha256sum -c --status; then
    rm -f "$dest.part"; echo "SHA256 mismatch for $url" >&2; exit 1
  fi
  mv "$dest.part" "$dest"
}

# capped CMD... — run a parallel job under the project's memory ceiling (AGENTS.md § memory cgroup)
capped() {
  if command -v systemd-run >/dev/null; then
    systemd-run --user --scope --quiet -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- "$@"
  else
    "$@"
  fi
}

# unpack ZIP PARENT TOP — extract ZIP's top-level dir TOP into PARENT atomically (marker on success),
# so an interrupted unzip is redone on the next run instead of being trusted
unpack() {
  local zip=$1 parent=$2 top=$3 tmp="$2/.unpack-$3"
  [ -f "$parent/$top/.unpacked" ] && return 0
  rm -rf "$parent/$top" "$tmp"; mkdir -p "$tmp"
  unzip -q "$zip" -d "$tmp"
  mv "$tmp/$top" "$parent/$top"; rmdir "$tmp"
  touch "$parent/$top/.unpacked"
}

need git
mkdir -p "$SRC" "$BIN"

step "python venv (.venv) from tools/requirements.txt"
need uv
[ -x "$REPO/.venv/bin/python" ] || uv venv --quiet "$REPO/.venv"
uv pip install --quiet --python "$REPO/.venv/bin/python" -r "$REPO/tools/requirements.txt"

step "pspdecrypt @ ${PSPDECRYPT_REV:0:7}"
need make; need clang++
checkout "$PSPDECRYPT_URL" "$PSPDECRYPT_REV" "$SRC/pspdecrypt"
if [ ! -x "$BIN/pspdecrypt" ] || [ "$(cat "$BIN/.pspdecrypt.rev" 2>/dev/null)" != "$PSPDECRYPT_REV" ]; then
  log="$SRC/pspdecrypt.build.log"
  if ! capped make -C "$SRC/pspdecrypt" -j8 >"$log" 2>&1; then
    tail -n 20 "$log" >&2; echo "pspdecrypt build failed (full log: $log)" >&2; exit 1
  fi
  install -m 755 "$SRC/pspdecrypt/pspdecrypt" "$BIN/pspdecrypt"
  echo "$PSPDECRYPT_REV" > "$BIN/.pspdecrypt.rev"
fi

step "PSPRecomp @ ${PSPRECOMP_REV:0:7} (source only; built in Phase 2)"
checkout "$PSPRECOMP_URL" "$PSPRECOMP_REV" "$SRC/PSPRecomp"

if [ "$with_ghidra" = 1 ]; then
  step "Ghidra 12.1.3 + ghidra-allegrex v21.4 (repo-local)"
  need curl; need unzip; need sha256sum
  mkdir -p "$GHIDRA_DIR/downloads"
  fetch_verified "$GHIDRA_URL" "$GHIDRA_SHA256" "$GHIDRA_DIR/downloads/ghidra.zip"
  fetch_verified "$ALLEGREX_URL" "$ALLEGREX_SHA256" "$GHIDRA_DIR/downloads/ghidra-allegrex.zip"
  unpack "$GHIDRA_DIR/downloads/ghidra.zip" "$GHIDRA_DIR" "$(basename "$GHIDRA_HOME")"
  unpack "$GHIDRA_DIR/downloads/ghidra-allegrex.zip" "$GHIDRA_HOME/Ghidra/Extensions" ghidra-allegrex
  # Ghidra 12.1 needs Java >= 21; pick the newest JDK >= 21 under /usr/lib/jvm without touching the system default.
  jdk=""
  for d in $(ls -d /usr/lib/jvm/java-*-openjdk 2>/dev/null | sort -V -r); do
    v=$(basename "$d" | sed -E 's/java-([0-9]+)-openjdk/\1/')
    if [ "$v" -ge 21 ] && [ -x "$d/bin/java" ]; then jdk=$d; break; fi
  done
  [ -n "$jdk" ] || { echo "no JDK >= 21 found under /usr/lib/jvm (install e.g. jdk21-openjdk)" >&2; exit 1; }
  props="$GHIDRA_HOME/support/launch.properties"
  if grep -q '^JAVA_HOME_OVERRIDE=' "$props"; then
    sed -i "s|^JAVA_HOME_OVERRIDE=.*|JAVA_HOME_OVERRIDE=$jdk|" "$props"
  else
    echo "JAVA_HOME_OVERRIDE=$jdk" >> "$props"
  fi
  grep -qx "JAVA_HOME_OVERRIDE=$jdk" "$props" || { echo "could not set JAVA_HOME_OVERRIDE in $props" >&2; exit 1; }
  echo "    GHIDRA_INSTALL_DIR=$GHIDRA_HOME"
  echo "    JAVA_HOME=$jdk"
fi

step "done"
