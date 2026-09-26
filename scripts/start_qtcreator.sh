#!/bin/bash
# =====================================================================================================================
# Musichien - launches Qt Creator with the project environment already set up.
#
#   Usage:  scripts/start_qtcreator.sh
#
# This is the ONLY supported way to open the project. Launching Qt Creator from the application menu
# would leave CLANG_DIR, MUSICHIEN_QT_DIR and MUSICHIEN_EXTERNAL_DIR unset: the presets could not be
# read, and the build would silently use the Qt installed on the machine.
# =====================================================================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=setup_env.sh
source "${SCRIPT_DIR}/setup_env.sh"

echo "Opening Qt Creator with the Musichien environment..."

# The build directory is created if needed so that Qt Creator finds the presets immediately.
mkdir -p "${MUSICHIEN_PROJECT_DIR}/../Musichien-build"

qtcreator "${MUSICHIEN_PROJECT_DIR}/CMakeLists.txt" &
