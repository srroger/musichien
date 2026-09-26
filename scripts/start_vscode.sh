#!/bin/bash
# =====================================================================================================================
# Musichien - launches VS Code with the project environment already set up.
#
#   Usage:  scripts/start_vscode.sh
#
# Same reasoning as scripts/start_qtcreator.sh: the environment is set BEFORE the editor starts, so
# that the CMake Tools extension can read the presets and pick the pinned Qt.
# =====================================================================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=setup_env.sh
source "${SCRIPT_DIR}/setup_env.sh"

echo "Opening VS Code with the Musichien environment..."

code "${MUSICHIEN_PROJECT_DIR}" &
