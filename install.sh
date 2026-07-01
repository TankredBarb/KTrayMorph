#!/usr/bin/env bash
set -euo pipefail

APPLET_ID="org.ktraymorph.plasmoid"
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
INSTALL_USER="${SUDO_USER:-${USER}}"
INSTALL_HOME="$(getent passwd "${INSTALL_USER}" | cut -d: -f6)"

if [[ "${EUID}" -ne 0 ]]; then
    echo "Please run this installer as root: sudo ./install.sh" >&2
    exit 1
fi

if [[ -z "${INSTALL_HOME}" ]]; then
    echo "Could not resolve home directory for ${INSTALL_USER}" >&2
    exit 1
fi

LOCAL_PLASMOID_DIRS=(
    "${INSTALL_HOME}/.local/share/plasma/plasmoids/${APPLET_ID}"
    "${INSTALL_HOME}/.local/share/kpackage/plasmoids/${APPLET_ID}"
)

echo "==> Configuring build"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release

echo "==> Building"
cmake --build "${BUILD_DIR}"

echo "==> Removing user-local plasmoid copies"
for dir in "${LOCAL_PLASMOID_DIRS[@]}"; do
    if [[ -e "${dir}" ]]; then
        echo "    removing ${dir}"
        rm -rf -- "${dir}"
    fi
done

echo "==> Installing system files"
cmake --install "${BUILD_DIR}"

echo "==> Installing Plasma applet globally"
if kpackagetool6 --type Plasma/Applet --global --upgrade "${ROOT_DIR}/plasmoid/package"; then
    :
else
    kpackagetool6 --type Plasma/Applet --global --install "${ROOT_DIR}/plasmoid/package"
fi

echo "==> Restarting Plasma"
runuser -u "${INSTALL_USER}" -- kquitapp6 plasmashell || true
runuser -u "${INSTALL_USER}" -- kstart plasmashell

echo "==> Installed ${APPLET_ID}"
