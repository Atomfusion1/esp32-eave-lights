#!/bin/bash
# Remakes the README screenshots (Docs/images/*.png) from the current web_page.h with sample
# data: a made-up house (house.html), a sample layout (layout.js), Christmas colors (frame.js)
# and fake board replies (mock.js). Nothing here is the real house. Run from the repo root in
# Git Bash on Windows (uses headless Edge).
set -e
D="$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)"
OUT="$D/.."
E="/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"
T="$(mktemp -d)"
"$E" --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=1 --window-size=1600,900 --screenshot="$(cd "$T" && pwd -W)/house.png" "file:///$D/house.html"
awk '/R"rawhtml\(/{sub(/.*R"rawhtml\(/,""); p=1} p&&/\)rawhtml"/{sub(/\)rawhtml".*/,""); print; exit} p{print}' web_page.h > "$T/body.html"
{ echo "<script>"; cat "$D/layout.js" "$D/frame.js" "$D/mock.js"; echo "</script>"; cat "$T/body.html"; } > "$T/app.html"
TW="$(cd "$T" && pwd -W)"
"$E" --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=2 --window-size=960,760 --virtual-time-budget=5000 --screenshot="$OUT/hero.png" "file:///$TW/app.html#home"
for t in home lights setup; do
  "$E" --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=2 --window-size=520,1120 --virtual-time-budget=5000 --screenshot="$OUT/$t.png" "file:///$TW/app.html#$t"
done
rm -rf "$T"
