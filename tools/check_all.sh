#!/usr/bin/env bash
# The full check of the engine, editor and web player in one command (docs/TESTING.md).
#
#   tools/check_all.sh [--jobs N] [--work DIR] [--only a,b] [--skip a,b] [--skip-e2e] [--skip-web]
#                      [--with-audio-test] [--emsdk DIR] [--sims-baseline FILE] [--update-baseline]
#                      [--strict] [--keep-logs]
#
# Parts (run in this order): sdl3 sdl2 editor-api ctest sims webtest vitest e2e
#   sdl3        build with SDL3 (folder build/), run ote_tests
#   sdl2        build with SDL2 (build-sdl2/), run ote_tests
#   editor-api  tests/editor_api_test.py against build/ote-editor
#   ctest       every ctest entry except the one that plays sound (see --with-audio-test)
#   sims        the sample games' simulators, compared with the previous run's output
#   webtest     tools/webtest browser checks of the web player (Chromium, then Firefox)
#   vitest      editor/web: typecheck, production build, vitest unit tests
#   e2e         editor/web Playwright end-to-end tests (Chromium; Firefox runs the @smoke ones)
#
# Output: every part's full log is in <logs>/<part>.log; the end of the run prints one PASS / FAIL / SKIPPED line per
# part plus the same greppable result lines the tools print ("N test(s) run, M failed", "N passed, M failed",
# SIMS_IDENTICAL, vitest and Playwright totals, e2e minutes). Exit code 0 only if no part FAILED (--strict: and none
# was SKIPPED).
#
# What it never does: use port 8765 (the editor helper and the web tests pick free ports), open a window or a sound
# device (the player runs --headless --no-audio inside the tests, the test browsers are muted), play the ctest
# "audio_player" test (only with --with-audio-test), read a game that git does not track (games/vision-quest and the
# owner's other projects: with --work the copy is made from `git ls-files`), or install anything. A part whose
# prerequisite is missing (no Emscripten, no node_modules, no Playwright browsers, no SDL2) is reported SKIPPED with
# the reason.
set -u

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
WORK=""
ONLY=""
SKIP=""
AUDIO_TEST=0
EMSDK_DIR="${EMSDK:-}"
BASELINE=""
UPDATE_BASELINE=0
STRICT=0
ALL_PARTS="sdl3 sdl2 editor-api ctest sims webtest vitest e2e"

usage() { sed -n '2,/^set -u/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'; }

while [ $# -gt 0 ]; do
  case "$1" in
    --jobs) JOBS="${2:?--jobs needs a number}"; shift 2 ;;
    --work) WORK="${2:?--work needs a folder}"; shift 2 ;;
    --only) ONLY="$(echo "${2:?--only needs a list}" | tr ',' ' ')"; shift 2 ;;
    --skip) SKIP="$SKIP $(echo "${2:?--skip needs a list}" | tr ',' ' ')"; shift 2 ;;
    --skip-e2e) SKIP="$SKIP e2e"; shift ;;
    --skip-web) SKIP="$SKIP webtest vitest e2e"; shift ;;
    --with-audio-test) AUDIO_TEST=1; shift ;;
    --emsdk) EMSDK_DIR="${2:?--emsdk needs a folder}"; shift 2 ;;
    --sims-baseline) BASELINE="${2:?--sims-baseline needs a file}"; shift 2 ;;
    --update-baseline) UPDATE_BASELINE=1; shift ;;
    --strict) STRICT=1; shift ;;
    --keep-logs) shift ;;  # accepted for old habits: logs are always kept
    -h|--help) usage; exit 0 ;;
    *) echo "unknown option: $1 (try --help)" >&2; exit 2 ;;
  esac
done
case "$JOBS" in ''|*[!0-9]*) echo "--jobs must be a number" >&2; exit 2 ;; esac
for p in $ONLY $SKIP; do
  case " $ALL_PARTS " in *" $p "*) ;; *) echo "unknown part: $p (parts: $ALL_PARTS)" >&2; exit 2 ;; esac
done

# ---- the tree to check ---------------------------------------------------------------------------------------
# Default: this checkout, built in its own build folders. --work DIR: a snapshot of the TRACKED files (working-tree
# contents, so uncommitted edits count; untracked and ignored files, games included, are left out) is made in
# DIR/src and everything is built and run there, so the checkout can keep changing meanwhile.
SIMS_HOME="$SRC/build/check-all"
if [ -n "$WORK" ]; then
  if ! git -C "$SRC" rev-parse --git-dir >/dev/null 2>&1; then
    echo "--work needs a git checkout (the copy is made from the tracked files)" >&2; exit 2
  fi
  mkdir -p "$WORK" || exit 2
  WORK="$(cd "$WORK" && pwd)"
  if [ -n "$(ls -A "$WORK")" ] && [ ! -f "$WORK/.ote-check-all" ]; then
    echo "$WORK is not empty and was not made by check_all.sh; pick an empty or new folder" >&2; exit 2
  fi
  touch "$WORK/.ote-check-all"
  ROOT="$WORK/src"
  LOGS="$WORK/logs"
  rm -rf "$ROOT"; mkdir -p "$ROOT" "$LOGS"
  ( cd "$SRC" || exit 1
    git ls-files -z | while IFS= read -r -d '' f; do [ -e "$f" ] && printf '%s\0' "$f"; done \
      | tar --null -T - -cf - ) | ( cd "$ROOT" && tar xf - ) || { echo "could not copy the tracked files" >&2; exit 2; }
  # node_modules is a prerequisite, not tracked: reuse the checkout's own if it has them (a copy; nothing is installed).
  for nm in editor/web/node_modules tools/webtest/node_modules; do
    [ -d "$SRC/$nm" ] && [ ! -d "$ROOT/$nm" ] && cp -a "$SRC/$nm" "$ROOT/$nm"
  done
else
  ROOT="$SRC"
  LOGS="$ROOT/build/check-all"
  mkdir -p "$LOGS"
fi
[ -n "$BASELINE" ] || BASELINE="$SIMS_HOME/sims_baseline.txt"

# ---- bookkeeping ---------------------------------------------------------------------------------------------
RESULTS=()      # "part|STATUS|detail"
RESULT_LINES=() # greppable lines, printed again at the end
FAILED=0
SKIPPED=0

selected() {
  case " $SKIP " in *" $1 "*) return 1 ;; esac
  [ -z "$ONLY" ] && return 0
  case " $ONLY " in *" $1 "*) return 0 ;; esac
  return 1
}
record() {  # part status detail
  RESULTS+=("$1|$2|$3")
  [ "$2" = FAIL ] && FAILED=$((FAILED + 1))
  [ "$2" = SKIPPED ] && SKIPPED=$((SKIPPED + 1))
  echo "[$2] $1${3:+: $3}"
}
keep_line() {  # label, line
  [ -n "$2" ] || return 0
  RESULT_LINES+=("$1: $2")
  echo "  $1: $2"
}
have() { command -v "$1" >/dev/null 2>&1; }
gen_args() {  # -G Ninja for a new build folder when ninja exists; an existing folder keeps its generator
  if [ ! -f "$1/CMakeCache.txt" ] && have ninja; then echo "-G Ninja"; fi
}
now() { date +%s; }

# ---- builds ----------------------------------------------------------------------------------------------------
# build_dir NAME [cmake args...] -> 0 ok, 1 failed, 2 skipped (reason in BUILD_REASON)
BUILD_REASON=""
build_dir() {
  local name="$1"; shift
  local dir="$ROOT/$name"
  BUILD_REASON=""
  have cmake || { BUILD_REASON="cmake is not installed"; return 2; }
  # shellcheck disable=SC2046
  if ! ( cd "$ROOT" && cmake -S . -B "$name" $(gen_args "$dir") "$@" ) > "$LOGS/$name-configure.log" 2>&1; then
    if grep -qiE "Could not find a package configuration file provided by \"?SDL[23]|SDL[23].*not found" "$LOGS/$name-configure.log"; then
      BUILD_REASON="$(grep -oiE 'SDL[23]' "$LOGS/$name-configure.log" | head -1) development files not found (see $LOGS/$name-configure.log)"
      return 2
    fi
    BUILD_REASON="configure failed (see $LOGS/$name-configure.log)"; return 1
  fi
  if ! ( cd "$ROOT" && cmake --build "$name" -j "$JOBS" ) > "$LOGS/$name-build.log" 2>&1; then
    BUILD_REASON="build failed (see $LOGS/$name-build.log)"; return 1
  fi
  local w
  w="$(grep -ciE "warning:" "$LOGS/$name-build.log")"
  [ "$w" -gt 0 ] && echo "  note: $name build printed $w warning line(s) (the project builds with none; see $LOGS/$name-build.log)"
  return 0
}

MAIN_STATE=""   # "", ok, fail, skip
MAIN_REASON=""
ensure_main_build() {
  if [ -z "$MAIN_STATE" ]; then
    build_dir build; case $? in 0) MAIN_STATE=ok ;; 1) MAIN_STATE=fail ;; *) MAIN_STATE=skip ;; esac
    MAIN_REASON="$BUILD_REASON"
  fi
  [ "$MAIN_STATE" = ok ]
}
# After a failed ensure_main_build: record the part as FAIL or SKIPPED and say why.
main_unavailable() {
  if [ "$MAIN_STATE" = skip ]; then record "$1" SKIPPED "main build unavailable: $MAIN_REASON"
  else record "$1" FAIL "$MAIN_REASON"; fi
}

run_ote_tests() {  # part, build folder
  local part="$1" dir="$2" line rc
  ( cd "$ROOT" && "./$dir/ote_tests" ) > "$LOGS/$part.log" 2>&1; rc=$?
  line="$(grep -E "test\(s\) run, [0-9]+ failed" "$LOGS/$part.log" | tail -1)"
  keep_line "ote_tests ($part)" "$line"
  if [ $rc -eq 0 ] && echo "$line" | grep -q " 0 failed"; then record "$part" PASS "$line"
  else record "$part" FAIL "${line:-no result line}; see $LOGS/$part.log"; fi
}

part_sdl3() {
  if ! ensure_main_build; then main_unavailable sdl3; return; fi
  run_ote_tests sdl3 build
}

part_sdl2() {
  build_dir build-sdl2 -DOTE_PLATFORM=sdl2
  case $? in
    0) run_ote_tests sdl2 build-sdl2 ;;
    1) record sdl2 FAIL "$BUILD_REASON" ;;
    *) record sdl2 SKIPPED "$BUILD_REASON" ;;
  esac
}

part_editor_api() {
  have python3 || { record editor-api SKIPPED "python3 is not installed"; return; }
  if ! ensure_main_build; then main_unavailable editor-api; return; fi
  [ -x "$ROOT/build/ote-editor" ] || { record editor-api SKIPPED "build/ote-editor was not built (OTE_BUILD_EDITOR=OFF?)"; return; }
  local line rc
  ( cd "$ROOT" && OTE_TEST_TMP="$ROOT/build/test-tmp" PYTHONDONTWRITEBYTECODE=1 \
      python3 tests/editor_api_test.py ./build/ote-editor . ) > "$LOGS/editor-api.log" 2>&1; rc=$?
  line="$(grep -E "^[0-9]+ passed, [0-9]+ failed" "$LOGS/editor-api.log" | tail -1)"
  keep_line "editor API test" "$line"
  if [ $rc -eq 0 ] && echo "$line" | grep -q " 0 failed"; then record editor-api PASS "$line"
  else record editor-api FAIL "${line:-no result line}; see $LOGS/editor-api.log"; fi
}

part_ctest() {
  have ctest || { record ctest SKIPPED "ctest is not installed"; return; }
  have python3 || { record ctest SKIPPED "python3 is not installed (most entries are Python scripts)"; return; }
  if ! ensure_main_build; then main_unavailable ctest; return; fi
  local excl=(-E audio_player) rc line
  # audio_player really opens the sound device. Never run by default.
  [ "$AUDIO_TEST" -eq 1 ] && excl=()
  ( cd "$ROOT" && ctest --test-dir build "${excl[@]}" -j "$JOBS" --output-on-failure ) > "$LOGS/ctest.log" 2>&1; rc=$?
  line="$(grep -E "tests passed, [0-9]+ tests failed" "$LOGS/ctest.log" | tail -1)"
  keep_line "ctest" "$line"
  local skipped
  skipped="$(grep -cE "\*\*\*Skipped|Skipped" "$LOGS/ctest.log")"
  if [ $rc -eq 0 ]; then record ctest PASS "$line"
  else record ctest FAIL "${line:-no result line}; see $LOGS/ctest.log"; fi
  [ "$skipped" -gt 0 ] && echo "  (ctest reports $skipped skipped entr$( [ "$skipped" -eq 1 ] && echo y || echo ies ); render3d_d3d11 is Windows only)"
}

part_sims() {
  if ! ensure_main_build; then main_unavailable sims; return; fi
  local sim="$ROOT/build/ote-sim" out="$LOGS/sims.txt" ok=1
  [ -x "$sim" ] || { record sims SKIPPED "build/ote-sim was not built"; return; }
  : > "$LOGS/sims.log"
  ( cd "$ROOT" || exit 1
    "$sim" games/demo-srpg bridge --runs 30
    "$sim" games/demo-srpg north_road --runs 30
    "$sim" games/demo-economy twin_rivers --runs 30
    "$sim" games/demo-hex fortress_ford --runs 30
    "$sim" games/demo-srpg --campaign --runs 20 ) > "$LOGS/sims.log" 2>&1 || ok=0
  # Compare without the timing, which differs from run to run.
  grep summary "$LOGS/sims.log" | sed -E 's/; avg [0-9.]+ ms\/run//' > "$out"
  [ "$(wc -l < "$out")" -eq 5 ] || ok=0
  if [ "$ok" -eq 0 ]; then record sims FAIL "a simulator failed or printed no summary; see $LOGS/sims.log"; return; fi
  mkdir -p "$(dirname "$BASELINE")"
  if [ ! -f "$BASELINE" ]; then
    cp "$out" "$BASELINE"
    keep_line "sims" "SIMS_BASELINE_SAVED ($BASELINE; nothing to compare with yet)"
    record sims PASS "no earlier output to compare with; this run saved as the baseline"
  elif diff -q "$BASELINE" "$out" >/dev/null; then
    keep_line "sims" "SIMS_IDENTICAL"
    [ "$UPDATE_BASELINE" -eq 1 ] && cp "$out" "$BASELINE"
    record sims PASS "SIMS_IDENTICAL"
  else
    diff "$BASELINE" "$out" | head -20 | sed 's/^/    /'
    keep_line "sims" "SIMS_DIFFER (baseline $BASELINE; expected only after an intended rules change; --update-baseline accepts)"
    [ "$UPDATE_BASELINE" -eq 1 ] && cp "$out" "$BASELINE"
    record sims FAIL "SIMS_DIFFER"
  fi
}

# ---- web parts -------------------------------------------------------------------------------------------------
playwright_browsers_present() {  # $1 = chromium | firefox
  local base="${PLAYWRIGHT_BROWSERS_PATH:-}"
  if [ -z "$base" ]; then
    case "$(uname -s)" in Darwin) base="$HOME/Library/Caches/ms-playwright" ;; *) base="$HOME/.cache/ms-playwright" ;; esac
  fi
  ls -d "$base"/$1-* >/dev/null 2>&1
}

WEB_STATE=""; WEB_REASON=""
emsdk_env() {  # prints "PATH=... EM_CONFIG=..." assignments for env(1) when --emsdk / $EMSDK names a folder
  if [ -n "$EMSDK_DIR" ] && [ -d "$EMSDK_DIR" ]; then
    echo "PATH=$EMSDK_DIR:$EMSDK_DIR/upstream/emscripten:$PATH" "EM_CONFIG=$EMSDK_DIR/.emscripten"
  fi
}
ensure_web_build() {
  if [ -z "$WEB_STATE" ]; then
    # shellcheck disable=SC2046
    if ! have env || ! env $(emsdk_env) sh -c 'command -v emcmake' >/dev/null 2>&1; then
      WEB_STATE=skip; WEB_REASON="Emscripten not found (put emcmake on PATH, or pass --emsdk DIR / set EMSDK)"
    elif ( cd "$ROOT" && env $(emsdk_env) emcmake cmake -S . -B build-web $(gen_args "$ROOT/build-web") -DCMAKE_BUILD_TYPE=Release \
             && env $(emsdk_env) cmake --build build-web -j "$JOBS" ) > "$LOGS/build-web.log" 2>&1; then
      WEB_STATE=ok
    else
      WEB_STATE=fail; WEB_REASON="web build failed (see $LOGS/build-web.log)"
    fi
  fi
  [ "$WEB_STATE" = ok ]
}
web_unavailable() {
  if [ "$WEB_STATE" = skip ]; then record "$1" SKIPPED "$WEB_REASON"; else record "$1" FAIL "$WEB_REASON"; fi
}

part_webtest() {
  have node || { record webtest SKIPPED "node is not installed"; return; }
  [ -d "$ROOT/tools/webtest/node_modules" ] || { record webtest SKIPPED "tools/webtest/node_modules missing (cd tools/webtest && npm install)"; return; }
  playwright_browsers_present chromium || { record webtest SKIPPED "Playwright browsers missing (npx playwright install chromium firefox)"; return; }
  if ! ensure_web_build; then web_unavailable webtest; return; fi
  local rc1 rc2 l1 l2
  ( cd "$ROOT/tools/webtest" && node run.mjs --build ../../build-web/engine/player --out "$LOGS/webtest-out" ) > "$LOGS/webtest-chromium.log" 2>&1; rc1=$?
  l1="$(tail -1 "$LOGS/webtest-chromium.log")"; keep_line "webtest chromium" "$l1"
  rc2=0; l2="(Firefox not installed: not run)"
  if playwright_browsers_present firefox; then
    ( cd "$ROOT/tools/webtest" && node run.mjs --browser firefox --build ../../build-web/engine/player --out "$LOGS/webtest-out" ) > "$LOGS/webtest-firefox.log" 2>&1; rc2=$?
    l2="$(tail -1 "$LOGS/webtest-firefox.log")"; keep_line "webtest firefox" "$l2"
  else
    echo "  webtest firefox: $l2"
  fi
  if [ $rc1 -eq 0 ] && [ $rc2 -eq 0 ]; then record webtest PASS "$l1 | $l2"
  else record webtest FAIL "see $LOGS/webtest-*.log"; fi
}

WEBUI_STATE=""
web_node_ready() {  # part -> 0 ready, else records SKIPPED and returns 1
  have node && have npm || { record "$1" SKIPPED "node / npm is not installed"; return 1; }
  [ -d "$ROOT/editor/web/node_modules" ] || { record "$1" SKIPPED "editor/web/node_modules missing (cd editor/web && npm install)"; return 1; }
  return 0
}
ensure_web_ui() {  # tsc + vite build -> editor/web/dist (the helper serves it; e2e needs it)
  if [ -z "$WEBUI_STATE" ]; then
    if ( cd "$ROOT/editor/web" && npm run build ) > "$LOGS/web-ui-build.log" 2>&1; then WEBUI_STATE=ok; else WEBUI_STATE=fail; fi
  fi
  [ "$WEBUI_STATE" = ok ]
}

part_vitest() {
  web_node_ready vitest || return
  local errs rc tl
  ( cd "$ROOT/editor/web" && npm run typecheck ) > "$LOGS/typecheck.log" 2>&1
  errs="$(grep -cE "error" "$LOGS/typecheck.log")"
  keep_line "typecheck" "$errs error line(s)"
  ensure_web_ui; local uirc=$?
  ( cd "$ROOT/editor/web" && npm test ) > "$LOGS/vitest.log" 2>&1; rc=$?
  tl="$(grep -E "Tests " "$LOGS/vitest.log" | tail -1 | sed 's/^ *//')"
  keep_line "vitest" "$tl"
  if [ "$errs" -eq 0 ] && [ $uirc -eq 0 ] && [ $rc -eq 0 ]; then record vitest PASS "$tl"
  else record vitest FAIL "typecheck errors: $errs; ui build rc $uirc; vitest rc $rc ($LOGS/{typecheck,web-ui-build,vitest}.log)"; fi
}

part_e2e() {
  web_node_ready e2e || return
  playwright_browsers_present chromium || { record e2e SKIPPED "Playwright browsers missing (cd editor/web && npx playwright install chromium firefox)"; return; }
  if ! ensure_web_build; then web_unavailable e2e; return; fi
  if ! ensure_main_build; then main_unavailable e2e; return; fi
  if ! ensure_web_ui; then record e2e FAIL "the web UI did not build (see $LOGS/web-ui-build.log)"; return; fi
  local t0 t1 rc mins
  t0="$(now)"
  ( cd "$ROOT/editor/web" && OTE_EDITOR_BIN="$ROOT/build/ote-editor" OTE_PLAYER_DIR="$ROOT/build-web/engine/player" \
      OTE_SIM_BIN="$ROOT/build/ote-sim" npm run e2e ) > "$LOGS/e2e.log" 2>&1; rc=$?
  t1="$(now)"
  mins="$(awk -v s="$t0" -v e="$t1" 'BEGIN { printf "%.1f", (e - s) / 60 }')"
  grep -E "^\s+[0-9]+ (passed|failed|skipped|flaky)|^\s+\[(chromium|firefox)\]" "$LOGS/e2e.log" | cut -c1-200 | while IFS= read -r l; do
    echo "  e2e: $l"
  done
  local summary
  summary="$(grep -E "^\s+[0-9]+ passed" "$LOGS/e2e.log" | sed 's/^ *//' | tr '\n' ' ')"
  RESULT_LINES+=("e2e: ${summary:-no totals}" "e2e minutes: $mins")
  echo "  e2e minutes: $mins"
  if [ $rc -eq 0 ]; then record e2e PASS "${summary}(${mins} min)"
  else record e2e FAIL "playwright exit $rc; see $LOGS/e2e.log"; fi
}

# ---- run -------------------------------------------------------------------------------------------------------
echo "check_all: tree $ROOT  jobs $JOBS  logs $LOGS"
[ "$AUDIO_TEST" -eq 1 ] && echo "check_all: --with-audio-test given: the ctest entry audio_player WILL open the sound device"
T_START="$(now)"
selected sdl3 && part_sdl3
selected sdl2 && part_sdl2
selected editor-api && part_editor_api
selected ctest && part_ctest
selected sims && part_sims
selected webtest && part_webtest
selected vitest && part_vitest
selected e2e && part_e2e

echo
echo "================ check_all summary ($(awk -v s="$T_START" -v e="$(now)" 'BEGIN { printf "%.1f", (e - s) / 60 }') min) ================"
for r in "${RESULTS[@]+"${RESULTS[@]}"}"; do
  IFS='|' read -r part status detail <<< "$r"
  printf '%-8s %-11s %s\n' "$status" "$part" "$detail"
done
echo "---- result lines ----"
for l in "${RESULT_LINES[@]+"${RESULT_LINES[@]}"}"; do echo "$l"; done
if [ "$FAILED" -gt 0 ]; then echo "CHECK_ALL: FAIL ($FAILED part(s) failed, $SKIPPED skipped)"; exit 1; fi
if [ "$STRICT" -eq 1 ] && [ "$SKIPPED" -gt 0 ]; then echo "CHECK_ALL: FAIL (--strict: $SKIPPED part(s) skipped)"; exit 1; fi
echo "CHECK_ALL: PASS ($SKIPPED part(s) skipped)"
exit 0
