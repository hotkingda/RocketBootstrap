#!/bin/bash
set -e

DEVICE_IP="${THEOS_DEVICE_IP:-iphone}"
DEVICE_PORT="${THEOS_DEVICE_PORT:-22}"
REPO="hotkingda/RocketBootstrap"
PKG_ID="com.rpetrich.rocketbootstrap"

cd "$(dirname "$0")"

VERSION=$(grep -m1 '^Version:' control | awk '{print $2}')

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

usage() {
    echo -e "${BOLD}RocketBootstrap Build Script${RESET}"
    echo ""
    echo -e "Usage: $0 <command> [options]"
    echo ""
    echo -e "${CYAN}Commands:${RESET}"
    echo "  debug         Build debug .deb (rootless)"
    echo "  release        Build release .deb (rootless, stripped)"
    echo "  install        Build debug + install to device via SSH"
    echo "  clean          Clean build artifacts"
    echo "  gh             Build release + upload to GitHub Release"
    echo ""
    echo -e "${CYAN}Options:${RESET}"
    echo "  -i, --ip HOST  Device IP (default: \$THEOS_DEVICE_IP or 'iphone')"
    echo "  -p, --port N   Device SSH port (default: \$THEOS_DEVICE_PORT or 22)"
    echo ""
    echo -e "${CYAN}Examples:${RESET}"
    echo "  $0 debug"
    echo "  $0 install -i 192.168.1.100"
    echo "  $0 gh"
}

log() { echo -e "${GREEN}==>${RESET} ${BOLD}$1${RESET}"; }
warn() { echo -e "${YELLOW}==>${RESET} ${BOLD}$1${RESET}"; }
err() { echo -e "${RED}==>${RESET} ${BOLD}$1${RESET}" >&2; exit 1; }

do_clean() {
    log "Cleaning..."
    make clean
    rm -rf packages/*.deb
}

do_build() {
    local final="$1"
    local extra_args="THEOS_PACKAGE_SCHEME=rootless"

    if [ "$final" = "1" ]; then
        extra_args="$extra_args FINALPACKAGE=1"
        log "Building release v${VERSION} (rootless, arm64+arm64e)..."
    else
        log "Building debug v${VERSION} (rootless, arm64+arm64e)..."
    fi

    make package $extra_args

    local deb
    deb=$(ls -t packages/${PKG_ID}_*.deb 2>/dev/null | head -1)
    if [ -z "$deb" ]; then
        err "No .deb found after build"
    fi
    echo -e "${GREEN}==>${RESET} Built: ${CYAN}${deb}${RESET} ($(du -h "$deb" | cut -f1 | xargs))"
    echo "$deb"
}

do_install() {
    log "Building + installing to ${DEVICE_IP}:${DEVICE_PORT}..."
    local deb
    deb=$(do_build 0 | tail -1)

    log "Deploying ${deb}..."
    scp -P "$DEVICE_PORT" "$deb" "root@${DEVICE_IP}:/tmp/rbs.deb"
    ssh -p "$DEVICE_PORT" "root@${DEVICE_IP}" "dpkg -i /tmp/rbs.deb && rm /tmp/rbs.deb && killall -9 SpringBoard" || true
    log "Installed. SpringBoard is restarting."
}

do_github() {
    log "Building release for GitHub..."
    do_clean
    local deb
    deb=$(do_build 1 | tail -1)

    local tag="v${VERSION}"
    log "Creating GitHub Release ${tag}..."

    local commit_log
    commit_log=$(git log --oneline -5 | sed 's/^/- /')

    gh release delete "$tag" --repo "$REPO" --yes 2>/dev/null || true
    git tag -d "$tag" 2>/dev/null || true
    git push origin ":refs/tags/$tag" 2>/dev/null || true

    git tag "$tag" HEAD
    git push origin "$tag"

    gh release create "$tag" \
        --repo "$REPO" \
        --title "RocketBootstrap ${tag}" \
        --notes "## RocketBootstrap ${tag}

### Recent Changes
${commit_log}

### Info
- Architecture: arm64 + arm64e (fat binary)
- Package scheme: rootless
- Minimum iOS: 13.0
" \
        "$deb"

    log "Release published: https://github.com/${REPO}/releases/tag/${tag}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -i|--ip)    DEVICE_IP="$2"; shift 2 ;;
        -p|--port)  DEVICE_PORT="$2"; shift 2 ;;
        debug)      CMD="debug"; shift ;;
        release)    CMD="release"; shift ;;
        install)    CMD="install"; shift ;;
        clean)      CMD="clean"; shift ;;
        gh)         CMD="gh"; shift ;;
        -h|--help)  usage; exit 0 ;;
        *)          err "Unknown argument: $1" ;;
    esac
done

[ -z "$CMD" ] && { usage; exit 1; }

case "$CMD" in
    clean)   do_clean ;;
    debug)   do_build 0 ;;
    release) do_clean; do_build 1 ;;
    install) do_install ;;
    gh)      do_github ;;
esac
