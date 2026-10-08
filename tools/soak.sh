#!/usr/bin/env bash
# Soak test: runs the game headless with random input (REUNION_AUTOPILOT
# random mode) for a range of seeds and reports crashes and hangs per seed.
# Crashes and hangs get a gdb backtrace in the log directory.
#
#   tools/soak.sh [-n SEEDS] [-s FIRST] [-f FRAMES] [-t SECONDS] [-j JOBS]
#                 [-b BUILD_DIR] [-d DATA_DIR] [-o LOG_DIR] [-p SCRIPT -r START]
#
# -p/-r: play an autopilot script first (e.g. into a new game) and start the
# random input at frame START.
# Build first: cmake --preset trap && cmake --build --preset trap
# (UBSan in trap mode: undefined behaviour stops with SIGILL).
# Replay one seed: REUNION_AUTOPILOT=random:SEED:FRAMES build/trap/reunion
# SOAK_VIDEO=dummy runs about twice as fast as the default offscreen driver.
# SOAK_MUSIC sets REUNION_MUSIC (mod/adlib); default: as SAVE\SETUP.SPD says.
# Exit status: 0 if every seed finished, 1 otherwise.
set -u

root=$(cd "$(dirname "$0")/.." && pwd)
seeds=20 first=1 frames=30000 limit=120 jobs=$(nproc)
build="$root/build/trap" data="$root/vendor/Reunion_DOS_EN" logs="" prefix="" start=0

usage() { sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
while getopts "n:s:f:t:j:b:d:o:p:r:h" opt; do
    case $opt in
    n) seeds=$OPTARG ;; s) first=$OPTARG ;; f) frames=$OPTARG ;; t) limit=$OPTARG ;;
    j) jobs=$OPTARG ;; b) build=$OPTARG ;; d) data=$OPTARG ;; o) logs=$OPTARG ;;
    p) prefix=$OPTARG ;; r) start=$OPTARG ;;
    *) usage ;;
    esac
done
exe="$build/reunion"
[[ -x $exe ]] || { echo "soak: $exe not built" >&2; exit 2; }
[[ -d $data ]] || { echo "soak: no game data in $data" >&2; exit 2; }
logs=${logs:-$(mktemp -d "${TMPDIR:-/tmp}/reunion-soak.XXXXXX")}
mkdir -p "$logs"

# One seed: prints "SEED STATUS DETAIL".
run_seed() {
    local seed=$1 log="$logs/$1.log" bt="$logs/$1.bt"
    local home="$logs/home-$seed"
    rm -rf "$home"; mkdir -p "$home"
    # The software renderer: parallel runs on the GPU renderer stall each other.
    local -a env=(SDL_VIDEO_DRIVER="${SOAK_VIDEO:-offscreen}" SDL_RENDER_DRIVER=software
                  SDL_AUDIO_DRIVER=dummy REUNION_MUSIC="${SOAK_MUSIC:-}" XDG_DATA_HOME="$home"
                  HOME="$home" REUNION_AUTOPILOT="$prefix;random:$seed:$frames:$start")
    env "${env[@]}" timeout -k 5 "$limit" "$exe" "$data" >"$log" 2>&1
    local rc=$? status detail=""
    if ((rc == 124 || rc == 137)); then
        status=HANG
        # Run again and take a backtrace where it is stuck.
        env "${env[@]}" "$exe" "$data" >/dev/null 2>&1 &
        local pid=$!
        sleep "$limit"
        gdb -q -batch -p "$pid" -ex 'set debuginfod enabled off' -ex 'thread apply all bt' >"$bt" 2>&1
        kill -9 "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
    elif ((rc > 128)); then
        status=CRASH detail="signal $((rc - 128))"
        env "${env[@]}" timeout -k 5 "$((limit * 4))" gdb -q -batch \
            -ex 'set debuginfod enabled off' -ex run -ex bt -ex 'x/i $pc' \
            --args "$exe" "$data" >"$bt" 2>&1
    elif grep -q 'Runtime error' "$log"; then
        status=RUNERROR detail=$(grep -m1 'Runtime error' "$log")
    elif grep -q 'random run finished' "$log"; then
        status=ok
    else
        status=EXIT detail="code $rc"
    fi
    if [[ -s $bt ]]; then
        local where
        where=$(grep -m1 -E '^#[0-9]+ .* at [^ ]+:[0-9]+' "$bt" | sed -E 's/.* in ([^ ]+) .* at ([^ ]+)$/\1 \2/')
        detail="$detail ${where:-}"
    fi
    local restarts
    restarts=$(grep -c 'quit at frame' "$log")
    printf '%6s %-8s restarts=%-3s %s\n' "$seed" "$status" "$restarts" "$detail"
}

if [[ ${SOAK_ONE:-} ]]; then
    run_seed "$SOAK_ONE"
    exit
fi

trap 'trap - TERM; kill 0' INT TERM
echo "soak: seeds $first..$((first + seeds - 1)), $frames frames, ${limit}s limit, logs in $logs"
results=$(seq "$first" $((first + seeds - 1)) |
    xargs -P "$jobs" -I{} env SOAK_ONE={} "$0" -b "$build" -d "$data" -f "$frames" -t "$limit" -o "$logs" \
        -p "$prefix" -r "$start" |
    sort -n)
echo "$results"
bad=$(grep -vc ' ok ' <<<"$results")
echo "soak: $((seeds - bad))/$seeds ok"
((bad == 0))
