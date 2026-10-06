#!/bin/bash
# =====================================================================================================================
# Musichien - host platform helpers, shared by the shell scripts.
#
# Sourced, never executed:
#
#     source "$(dirname "${BASH_SOURCE[0]}")/platform.sh"
#
# The BUILD of this project must not depend on the machine, but the scripts that INSTALL it do have to
# talk to the machine. This file is the single place that knows whether that machine is a Linux (Arch)
# or a macOS, and that hides the difference behind a few functions. Two consequences:
#
#   * no script needs a GNU-only tool: 'find -maxdepth' and 'sort -V' do not exist on macOS, and a Mac
#     user should not have to install coreutils to compare two Qt version numbers;
#   * adding a third host (Windows, one day) means adding a branch HERE, not editing every script.
# =====================================================================================================================

# ---------------------------------------------------------------------------------------------------------------------
# Host detection
# ---------------------------------------------------------------------------------------------------------------------
case "$(uname -s)" in
    Linux)  MUSICHIEN_HOST_OS="linux" ;;
    Darwin) MUSICHIEN_HOST_OS="macos" ;;
    *)      MUSICHIEN_HOST_OS="unknown" ;;
esac
export MUSICHIEN_HOST_OS

# ---------------------------------------------------------------------------------------------------------------------
# musichien_greatest_version <version>...
#
# Prints the greatest dotted version among its arguments. 'sort -V' would do it, but it is a GNU
# extension that macOS does not have. awk is present everywhere, so the comparison is written there.
# ---------------------------------------------------------------------------------------------------------------------
musichien_greatest_version()
{
    [ "$#" -gt 0 ] || return 0

    printf '%s\n' "$@" | awk '
        function compare(left, right,   leftCount, rightCount, i, leftField, rightField, fieldCount) {
            leftCount  = split(left,  leftParts,  ".")
            rightCount = split(right, rightParts, ".")
            fieldCount = (leftCount > rightCount) ? leftCount : rightCount
            for (i = 1; i <= fieldCount; i++) {
                leftField  = (i <= leftCount)  ? leftParts[i]  + 0 : 0
                rightField = (i <= rightCount) ? rightParts[i] + 0 : 0
                if (leftField != rightField) {
                    return (leftField < rightField) ? -1 : 1
                }
            }
            return 0
        }
        {
            if (best == "" || compare($0, best) > 0) {
                best = $0
            }
        }
        END { if (best != "") print best }
    '
}

# ---------------------------------------------------------------------------------------------------------------------
# musichien_newest_directory <glob>
#
# Prints the newest existing directory matching the glob, or nothing. The version is taken as the text
# after the LAST dash of the directory name (cmake-3.30.5 -> 3.30.5, Qt-6.8.0 -> 6.8.0, a bare NDK
# revision 27.2.12479018 -> itself), which is exactly how the pinned directories are named.
#
# Replaces 'find -maxdepth 1 | sort -V', neither of which macOS understands.
# ---------------------------------------------------------------------------------------------------------------------
musichien_newest_directory()
{
    local pattern="${1:-}"
    local entry name version bestDirectory bestName bestVersion newestVersion

    [ -n "${pattern}" ] || return 0

    bestDirectory=""
    bestName=""
    bestVersion=""

    # The pattern is deliberately unquoted: it holds a glob, and the loop is what expands it. When
    # nothing matches, bash leaves the literal pattern, and the '-d' test rejects it.
    for entry in ${pattern}; do
        [ -d "${entry}" ] || continue

        name="${entry##*/}"
        version="${name##*-}"

        if [ -z "${bestDirectory}" ]; then
            bestDirectory="${entry}"; bestName="${name}"; bestVersion="${version}"
            continue
        fi

        newestVersion="$(musichien_greatest_version "${version}" "${bestVersion}")"
        if [ "${newestVersion}" = "${version}" ]; then
            bestDirectory="${entry}"; bestName="${name}"; bestVersion="${version}"
        fi
    done

    printf '%s' "${bestDirectory}"
}

# ---------------------------------------------------------------------------------------------------------------------
# musichien_brew_prefix <formula>
#
# Prints the Homebrew prefix of a formula, or nothing. Homebrew is not everywhere, and some formulas
# are keg-only (llvm, openjdk@21): their prefix is the only way to find them, because they are not on
# the PATH.
# ---------------------------------------------------------------------------------------------------------------------
musichien_brew_prefix()
{
    command -v brew >/dev/null 2>&1 || return 0
    brew --prefix "${1:-}" 2>/dev/null || true
}

# ---------------------------------------------------------------------------------------------------------------------
# musichien_jdk_home
#
# Prints the JAVA_HOME of the pinned JDK 21, the one the Android toolchain of Qt expects. A newer JDK
# (26, installed by default on the reference machine) is too recent for Gradle and AGP: the build then
# fails with obscure class-file-version messages.
# ---------------------------------------------------------------------------------------------------------------------
musichien_jdk_home()
{
    local prefix

    if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
        prefix="$(musichien_brew_prefix openjdk@21)"
        if [ -n "${prefix}" ] && [ -d "${prefix}/libexec/openjdk.jdk/Contents/Home" ]; then
            printf '%s' "${prefix}/libexec/openjdk.jdk/Contents/Home"
        fi
        return 0
    fi

    printf '%s' "/usr/lib/jvm/java-21-openjdk"
}

# ---------------------------------------------------------------------------------------------------------------------
# musichien_install_packages <package>...
#
# Installs packages with the host package manager. Nothing is installed when the list is empty, which
# is the common case on a machine that is already set up.
# ---------------------------------------------------------------------------------------------------------------------
musichien_install_packages()
{
    [ "$#" -gt 0 ] || return 0

    if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
        brew install "$@"
    else
        sudo pacman -S --needed --noconfirm "$@"
    fi
}

# ---------------------------------------------------------------------------------------------------------------------
# musichien_qt_desktop_subdir <versionDirectory>
#
# Prints the desktop Qt installation inside a Qt version directory, or nothing. The sub directory has
# a different name per host and per packaging:
#
#     gcc_64     aqtinstall on Linux
#     macos      aqtinstall on macOS
#     clang_64   the flat layout used by the other personal projects
#
# The first one that holds a real Qt6 installation wins.
# ---------------------------------------------------------------------------------------------------------------------
musichien_qt_desktop_subdir()
{
    local versionDirectory="${1:-}"
    local candidate

    for candidate in gcc_64 macos clang_64; do
        if [ -f "${versionDirectory}/${candidate}/lib/cmake/Qt6/Qt6Config.cmake" ]; then
            printf '%s' "${versionDirectory}/${candidate}"
            return 0
        fi
    done
}
