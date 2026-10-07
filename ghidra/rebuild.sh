#!/usr/bin/env bash
# Recreates the Ghidra project from the original executable and applies
# everything recorded in ghidra/symbols.txt.
#
#   ghidra/rebuild.sh            full rebuild (import + analysis + symbols)
#   ghidra/rebuild.sh --apply    re-apply symbols.txt to the existing project
#
# GHIDRA_HOME defaults to ~/ghidra.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
ghidra=${GHIDRA_HOME:-$HOME/ghidra}
binary=$root/vendor/Reunion_DOS_EN/GRWAR/REUNION.PRG
project=$here/project
log=$project/headless.log

# (Re)install the language extension when it's missing or out of date.
props=$ghidra/Ghidra/application.properties
version=$(sed -n 's/^application\.version=//p' "$props")
release=$(sed -n 's/^application\.release\.name=//p' "$props")
ext=${XDG_CONFIG_HOME:-$HOME/.config}/ghidra/ghidra_${version}_${release}/Extensions/ReunionX86
if [[ ! -f $ext/data/languages/x86-pascal.sla ]] ||
   [[ -n $(find "$here/lang" -newer "$ext/data/languages/x86-pascal.sla" -type f) ]]; then
    "$here/lang/install.sh"
fi

mkdir -p "$project"
headless=("$ghidra/support/analyzeHeadless" "$project" reunion -scriptPath "$here/scripts")

if [[ ${1:-} == --apply ]]; then
    "${headless[@]}" -process REUNION.PRG -noanalysis \
        -postScript ApplySymbols.java "$here/symbols.txt" >"$log" 2>&1
else
    "${headless[@]}" -import "$binary" -overwrite \
        -processor "x86:LE:16:Borland Pascal Real Mode" -cspec pascal \
        -postScript ApplySymbols.java "$here/symbols.txt" >"$log" 2>&1
fi

grep -E 'ApplySymbols|symbols\.txt:|ERROR' "$log" || true
