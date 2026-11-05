#!/bin/sh
# The PS2 changes of the submodules, kept as patches (the submodules stay at
# upstream's commits): ps2/patches/<name>.patch for <name>.
#
#   ps2/patches.sh apply     applies each patch, unless it is applied already
#   ps2/patches.sh export    writes each submodule's changes (git diff) to its
#                            patch, after editing them
set -e
cd "$(dirname "$0")/.."
SUBMODULES="jfbuild jfaudiolib jfmact"

case "$1" in
apply)
	for s in $SUBMODULES; do
		p="$PWD/ps2/patches/$s.patch"
		[ -s "$p" ] || continue
		if git -C "$s" apply --reverse --check "$p" 2>/dev/null; then
			continue # applied
		fi
		git -C "$s" apply "$p"
		echo "patches.sh: applied $s.patch"
	done ;;
export)
	for s in $SUBMODULES; do
		git -C "$s" diff > "ps2/patches/$s.patch"
		[ -s "ps2/patches/$s.patch" ] || rm -f "ps2/patches/$s.patch"
	done ;;
*)
	echo "usage: $0 apply|export" >&2
	exit 64 ;;
esac
