#!/usr/bin/env bash
# Stage ReactorDrone.app: exe + brew SDL3 dylibs (rewritten to @executable_path
# by dylibbundler) + assets in Contents/Resources, where SDL_GetBasePath()
# points for a bundled app. Ad-hoc signed: arm64 refuses to run unsigned code
# at all, and "-" needs no certificate. Build first:
#   cmake -B CPP/build-portable -S CPP -DRD_PORTABLE=ON -DBUILD_TESTING=OFF
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/installer/ReactorDrone.app"
BIN="$ROOT/CPP/build-portable/game/game"
[ -x "$BIN" ] || { echo "build first: cmake -B CPP/build-portable -S CPP -DRD_PORTABLE=ON"; exit 1; }
VER="${1:-2.0.0}"

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
cp "$BIN" "$APP/Contents/MacOS/ReactorDrone"
cp -r "$ROOT/assets" "$APP/Contents/Resources/assets"
cp "$ROOT/PRIVACY.md" "$APP/Contents/Resources/"

cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleExecutable</key><string>ReactorDrone</string>
  <key>CFBundleIdentifier</key><string>com.conradm.reactordrone</string>
  <key>CFBundleName</key><string>Reactor Drone</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VER</string>
  <key>CFBundleVersion</key><string>$VER</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
</dict></plist>
EOF

# Pull every non-system dylib (SDL3 stack from brew) into Contents/Frameworks
# and rewrite the load commands. -b already recurses into inter-dylib refs;
# there is no -f flag (dylibbundler 1.0.x rejects it with "Unknown flag -f").
dylibbundler -od -b \
  -x "$APP/Contents/MacOS/ReactorDrone" \
  -d "$APP/Contents/Frameworks" \
  -p '@executable_path/../Frameworks' > /dev/null

codesign --force --deep -s - "$APP"
echo "staged: $(du -sh "$APP" | cut -f1) at $APP"
