#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
PROJECT_NAME="opengl-sim"

usage() {
    cat <<'EOF'
Usage:
  ./scripts/manage.sh build [--single]
  ./scripts/manage.sh run [--single]
  ./scripts/manage.sh test
  ./scripts/manage.sh clean

Commands:
  build    Configure and compile the project.
  run      Build and run the project.
  test     Build and run the unit tests.
  clean    Remove the build directory.

Options:
  --single Also copy the built executable into the project root (build/run only).
EOF
}

copy_single="false"
command_name="${1:-}"

if [[ -z "$command_name" ]]; then
    usage >&2
    exit 2
fi
shift

while (($# > 0)); do
    case "$1" in
        --single)
            if [[ "$command_name" == "clean" ]]; then
                echo "Error: --single cannot be used with clean." >&2
                exit 2
            fi
            copy_single="true"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Error: unknown option '$1'." >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

case "$command_name" in
    build|run|test)
        cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
        cmake --build "$BUILD_DIR" --config Release --parallel

        executable_name="$PROJECT_NAME"
        case "$(uname -s)" in
            MINGW*|MSYS*|CYGWIN*) executable_name+=".exe" ;;
        esac
        executable="$BUILD_DIR/$executable_name"

        if [[ ! -f "$executable" ]]; then
            echo "Error: expected executable was not created at $executable." >&2
            exit 1
        fi

        if [[ "$copy_single" == "true" ]]; then
            cp "$executable" "$ROOT_DIR/$executable_name"
            echo "Executable copied to $ROOT_DIR/$executable_name"
        fi

        if [[ "$command_name" == "test" ]]; then
            ctest --test-dir "$BUILD_DIR" --output-on-failure
            exit $?
        fi

        if [[ "$command_name" == "run" ]]; then
            (cd "$ROOT_DIR" && "$executable")
        fi
        ;;
    clean)
        rm -rf -- "$BUILD_DIR"
        echo "Removed $BUILD_DIR"
        ;;
    -h|--help)
        usage
        ;;
    *)
        echo "Error: unknown command '$command_name'." >&2
        usage >&2
        exit 2
        ;;
esac
