#!/bin/bash
# =====================================================================================================================
# Musichien - checks the code without modifying it.
#
#   Usage:  scripts/check_code.sh
#
# Exits with a non zero code when something does not comply, which makes it usable as a pre-commit
# hook or a CI step.
#
#   C++  -> clang-format --dry-run  (formatting)
#        -> clang-tidy              (naming rules, m_ / p_, const correctness, modernisation)
#   QML  -> qmllint                (static analysis)
# =====================================================================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${PROJECT_DIR}"

FAILURE_COUNT=0

# ---------------------------------------------------------------------------------------------------------------------
# C++ formatting
# ---------------------------------------------------------------------------------------------------------------------
echo "--- clang-format --dry-run --------------------------------------------------------------------"

mapfile -d '' CPP_FILES < <(find source -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0)

if [ ${#CPP_FILES[@]} -gt 0 ]; then
    if clang-format --style=file --dry-run -Werror "${CPP_FILES[@]}" 2>&1; then
        echo "  OK - every C++ file is formatted"
    else
        echo "  FAILED - run scripts/format_code.sh"
        FAILURE_COUNT=$((FAILURE_COUNT + 1))
    fi
fi

# ---------------------------------------------------------------------------------------------------------------------
# QML static analysis
# ---------------------------------------------------------------------------------------------------------------------
echo "--- qmllint -----------------------------------------------------------------------------------"

QML_LINT="$(command -v qmllint || echo "")"

if [ -z "${QML_LINT}" ]; then
    echo "  qmllint not found: skipped"
else

    mapfile -d '' QML_FILES < <(find source -type f -name '*.qml' -print0)

    if [ ${#QML_FILES[@]} -gt 0 ]; then
        if "${QML_LINT}" "${QML_FILES[@]}" 2>&1; then
            echo "  OK"
        else
            echo "  FAILED - see the findings above"
            FAILURE_COUNT=$((FAILURE_COUNT + 1))
        fi
    fi

fi

# ---------------------------------------------------------------------------------------------------------------------
# QML: a comment that swallows the line after it
#
# THE ONE TRAP NEITHER qmlformat NOR qmllint REPORTS. When qmlformat - or a careless edit - joins a comment and the code
# that followed it onto ONE line, everything from the first '//' onwards becomes a comment: the object silently stops
# existing. No tool complains. qmllint sees a comment, and so does the compiler, so the check passes and the bug ships.
#
# Both costs were paid for real: a course card that invited the player to sing and had NO BUTTON to press, and an
# exercise screen that showed the singing interface on EVERY question because its 'visible:' guard had been swallowed.
#
# The signature is simple: a QML line that is long AND carries a comment. A long binding line has no '//' in it, and a
# commented line is short. Both at once is always the accident.
# ---------------------------------------------------------------------------------------------------------------------
echo "--- QML: collapsed comment lines --------------------------------------------------------------"

COLLAPSED="$(find source -type f -name '*.qml' -exec awk 'length($0) > 200 && /\/\// { printf "%s:%d (%d chars)\n", FILENAME, NR, length($0) }' {} +)"

if [ -z "${COLLAPSED}" ]; then
    echo "  OK - no comment swallows the code after it"
else
    printf '%s\n' "${COLLAPSED}"
    echo "  FAILED - a comment and code share one line: everything after '//' is lost"
    FAILURE_COUNT=$((FAILURE_COUNT + 1))
fi

# ---------------------------------------------------------------------------------------------------------------------
# JSON: a content file that does not parse costs EVERYTHING, not just the line that broke it
#
# Every content bank is read with nlohmann::json::parse(..., allow_exceptions = false), and a discarded document makes
# the reader return an EMPTY book. So one stray character - a '//' comment, a trailing comma, a missing quote - does not
# cost the line it sits on: it costs the WHOLE file.
#
# It happened for real: six anecdotes were added with two '//' comment lines, the JSON stopped parsing, and the 885
# other anecdotes would have gone with them on every fresh build.
#
# NOTHING ELSE CATCHES IT. The C++ compiles, the tests pass (they read their own fixtures), and the application merely
# prints how many entries it read - a number that is easy not to look at. JSON has no comments, and no tool here was
# saying so.
# ---------------------------------------------------------------------------------------------------------------------
echo "--- JSON: content files parse ----------------------------------------------------------------"

if command -v python3 >/dev/null 2>&1; then

    JSON_FAILURES="$(python3 - <<'PYEOF'
import json
import pathlib

failures = []

for path in sorted(pathlib.Path("assets/content").rglob("*.json")):
    try:
        json.loads(path.read_text(encoding="utf-8"))
    except Exception as error:
        failures.append(f"{path}: {error}")

print("\n".join(failures))
PYEOF
)"

    if [ -z "${JSON_FAILURES}" ]; then
        echo "  OK - every content JSON parses"
    else
        printf '%s\n' "${JSON_FAILURES}"
        echo "  FAILED - a content file does not parse, and its WHOLE bank would be empty"
        FAILURE_COUNT=$((FAILURE_COUNT + 1))
    fi

else
    echo "  python3 not found: skipped"
fi

# ---------------------------------------------------------------------------------------------------------------------
# clang-tidy
#
# It needs a configured build directory to know how to compile each file. The compile_commands.json
# produced by the Debug preset is used when it exists; otherwise the step is skipped with a hint.
# ---------------------------------------------------------------------------------------------------------------------
echo "--- clang-tidy --------------------------------------------------------------------------------"

COMPILE_COMMANDS="${PROJECT_DIR}/../Musichien-build/Clang-Debug/compile_commands.json"

if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "  clang-tidy not found: skipped"
elif [ ! -f "${COMPILE_COMMANDS}" ]; then
    echo "  ${COMPILE_COMMANDS} not found"
    echo "  configure the project first:  cmake --preset \"Clang-Debug Musichien\""
else

    # Careful: piping clang-tidy into 'tail' would make the 'if' test the status of 'tail', which is
    # always zero. The output is therefore captured first, and inspected for findings.
    #
    # ONE FILE PER CORE. clang-tidy re-parses every header of the project for each translation unit, so a
    # sequential run spends minutes doing the same work over and over - the same reason a compiler is never
    # invoked one file at a time on a modern machine. Parallelising it is what takes this step from minutes
    # to seconds, and it costs nothing in coverage: every file is still checked, only the waiting is removed.
    TIDY_LOG="$(mktemp)"
    TIDY_JOBS="$(nproc 2>/dev/null || echo 4)"

    set +e

    # AndroidNotificationScheduler.cpp and AndroidSystemBars.cpp are excluded: they are compiled ONLY on Android, and
    # their JNI headers do not exist on the desktop. clang-tidy would otherwise fail on files it cannot parse here.
    find source -type f -name '*.cpp' ! -name '*_test.cpp' ! -name 'AndroidNotificationScheduler.cpp' ! -name 'AndroidSystemBars.cpp' -print0 |
        xargs -0 -n1 -P "${TIDY_JOBS}" \
            clang-tidy -p "${PROJECT_DIR}/../Musichien-build/Clang-Debug" \
                       --config-file="${PROJECT_DIR}/.clang-tidy" > "${TIDY_LOG}" 2>&1

    # xargs reports a non zero status as soon as ONE of its commands does, and clang-tidy does exactly that
    # when it has something to say. The log is what decides, not the exit status.
    TIDY_STATUS=0

    # One progress line per finished file, so that a slow run can be watched instead of waited on.
    printf '  checked %s file(s) on %s core(s)\n' \
           "$(find source -type f -name '*.cpp' ! -name '*_test.cpp' | wc -l)" "${TIDY_JOBS}"

    # The findings themselves, and nothing else: the 'Suppressed ... warnings ...' summary of each run would
    # bury them under thousands of lines that say the same thing.
    TIDY_FINDINGS="$(grep -E 'warning:|error:' "${TIDY_LOG}" || true)"

    set -e

    if [ -n "${TIDY_FINDINGS}" ]; then
        # LE JOURNAL COMPLET EST CONSERVE, et c'est deliberé : n'afficher que les trente derniers findings rendait tout
        # diagnostic impossible - on voyait les trouvailles dans l'ordre des fichiers, jamais celles qui comptent, et il
        # fallait relancer vingt minutes d'analyse pour voir la suite. Le fichier est ecrit dans le dossier de build, a
        # cote de compile_commands.json, donc au meme endroit que ce que l'analyse a lu.
        TIDY_REPORT="${PROJECT_DIR}/../Musichien-build/Clang-Debug/clang-tidy-report.txt"

        printf '%s\n' "${TIDY_FINDINGS}" > "${TIDY_REPORT}"

        # UN RESUME PAR REGLE, du plus frequent au moins frequent : c'est ce qui dit en une seconde si l'echec est un
        # vrai risque ou une regle a regler. Compter les trouvailles ligne par ligne ne le disait pas.
        #
        # Les codes COULEUR de clang-tidy (UseColor) sont retires AVANT tout comptage : sans cela, chaque ligne finit par
        # une sequence d'echappement et non par « [regle] », donc aucun regroupement ne se fait et le resume affiche
        # 214 lignes comptees une par une - ce qui ne resume rien du tout.
        TIDY_PLAIN="$(printf '%s\n' "${TIDY_FINDINGS}" | sed -E $'s/\033\\[[0-9;]*m//g')"

        printf '  %s finding(s) - summary by rule (most frequent first):\n' "$(printf '%s\n' "${TIDY_PLAIN}" | wc -l)"

        printf '%s\n' "${TIDY_PLAIN}" |
            sed -E 's/^.*\[([^]]*)\][^]]*$/\1/' |
            tr ',' '\n' |
            sort | uniq -c | sort -rn | head -12 |
            sed 's/^/    /'

        printf '  every finding: %s\n' "${TIDY_REPORT}"
        echo "  FAILED - see the summary above, and the report for the details"
        FAILURE_COUNT=$((FAILURE_COUNT + 1))
    else
        echo "  OK - no finding"
    fi

    rm -f "${TIDY_LOG}"
fi


# ---------------------------------------------------------------------------------------------------------------------
# Result
# ---------------------------------------------------------------------------------------------------------------------
echo
if [ ${FAILURE_COUNT} -eq 0 ]; then
    echo "check_code.sh: everything is clean"
else
    echo "check_code.sh: ${FAILURE_COUNT} check(s) failed"
    exit 1
fi
