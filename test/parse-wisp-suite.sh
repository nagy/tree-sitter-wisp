#!/usr/bin/env sh
# Parse every vendored wisp reference test file; fail on any ERROR node.
# The suite is expat-licensed and vendored from hg.sr.ht/~arnebab/wisp.
set -eu
cd "$(dirname "$0")/.."
fail=0
for f in test/wisp-suite/*.w; do
  out=$(tree-sitter parse "$f" 2>&1) || true
  case "$out" in
    *ERROR*)
      echo "ERROR nodes in $f"
      echo "$out" | sed -n '/ERROR/p' | head -3
      fail=1
      ;;
  esac
done
if [ "$fail" -ne 0 ]; then
  exit 1
fi
echo "All wisp-suite files parse without ERROR nodes."
