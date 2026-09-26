# =====================================================================================================================
# Pinned versions of the external dependencies
#
# This file is the single place where a version number is written. It is shared by:
#   * the superbuild configuration, which downloads and builds the dependency
#   * the normal configuration, which looks the dependency up in MUSICHIEN_EXTERNAL_DIR
#
# Rule: a version is only ever bumped here, in a dedicated commit, never by accident.
# =====================================================================================================================

set(MUSICHIEN_GOOGLETEST_VERSION "1.18.0")
set(MUSICHIEN_GOOGLETEST_GIT_TAG "v${MUSICHIEN_GOOGLETEST_VERSION}")

set(MUSICHIEN_NLOHMANN_JSON_VERSION "3.12.0")
set(MUSICHIEN_NLOHMANN_JSON_GIT_TAG "v${MUSICHIEN_NLOHMANN_JSON_VERSION}")


# Version of Qt this project is developed and tested against.
#
# Deliberately left EMPTY: the exact version is installed by scripts/install_dependencies.sh, which
# asks the Qt download mirror what is available. Hard coding a version here would be a guess, and a
# wrong guess breaks the build in a confusing way.
#
# Once the version is installed, it is recorded in docs/BUILD_AND_SETUP.md and exported by
# scripts/setup_env.sh through MUSICHIEN_QT_DIR.
set(MUSICHIEN_QT_TAG "")

