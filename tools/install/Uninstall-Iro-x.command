#!/bin/bash
# Removes Iro-x (Audio Unit and VST3) from this Mac. Double-click to run; asks for the Mac password once.
# It deletes only these four items, if they exist:
AU="/Library/Audio/Plug-Ins/Components/Iro-x.component"
VST3="/Library/Audio/Plug-Ins/VST3/Iro-x.vst3"
AU_USER="$HOME/Library/Audio/Plug-Ins/Components/Iro-x.component"
VST3_USER="$HOME/Library/Audio/Plug-Ins/VST3/Iro-x.vst3"

echo "This will remove, if present:"
for P in "$AU" "$VST3" "$AU_USER" "$VST3_USER"; do [ -e "$P" ] && echo "  $P"; done
echo
read -n 1 -s -r -p "Press y to remove, any other key to cancel: " ANS
echo
if [ "$ANS" != "y" ]; then echo "Cancelled. Nothing was removed."; read -n 1 -s -r -p "Press any key to close."; exit 0; fi

sudo rm -rf "$AU" "$VST3"
rm -rf "$AU_USER" "$VST3_USER"
killall -9 AudioComponentRegistrar 2>/dev/null
echo "Removed. In Logic: Settings > Plug-in Manager > Reset & Rescan Selection."
read -n 1 -s -r -p "Press any key to close."
