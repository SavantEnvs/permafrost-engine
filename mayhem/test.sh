#!/usr/bin/env bash
#
# mayhem/test.sh — known-answer tests for the engine's asset parsers (probe built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# pf_probe is the engine's real parsers (PFOBJ, PFMAP, stb_image, attribute lines) built with the
# normal release flags by mayhem/build.sh. Each case below feeds it an asset and compares the parsed
# values with answers taken independently of the engine (the asset's own text, a hand-computed
# height, a separate PNG decoder). A missing probe or fixture is a failure, never a skip.
PROBE="$SRC/pf_probe"
passed=0; failed=0

run_probe() { "$PROBE" "$@" 2>/dev/null; }

# expect <name> <probe output> <exact line that must appear>
expect() {
  if printf '%s\n' "$2" | grep -Fxq -- "$3"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1)); echo "FAIL $1: expected line '$3'" >&2
  fi
}

# expect_prefix <name> <probe output> <prefix of a line that must appear>
expect_prefix() {
  if printf '%s\n' "$2" | grep -Fq -- "$3"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1)); echo "FAIL $1: expected a line starting '$3'" >&2
  fi
}

if [ ! -x "$PROBE" ]; then
  echo "FAIL: $PROBE missing — mayhem/build.sh did not produce it" >&2
  emit_ctrf "pf_probe-kat" 0 1 0
  exit $?
fi

# --- PFOBJ: arrow-green.pfobj ------------------------------------------------------------------
OBJ=assets/models/arrow/arrow-green.pfobj
out=$(run_probe pfobj "$OBJ"); rc=$?
[ "$rc" -eq 0 ] && passed=$((passed + 1)) || { failed=$((failed + 1)); echo "FAIL pfobj: rc=$rc" >&2; }
nverts=$(grep -c '^v ' "$OBJ")
expect pfobj-header   "$out" "pfobj header verts=$nverts joints=3 materials=1 anim_sets=1 collision=1"
expect pfobj-render   "$out" "pfobj render verts=$nverts materials=1 vbuff_bytes=31824"
expect pfobj-material "$out" "pfobj material 0 texture=green.jpg ambient=1.000 diffuse=0.640,0.640,0.640"
expect pfobj-skeleton "$out" "pfobj skeleton joints=3 clips=1"
# joint names and parents come from the file's own "j <parent+1> <name> ..." lines
i=0
while read -r parent name; do
  expect_prefix "pfobj-joint-$i" "$out" "pfobj joint $i name=$name parent=$((parent - 1)) "
  i=$((i + 1))
done < <(awk '$1 == "j" { print $2, $3 }' "$OBJ")
[ "$i" -eq 3 ] && passed=$((passed + 1)) || { failed=$((failed + 1)); echo "FAIL pfobj joint lines: $i" >&2; }
expect pfobj-joint-trans0 "$out" "pfobj joint 0 name=arrow3 parent=-1 scale=1.000,1.000,1.000 trans=1.483,0.905,-2.423"
expect pfobj-joint-trans2 "$out" "pfobj joint 2 name=arrow1 parent=-1 scale=1.000,1.000,1.000 trans=-2.839,0.912,0.006"
expect pfobj-clip     "$out" "pfobj clip 0 name=Converge frames=24 frame0_aabb=-3.203,3.842,0.585,1.231,-3.839,2.978"
expect pfobj-aabb     "$out" "pfobj aabb=-3.242,3.894,0.584,1.232,-3.890,3.016"

# --- PFMAP: two raised/sunk tiles on a flat 1x1-chunk map ---------------------------------------
# Tile base height is in steps of Y_COORDS_PER_TILE (4): +5 -> 20, -3 -> -12; every other tile is 0.
out=$(run_probe pfmap mayhem/testdata/two_bumps.pfmap); rc=$?
[ "$rc" -eq 0 ] && passed=$((passed + 1)) || { failed=$((failed + 1)); echo "FAIL pfmap: rc=$rc" >&2; }
expect pfmap-size       "$out" "pfmap chunks=1x1 pos=0.0,0.0,0.0"
expect pfmap-raised     "$out" "pfmap height(row=3,col=5)=20.0000"
expect pfmap-sunk       "$out" "pfmap height(row=20,col=10)=-12.0000"
expect pfmap-flat       "$out" "pfmap height(row=0,col=0)=0.0000"
expect pfmap-flat-far   "$out" "pfmap height(row=31,col=31)=0.0000"
expect pfmap-neighbor-a "$out" "pfmap height(row=3,col=6)=0.0000"
expect pfmap-neighbor-b "$out" "pfmap height(row=4,col=5)=0.0000"

out=$(run_probe pfmap assets/maps/test.pfmap)
expect pfmap-shipped-size "$out" "pfmap chunks=4x4 pos=0.0,0.0,0.0"

# --- stb_image: folder-icon.png (200x154 RGBA); checksums from an independent PNG decoder -----
out=$(run_probe image assets/icons/folder-icon.png); rc=$?
[ "$rc" -eq 0 ] && passed=$((passed + 1)) || { failed=$((failed + 1)); echo "FAIL image: rc=$rc" >&2; }
expect image-size   "$out" "image size=200x154 channels=4 sum=22743675 fnv=7d3ec0370e557a98"
expect image-pixel  "$out" "image first_pixel=255,255,255,0"

# --- attribute lines -----------------------------------------------------------------------------
expect attr-vec3   "$(run_probe attr 'vec3 1.5 2 -3')"          "attr value=1.5000,2.0000,-3.0000"
expect attr-named  "$(run_probe attr 'mykey string hello world')" "attr value=hello world"
expect attr-key    "$(run_probe attr 'mykey string hello world')" "attr key=mykey type=0"
expect attr-quat   "$(run_probe attr 'quat 0 0 0.5 1')"          "attr value=0.0000,0.0000,0.5000,1.0000"
expect attr-int    "$(run_probe attr 'int -42')"                  "attr value=-42"
expect attr-float  "$(run_probe attr 'float 3.25')"               "attr value=3.2500"
expect attr-bool   "$(run_probe attr 'bool 1')"                   "attr value=1"
expect attr-reject "$(run_probe attr 'bool 7')"                   "attr parse=failed"
expect attr-vec2   "$(run_probe attr 'pos vec2 1 2')"             "attr value=1.0000,2.0000"

emit_ctrf "pf_probe-kat" "$passed" "$failed" 0
