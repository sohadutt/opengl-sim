#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

XCODE_DEVELOPER_DIR="/Applications/Xcode.app/Contents/Developer"
if [[ -d "$XCODE_DEVELOPER_DIR" && ( -z "${DEVELOPER_DIR:-}" || "$DEVELOPER_DIR" == "/Library/Developer/CommandLineTools" ) ]]; then
  export DEVELOPER_DIR="$XCODE_DEVELOPER_DIR"
fi

action="${1:-build}"
if [[ "$action" == "build" || "$action" == "release" || "$action" == "run" ]] && ! xcrun metal -version >/dev/null 2>&1; then
  cat >&2 <<'EOF'
Error: Apple's Metal shader compiler is unavailable in the selected developer tools.
Select the full Xcode installation and install the Metal Toolchain component:

  sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
  xcodebuild -downloadComponent MetalToolchain

The Command Line Tools package alone does not include the Metal compiler. The Xcode selection command asks for your macOS administrator password.
EOF
  exit 1
fi

case "$action" in
  -h|--help)
    echo "Usage: ./scripts/manage.sh {build|release|run|clean} (standalone Swift/Metal app)"
    ;;
  build)
    xcrun swift build
    ;;
  release)
    xcrun swift build -c release
    ;;
  run)
    xcrun swift run opengl-sim
    ;;
  clean)
    xcrun swift package clean
    ;;
  *)
    echo "Usage: $0 {build|release|run|clean} (standalone Swift/Metal app)" >&2
    exit 2
    ;;
esac
