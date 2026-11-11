#!/bin/bash
# Installs Iro-x: the Audio Unit (for Logic) and the VST3, for all users.
# Leave this file in the unzipped plugin folder, beside Iro-x.component and Iro-x.vst3, and double-click it.
# Asks for the Mac password once. It touches only the Iro-x files next to this file, the same-named copies in
# /Library/Audio/Plug-Ins/Components and /Library/Audio/Plug-Ins/VST3, and old same-named copies in ~/Library/Audio/Plug-Ins.
cd "$(dirname "$0")" || exit 1
SRC="$(pwd)"
AU_DEST="/Library/Audio/Plug-Ins/Components"
VST3_DEST="/Library/Audio/Plug-Ins/VST3"
AU_OLD="$HOME/Library/Audio/Plug-Ins/Components"
VST3_OLD="$HOME/Library/Audio/Plug-Ins/VST3"

shopt -s nullglob
AUS=("$SRC"/Iro-x.component)
VST3S=("$SRC"/Iro-x.vst3)

if [ ${#AUS[@]} -eq 0 ] && [ ${#VST3S[@]} -eq 0 ]; then
  echo "Cannot find Iro-x.component or Iro-x.vst3 next to this file."
  echo "Leave this file in the unzipped plugin folder."
  read -n 1 -s -r -p "Press any key to close."
  exit 1
fi

echo "Found:"
for C in "${AUS[@]}" "${VST3S[@]}"; do echo "  $(basename "$C")"; done
echo
xattr -dr com.apple.quarantine "$SRC" 2>/dev/null
echo "Your Mac password is needed to write to /Library/Audio/Plug-Ins (nothing shows while you type)."
sudo mkdir -p "$AU_DEST" "$VST3_DEST" || { read -n 1 -s -r -p "Password failed. Press any key to close."; exit 1; }

FAILED=0
install_one() {  # $1 = bundle, $2 = destination folder, $3 = old per-user folder
  local NAME
  NAME="$(basename "$1")"
  rm -rf "$3/$NAME"
  sudo rm -rf "$2/$NAME"
  sudo cp -R "$1" "$2/"
  if [ -d "$2/$NAME" ]; then
    echo "Installed: $2/$NAME"
  else
    echo "Install FAILED: $NAME is not in $2"
    FAILED=1
  fi
}
for C in "${AUS[@]}"; do install_one "$C" "$AU_DEST" "$AU_OLD"; done
for C in "${VST3S[@]}"; do install_one "$C" "$VST3_DEST" "$VST3_OLD"; done
killall -9 AudioComponentRegistrar 2>/dev/null

echo
if [ $FAILED -eq 0 ]; then
  echo "Done. Open Logic. If Iro-x does not appear: Settings > Plug-in Manager > Reset & Rescan Selection."
fi
echo
read -n 1 -s -r -p "Press any key to close."
