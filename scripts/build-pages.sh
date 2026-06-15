#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PUBLIC_DIR="${PAGES_PUBLIC_DIR:-"${ROOT_DIR}/public"}"
SKILL_DIR="${STOPWATCH_SKILL_DIR:-"${ROOT_DIR}/../stopwatch-skill"}"

copy_dir() {
  local from="$1"
  local to="$2"

  if [[ ! -d "${from}" ]]; then
    echo "Missing source directory: ${from}" >&2
    exit 1
  fi

  mkdir -p "${to}"
  cp -a "${from}/." "${to}/"
}

rm -rf "${PUBLIC_DIR}"
mkdir -p "${PUBLIC_DIR}"

copy_dir "${ROOT_DIR}/site" "${PUBLIC_DIR}"
copy_dir "${ROOT_DIR}/thirdparty/Ratchet-StopWatch/web" "${PUBLIC_DIR}/apps/ratchet"
copy_dir "${ROOT_DIR}/thirdparty/Schulte-StopWatch/web" "${PUBLIC_DIR}/apps/schulte"
copy_dir "${SKILL_DIR}/site" "${PUBLIC_DIR}/skill"
copy_dir "${SKILL_DIR}/html-framework" "${PUBLIC_DIR}/skill/framework"

touch "${PUBLIC_DIR}/.nojekyll"

echo "Built GitHub Pages site at ${PUBLIC_DIR}"
