#!/usr/bin/env bash
# =====================================================================================================================
# Musichien - shrink_apk.sh
#
# Enleve du paquet les bibliotheques Qt que l'application n'execute jamais, et re-signe le resultat.
#
# ---------------------------------------------------------------------------------------------------------------------
# POURQUOI CE SCRIPT EXISTE, ET POURQUOI PAS DANS LE BUILD
#
# Le paquet pese environ cent megaoctets, et le CONTENU n'en fait que vingt-deux. Une part de l'infrastructure Qt
# embarquee est INUTILE a Musichien :
#
#   * QUATRE STYLES DE WIDGETS que l'ecran ne montre jamais. Le qmldir de QtQuick.Controls declare les cinq styles en
#     « optional ... auto », donc Qt les deploie tous ; Musichien n'utilise que Material. Environ QUINZE megaoctets.
#
#   * LES PLUGINS DE DEBOGAGE QML (qmldbg_*), qui servent a inspecter une application depuis Qt Creator. Un paquet de
#     publication n'en a aucun besoin. Environ UN megaoctet.
#
# Essaye d'abord DANS le build, et mesure : `qt_import_plugins(EXCLUDE ...)` NE RETIRE RIEN, parce que Qt collecte la
# liste des bibliotheques depuis ses propres dependances et non depuis la cible de l'application. 100,0 Mo avant comme
# apres.
#
# ---------------------------------------------------------------------------------------------------------------------
# POURQUOI IL PASSE PAR LE DOSSIER DE BUILD, ET NON SUR L'APK
#
# Un premier jet retirait les .so de l'APK deja signe : l'application MOURAIT au demarrage. La raison est dans
# 'res/values/libs.xml', que Qt genere avec le paquet :
#
#     <array name="qt_libs">
#         <item>arm64-v8a;Qt6Core_arm64-v8a</item>
#         ...
#         <item>arm64-v8a;Qt6QuickControls2FusionStyleImpl_arm64-v8a</item>
#
# Le chargeur Java lit CETTE liste et ouvre chaque bibliotheque au demarrage ; une bibliotheque listee mais absente fait
# echouer 'loadLibrary', et Qt appelle System.exit. Retirer un .so demande donc de retirer AUSSI sa ligne.
#
# Ce fichier vit dans le dossier de build, et les bibliotheques dans son sous-dossier 'libs/' - que Gradle reprend tels
# quels (voir 'jniLibs.srcDirs'). Les modifier AVANT de relancer Gradle suffit donc : rien n'est pirate dans un paquet
# signe.
#
# ---------------------------------------------------------------------------------------------------------------------
# CE QUI EST RETIRE, ET CE QUI RESTE
#
#   4 styles de widgets (Fusion, Imagine, Universal, FluentWinUI3)  -> MATERIAL et BASIC restent
#   les plugins de debogage QML (qmldbg_*)                         -> ils ne sont PAS dans la liste de chargement
#   le moteur multimedia FFmpeg, ET les bibliotheques libav*       -> 'androidmediaplugin' reste, c'est lui qui SORT le son
#
# BASIC reste, et c'est une ceinture : c'est le style PAR DEFAUT du module, et le laisser garantit que l'application
# s'affiche meme si Material manquait un jour.
#
# FFMPEG PART AUSSI, et c'est la preuve qui l'autorise : 'libQt6Multimedia' n'exige AUCUNE bibliotheque FFmpeg - seul le
# GREFFON les exige, et il est le seul a les demander au chargeur. Musichien ne lit aucun fichier media : il fabrique ses
# echantillons et lit ses propres WAV dans le domaine. Le drapeau '--keep-media' le remet, et c'est le seul : la ou le son
# est en jeu, c'est l'oreille qui tranche, pas un raisonnement.
#
# ---------------------------------------------------------------------------------------------------------------------
# LA LIGNE QUI A FAILLI FAIRE ECHOUER TOUT LE LOT
#
# La liste de chargement contient une entree faite d'une CHAINE, separee par des deux-points :
#
#     <item>arm64-v8a;libplugins_platforms_qtforandroid_...so:libavcodec.so:...:libplugins_multimedia_androidmediaplugin_...so</item>
#
# Elle se filtre MAILLON PAR MAILLON : retirer la ligne entiere emporterait le greffon de plateforme et celui du son, qui
# n'ont rien a voir avec FFmpeg, et l'application ne demarrerait plus du tout.
#
# ---------------------------------------------------------------------------------------------------------------------
# CE QU'IL NE TOUCHE PAS
#
# Le CONTENU : aucun son, aucune image, aucun texte. Roger l'a demande ainsi - « ne reduit pas le contenu ».
#
# ---------------------------------------------------------------------------------------------------------------------
# COMMENT IL S'EN SERT
#
#   scripts/build_android.sh --no-install      # construit et signe, comme d'habitude
#   scripts/shrink_apk.sh                      # allege, relance Gradle, re-signe
#   scripts/shrink_apk.sh --keep-media         # la meme chose, moteur FFmpeg compris
#
# ---------------------------------------------------------------------------------------------------------------------

set -euo pipefail

SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIRECTORY="$(cd "${SCRIPT_DIRECTORY}/.." && pwd)"

# shellcheck source=/dev/null
source "${SCRIPT_DIRECTORY}/setup_env.sh" >/dev/null 2>&1

BUILD_DIRECTORY="${PROJECT_DIRECTORY}/../Musichien-build/Android-Release/source/application"
ANDROID_BUILD_DIRECTORY="${BUILD_DIRECTORY}/android-build"
LIBRARIES_XML="${ANDROID_BUILD_DIRECTORY}/res/values/libs.xml"
NATIVE_LIBRARIES_DIRECTORY="${ANDROID_BUILD_DIRECTORY}/libs"

# La liste NON filtree est gardee HORS de 'res/' : le fusionneur de ressources de Gradle lit TOUT ce qui s'y trouve, et un
# fichier de sauvegarde qui ne finit pas par '.xml' fait echouer la construction avec un message qui ne parle pas de lui.
# C'est ce qui est arrive au premier essai de ce script.
FULL_LIBRARIES_XML="${ANDROID_BUILD_DIRECTORY}/libs.xml.full"

if [ ! -f "${LIBRARIES_XML}" ]; then
    echo "ERROR: no '${LIBRARIES_XML}'."
    echo "       Build once first: scripts/build_android.sh --no-install"
    exit 1
fi

# Les motifs retires, ecrits EN CLAIR pour qu'un lecteur - ou Roger - puisse verifier la liste.
#
# LE MEME MOTIF SERT AUX DEUX FORMES : la liste de chargement nomme les bibliotheques une par une ('avcodec'), et sa chaine
# de dependances les nomme avec leur prefixe de fichier ('libavcodec.so'). Un motif qui apparait dans les deux, comme
# 'avcodec', les attrape donc toutes les deux.
REMOVED_PATTERNS=(
    # Les styles de widgets que l'ecran ne montre jamais : il n'utilise que Material.
    "Qt6QuickControls2Fusion"
    "Qt6QuickControls2Imagine"
    "Qt6QuickControls2Universal"
    "Qt6QuickControls2FluentWinUI3"
    # Les plugins de debogage QML : ils servent a inspecter depuis Qt Creator, et rien d'autre.
    "plugins_qmltooling_"
    # LE MOTEUR FFMPEG, quand il est retire : voir KEEP_MEDIA_ENGINE plus bas.
    "ffmpegmediaplugin"
    "FFmpegStub"
    "avcodec"
    "avformat"
    "avutil"
    "swscale"
    "swresample"
)

# Le moteur multimedia FFmpeg part par DEFAUT. Il ne sert a RIEN ici - Musichien ne lit aucun fichier media, il fabrique
# ses echantillons et lit ses propres WAV dans le domaine - et il pese a lui seul environ DIX-SEPT megaoctets.
#
# La preuve qu'il peut partir est dans l'edition de liens, et elle a ete verifiee : 'libQt6Multimedia' n'exige AUCUNE
# bibliotheque FFmpeg. Seul le GREFFON les exige, et c'est le seul qui les demande au chargeur. Sans lui, Qt Multimedia
# garde 'androidmediaplugin', qui est celui dont QAudioSink se sert pour SORTIR du son.
#
# '--keep-media' le remet, et c'est le seul drapeau : un paquet ou le son est en jeu se remet en cause par l'oreille, pas
# par un raisonnement.
KEEP_MEDIA_ENGINE="false"

if [ "${1:-}" = "--keep-media" ]; then
    KEEP_MEDIA_ENGINE="true"
fi

if [ "${KEEP_MEDIA_ENGINE}" = "true" ]; then
    # Les neuf derniers motifs sont ceux du moteur : le tableau est reecrit sans eux.
    REMOVED_PATTERNS=(
        "Qt6QuickControls2Fusion"
        "Qt6QuickControls2Imagine"
        "Qt6QuickControls2Universal"
        "Qt6QuickControls2FluentWinUI3"
        "plugins_qmltooling_"
    )

    echo "  (--keep-media: the FFmpeg engine stays in the package)"
    echo
fi

echo "--- Musichien: shrinking the package ----------------------------------------------------------"
echo

# ---------------------------------------------------------------------------------------------------------------------
# 1. Retirer les lignes de la LISTE, puis les bibliotheques elles-memes
#
# Les deux vont ensemble, et dans cet ordre : une ligne sans bibliotheque tue l'application (c'est ce qu'a fait le premier
# jet), et une bibliotheque sans ligne est chargee quand meme - du poids mort qu'on croit avoir retire.
# ---------------------------------------------------------------------------------------------------------------------
echo "  filtering the loading list..."

# La liste d'origine est restauree d'abord, MAIS seulement si la liste courante est deja filtree. Un dossier de build
# regenere par androiddeployqt porte une liste fraiche : la remplacer par une sauvegarde plus vieille ferait silencieusement
# revenir a l'etat precedent. Le test est simple - la liste courante est filtree quand AUCUN motif ne s'y trouve plus.
ALL_PATTERNS="$(IFS='|'; echo "${REMOVED_PATTERNS[*]}")"

if grep -qE "${ALL_PATTERNS}" "${LIBRARIES_XML}"; then
    cp "${LIBRARIES_XML}" "${FULL_LIBRARIES_XML}"
elif [ -f "${FULL_LIBRARIES_XML}" ]; then
    cp "${FULL_LIBRARIES_XML}" "${LIBRARIES_XML}"
fi

python3 - "${LIBRARIES_XML}" "${REMOVED_PATTERNS[@]}" <<'PYTHON'
import sys

path = sys.argv[1]
patterns = sys.argv[2:]


def is_wanted(segment):
    return not any(pattern in segment for pattern in patterns)


kept = []
removed = 0

with open(path, encoding="utf-8") as source:
    for line in source:
        stripped = line.strip()

        # Les balises, les commentaires et les chaines simples restent intacts : seules les ENTREES sont filtrees.
        if not stripped.startswith("<item>") or not stripped.endswith("</item>"):
            kept.append(line)
            continue

        body = stripped[len("<item>"):-len("</item>")]

        # Une entree peut etre une CHAINE, separee par des deux-points : le chargeur en ouvre alors chaque maillon dans
        # l'ordre. C'est ainsi qu'est declaree 'load_local_libs', ou le greffon de plateforme, celui du multimedia et les
        # bibliotheques FFmpeg se suivent sur UNE SEULE ligne.
        #
        # Elle se filtre donc MAILLON PAR MAILLON. Retirer la ligne entiere emporterait le greffon de plateforme et celui
        # du son - qui n'ont rien a voir avec FFmpeg - et l'application ne demarrerait plus du tout.
        abi, separator, chain = body.partition(";")
        segments = chain.split(":")
        remaining = [segment for segment in segments if is_wanted(segment)]

        removed += len(segments) - len(remaining)

        if not remaining:
            continue

        kept.append("        <item>" + abi + separator + ":".join(remaining) + "</item>\n")

with open(path, "w", encoding="utf-8") as target:
    target.writelines(kept)

with open(path + ".removed", "w", encoding="utf-8") as count_file:
    count_file.write(str(removed))
PYTHON

REMOVED_COUNT="$(cat "${LIBRARIES_XML}.removed")"
rm -f "${LIBRARIES_XML}.removed"

echo "    ${REMOVED_COUNT} entree(s) retiree(s) de 'qt_libs'"

if [ "${REMOVED_COUNT}" -eq 0 ]; then
    echo "  nothing matched: nothing to do."
    exit 0
fi

echo
echo "  removing the libraries themselves..."

REMOVED_LIBRARIES=0

while IFS= read -r library_file; do
    library_name="$(basename "${library_file}")"

    for pattern in "${REMOVED_PATTERNS[@]}"; do
        case "${library_name}" in
            *"${pattern}"*)
                rm -f "${library_file}"
                echo "    - ${library_name}"
                REMOVED_LIBRARIES=$((REMOVED_LIBRARIES + 1))
                break
                ;;
        esac
    done
done < <(find "${NATIVE_LIBRARIES_DIRECTORY}" -type f -name '*.so')

echo "    ${REMOVED_LIBRARIES} bibliotheque(s) retiree(s)"


# ---------------------------------------------------------------------------------------------------------------------
# 1 bis. La LISTE et les FICHIERS doivent coincider
#
# Une bibliotheque annoncee par la liste mais absente du dossier fait echouer 'loadLibrary', et Qt appelle System.exit :
# c'est le defaut que ce script existe pour corriger. Il se represente quand on redemande le moteur multimedia sur un
# dossier ou il a DEJA ete retire : les fichiers ne reviennent pas tout seuls, et le paquet qui en sortirait ne demarrerait
# pas. Mieux vaut s'arreter ici que de fabriquer une application morte.
# ---------------------------------------------------------------------------------------------------------------------
if [ "${KEEP_MEDIA_ENGINE}" = "true" ] &&
   ! find "${NATIVE_LIBRARIES_DIRECTORY}" -type f -name 'libplugins_multimedia_ffmpegmediaplugin_*.so' | grep -q .; then
    echo
    echo "  ERROR: '--keep-media' was asked for, but the FFmpeg libraries are gone from:"
    echo "         ${NATIVE_LIBRARIES_DIRECTORY}"
    echo "         Rebuild first, then shrink: scripts/build_android.sh --no-install"
    exit 1
fi


# ---------------------------------------------------------------------------------------------------------------------
# 2. Relancer GRADLE, qui reprend 'res/' et 'libs/' tels quels
#
# Gradle recompile la liste filtree et recopie le jeu de bibliotheques reduit : c'est ce qui fait entrer le changement
# dans le paquet SANS jamais toucher a une archive signee.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "  re-running Gradle (this takes a minute)..."

(
    cd "${ANDROID_BUILD_DIRECTORY}"

    # 'clean' n'est pas du zele : Gradle met en cache les ressources fusionnees, et une liste filtree peut passer inapercue
    # pour lui. Le paquet en sortirait inchange, et personne ne le verrait avant de le peser.
    ./gradlew --quiet clean assembleRelease
)

UNSIGNED_PACKAGE_FILE="${ANDROID_BUILD_DIRECTORY}/build/outputs/apk/release/android-build-release-unsigned.apk"

# La sortie de Gradle, et NON le 'musichien_app.apk' de la racine : celui-ci est un exemplaire pose par androiddeployqt, et
# il n'est PAS reecrit par un simple passage de Gradle. Signer ce fichier-la donnerait un paquet d'apparence nouvelle et
# d'un contenu perime - ce qui est arrive au deuxieme essai de ce script, ou la taille n'avait pas bouge d'un octet.
if [ ! -f "${UNSIGNED_PACKAGE_FILE}" ]; then
    echo "  ERROR: Gradle did not produce '${UNSIGNED_PACKAGE_FILE}'."
    exit 1
fi

# ---------------------------------------------------------------------------------------------------------------------
# 3. Aligner, puis SIGNER
#
# L'alignement se fait AVANT la signature : signer reecrit l'archive, et aligner ensuite invaliderait la signature. '-p'
# garde les pages des bibliotheques partagees contigues, ce qui permet a Android de les projeter directement depuis le
# paquet au lieu de les recopier.
#
# La cle est celle de 'build_android.sh', au meme endroit, pour la meme raison : Android refuse une mise a jour dont la
# signature differe de celle deja installee.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "  signing..."

ANDROID_KEYSTORE_DIRECTORY="${HOME}/.android"
ANDROID_KEYSTORE_FILE="${ANDROID_KEYSTORE_DIRECTORY}/musichien-release.keystore"
ANDROID_KEYSTORE_PASSWORD_FILE="${ANDROID_KEYSTORE_DIRECTORY}/musichien-release.password"
ANDROID_KEY_ALIAS="musichien"

APK_SIGNER="$(find "${ANDROID_SDK_ROOT}/build-tools" -maxdepth 2 -type f -name 'apksigner' 2>/dev/null |
              sort -V | tail -n 1)"
ZIP_ALIGNER="$(find "${ANDROID_SDK_ROOT}/build-tools" -maxdepth 2 -type f -name 'zipalign' 2>/dev/null |
               sort -V | tail -n 1)"

if [ -z "${APK_SIGNER}" ] || [ -z "${ZIP_ALIGNER}" ] || [ ! -f "${ANDROID_KEYSTORE_FILE}" ]; then
    echo "  ERROR: apksigner, zipalign or the signing key is missing. See scripts/build_android.sh."
    exit 1
fi

ALIGNED_PACKAGE_FILE="$(mktemp -t musichien-aligned-XXXXXX.apk)"
SIGNED_PACKAGE_FILE="${ANDROID_BUILD_DIRECTORY}/musichien-release-signed.apk"

"${ZIP_ALIGNER}" -f -p 4 "${UNSIGNED_PACKAGE_FILE}" "${ALIGNED_PACKAGE_FILE}"

"${APK_SIGNER}" sign \
    --ks "${ANDROID_KEYSTORE_FILE}" \
    --ks-pass "file:${ANDROID_KEYSTORE_PASSWORD_FILE}" \
    --ks-key-alias "${ANDROID_KEY_ALIAS}" \
    --out "${SIGNED_PACKAGE_FILE}" \
    "${ALIGNED_PACKAGE_FILE}"

rm -f "${ALIGNED_PACKAGE_FILE}"

# Une signature qui ne se verifie pas est refusee a l'installation, avec un message qui ne dit rien de la vraie cause : elle
# est donc verifiee ici, et le script echoue bruyamment plutot que de rendre un paquet inutilisable.
"${APK_SIGNER}" verify --print-certs "${SIGNED_PACKAGE_FILE}" 2>/dev/null | head -4

PACKAGE_SIZE="$(stat -c '%s' "${SIGNED_PACKAGE_FILE}")"

echo
printf '  signed package: %s\n' "$(awk -v s="${PACKAGE_SIZE}" 'BEGIN { printf "%.1f Mo", s / 1048576 }')"
printf '  entries removed from the loading list: %s\n' "${REMOVED_COUNT}"
printf '  libraries removed: %s\n' "${REMOVED_LIBRARIES}"

echo
echo "  OK. TO TEST ON THE PHONE: the interface shows, and the app starts."
echo "  Install: adb install -r '${SIGNED_PACKAGE_FILE}'"

exit 0
