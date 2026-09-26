#!/bin/bash
# =====================================================================================================================
# Musichien - launches code-oss (the open source build of VS Code) with the environment set up.
#
#   Usage:  scripts/start_code_oss.sh
#
# This is the ONLY supported way to open the project.
#
# Launching the editor from the application menu would leave CLANG_DIR, MUSICHIEN_QT_DIR,
# MUSICHIEN_EXTERNAL_DIR and the pinned CMake/Ninja out of the environment. Consequences:
#   * the CMake presets could not be read at all, because they reference those variables;
#   * the build would silently use the Qt installed on the machine instead of the pinned one;
#   * the clangd language server would not find the pinned Qt headers and would report false errors.
# =====================================================================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=setup_env.sh
source "${SCRIPT_DIR}/setup_env.sh"

# The editor to launch. code-oss is the open source build packaged on Arch; 'code' is the fallback
# name used by other distributions and by the official Microsoft build.
EDITOR_EXECUTABLE=""
for candidate in code-oss code; do
    if command -v "${candidate}" >/dev/null 2>&1; then
        EDITOR_EXECUTABLE="${candidate}"
        break
    fi
done

if [ -z "${EDITOR_EXECUTABLE}" ]; then
    echo "Musichien: neither 'code-oss' nor 'code' was found on the PATH."
    exit 1
fi

echo "Opening ${EDITOR_EXECUTABLE} with the Musichien environment..."

# The build directory is created if needed so that clangd finds compile_commands.json immediately.
mkdir -p "${MUSICHIEN_PROJECT_DIR}/../Musichien-build/Clang-Debug"

"${EDITOR_EXECUTABLE}" "${MUSICHIEN_PROJECT_DIR}" &
