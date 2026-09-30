#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./clang_tidy.sh

Apply .clang-tidy fixes to the macOS tests and game entry points, covering
Metal, Vulkan/MoltenVK, and Vulkan/KosmicKrisp.
Shared headers are processed through these entry points, sequentially.

  -h, --help    Show this help.

CLANG_TIDY can select a clang-tidy executable. VULKAN_INCLUDE_DIR can select
the include directory containing vulkan/vulkan.h. Otherwise, LLVM and Vulkan
headers are located using PATH, VULKAN_SDK, and common Homebrew locations.
Logs are saved under bin/clang-tidy/; compiler errors produce a nonzero exit.
EOF
}

case "${1:-}" in
    '') ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac
if [ "$#" -ne 0 ]; then
    usage >&2
    exit 2
fi

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$project_root"

if [ "$(uname -s)" != Darwin ]; then
    echo "This script targets the macOS builds; Win32 requires its own SDK and compiler flags." >&2
    exit 1
fi

if [ -n "${CLANG_TIDY:-}" ]; then
    tidy_executable="$(command -v "$CLANG_TIDY")" || {
        echo "CLANG_TIDY executable not found: $CLANG_TIDY" >&2
        exit 1
    }
elif command -v clang-tidy >/dev/null 2>&1; then
    tidy_executable="$(command -v clang-tidy)"
else
    tidy_executable=''
    for candidate in /opt/homebrew/opt/llvm/bin/clang-tidy /usr/local/opt/llvm/bin/clang-tidy; do
        if [ -x "$candidate" ]; then
            tidy_executable="$candidate"
            break
        fi
    done
    if [ -z "$tidy_executable" ]; then
        echo "clang-tidy was not found. Install LLVM or set CLANG_TIDY to its executable." >&2
        exit 1
    fi
fi

vulkan_include_dir="${VULKAN_INCLUDE_DIR:-}"
if [ -z "$vulkan_include_dir" ]; then
    vulkan_candidates=()
    if [ -n "${VULKAN_SDK:-}" ]; then
        vulkan_candidates+=("$VULKAN_SDK/include" "$VULKAN_SDK/macOS/include")
    fi
    vulkan_candidates+=(/usr/local/include /opt/homebrew/include)
    for candidate in "${vulkan_candidates[@]}"; do
        if [ -f "$candidate/vulkan/vulkan.h" ]; then
            vulkan_include_dir="$candidate"
            break
        fi
    done
fi
if [ ! -f "$vulkan_include_dir/vulkan/vulkan.h" ]; then
    echo "Vulkan headers not found. Set VULKAN_INCLUDE_DIR to the directory containing vulkan/vulkan.h." >&2
    exit 1
fi

sdk_path="$(xcrun --show-sdk-path)"
compiler_flags=(-x objective-c -std=c11 -Isrc -I "$vulkan_include_dir" -isysroot "$sdk_path")
log_dir="$project_root/bin/clang-tidy"
mkdir -p "$log_dir"
failures=0

run_tidy() {
    local name="$1" source="$2"
    shift 2
    local log_file="$log_dir/fix-$name.log"
    local tidy_flags=(--fix --config-file="$project_root/.clang-tidy" --format-style=none)

    printf 'fix: %s\n' "$name"
    if "$tidy_executable" "${tidy_flags[@]}" "$source" -- "${compiler_flags[@]}" "$@" >"$log_file" 2>&1; then
        printf '  Completed; diagnostics: %s\n' "$log_file"
    else
        failures=$((failures + 1))
        printf '  FAILED; diagnostics: %s\n' "$log_file" >&2
        sed -n '1,80p' "$log_file" >&2
    fi
}

run_variants() {
    local entry name
    run_tidy tests src/test.c
    for entry in src/main.c src/gpu/gpu_test.c; do
        if [ "$entry" = src/main.c ]; then
            name=main
        else
            name=gpu-test
        fi
        run_tidy "$name-metal" "$entry" -DGPU_IMPLEMENTATION_METAL
        run_tidy "$name-vulkan-molten" "$entry" \
            -DGPU_IMPLEMENTATION_VULKAN -DMAC_VULKAN_BACKEND_MOLTEN -DVK_NEEDS_PORTABILITY_EXTENSIONS
        run_tidy "$name-vulkan-kosmic" "$entry" \
            -DGPU_IMPLEMENTATION_VULKAN -DMAC_VULKAN_BACKEND_KOSMIC
    done
}

echo "Using $tidy_executable"
echo "Coverage: macOS entry points and included headers; excludes Win32 and the unused collision_render.h."
# Never run fix jobs concurrently: their entry points share source headers.
run_variants

if [ "$failures" -ne 0 ]; then
    printf '\n%d invocation(s) failed. Review %s before running again.\n' "$failures" "$log_dir" >&2
    exit 1
fi

printf '\nPass completed. Remaining warnings are recorded in %s.\n' "$log_dir"
