#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Local packaging capability checks and .build-host inspection.
#
# Remote sync/SSH builds are handled by gh-makerelease (not this script).
#
# Usage:
#   host.sh can-local <kind>
#   host.sh run <kind> -- <command...>   # local only; fails if tools missing
#   host.sh dump [file]
#
# Environment:
#   PIDLOAD_SRCDIR       project root (default: packaging/../..)
#   PIDLOAD_FORCE_LOCAL  skip can-local check (set on a build host by gh-makerelease)
set -euo pipefail

_LIBDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PIDLOAD_SRCDIR=${PIDLOAD_SRCDIR:-$(cd "$_LIBDIR/../.." && pwd)}

_usage() {
    sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'
}

_strip_comment() {
    local line=$1
    line=${line%%#*}
    printf '%s' "$line"
}

# Parse a .build-host file into parallel arrays (last hop is the build host).
# Sets: BH_N BH_NAME BH_HOST BH_USER BH_PASSWORD BH_IDENTITY BH_BUILD_DIR BH_PRESERVED
parse_build_host_file() {
    local file=$1
    BH_N=0
    BH_NAME=()
    BH_HOST=()
    BH_USER=()
    BH_PASSWORD=()
    BH_IDENTITY=()
    BH_BUILD_DIR=()
    BH_PRESERVED=()
    BH_SHELL=()

    local name="" host="" user="" password="" identity="" build_dir="" preserved="" shell=""
    local had=0

    _bh_flush() {
        if ((had)) && [[ -n $host ]]; then
            BH_NAME[BH_N]=$name
            BH_HOST[BH_N]=$host
            BH_USER[BH_N]=$user
            BH_PASSWORD[BH_N]=$password
            BH_IDENTITY[BH_N]=$identity
            BH_BUILD_DIR[BH_N]=$build_dir
            BH_PRESERVED[BH_N]=$preserved
            BH_SHELL[BH_N]=$shell
            BH_N=$((BH_N + 1))
        fi
        name="" host="" user="" password="" identity="" build_dir="" preserved="" shell=""
        had=0
    }

    local line key val
    while IFS= read -r line || [[ -n $line ]]; do
        line=$(_strip_comment "$line")
        line=${line%"${line##*[![:space:]]}"}
        if [[ -z ${line//[[:space:]]/} ]]; then
            if ((had)); then
                _bh_flush
            fi
            continue
        fi
        key=${line%%:*}
        val=${line#*:}
        key=$(printf '%s' "$key" | tr '[:upper:]' '[:lower:]' | sed 's/[[:space:]]//g')
        val=${val#"${val%%[![:space:]]*}"}
        val=${val%"${val##*[![:space:]]}"}
        had=1
        case $key in
            name) name=$val ;;
            host) host=$val ;;
            user) user=$val ;;
            password) password=$val ;;
            identity)
                identity=$val
                identity=${identity/#\~/$HOME}
                ;;
            build_dir) build_dir=$val ;;
            preserved) preserved=$val ;;
            shell) shell=$val ;;
        esac
    done < "$file"
    if ((had)); then
        _bh_flush
    fi
}

_kind_fallbacks() {
    case $1 in
        mingw) echo mingw win32 ;;
        innosetup) echo innosetup win32 ;;
        wix) echo wix win32 ;;
        *) echo "$1" ;;
    esac
}

find_build_host_file() {
    local kind=$1
    local k base
    for k in $(_kind_fallbacks "$kind"); do
        for base in "$PIDLOAD_SRCDIR/.config/pidload" "$HOME/.config/pidload"; do
            if [[ -f $base/$k.build-host ]]; then
                printf '%s\n' "$base/$k.build-host"
                return 0
            fi
        done
    done
    return 1
}

_bool_true() {
    case $(printf '%s' "$1" | tr '[:upper:]' '[:lower:]') in
        true|yes|1|on) return 0 ;;
        *) return 1 ;;
    esac
}

last_preserved() {
    local i=$((BH_N - 1))
    local p=${BH_PRESERVED[i]}
    local d=${BH_BUILD_DIR[i]}
    if [[ -n $p ]]; then
        _bool_true "$p"
        return
    fi
    [[ -n $d ]]
}

# Effective remote shell for kind (last hop). innosetup/wix → powershell.
last_shell() {
    local kind=${1:-}
    local i=$((BH_N - 1))
    local s=${BH_SHELL[i]:-}
    s=$(printf '%s' "$s" | tr '[:upper:]' '[:lower:]')
    case $s in
        ps|pwsh|powershell|powershell.exe) echo powershell; return ;;
        cmd|cmd.exe) echo cmd; return ;;
        bash|sh) echo bash; return ;;
    esac
    case $kind in
        innosetup|wix) echo powershell ;;
        *) echo bash ;;
    esac
}

innosetup_iscc() {
    if command -v iscc >/dev/null 2>&1; then
        printf '%s\n' "$(command -v iscc)"
        return 0
    fi
    if command -v ISCC >/dev/null 2>&1; then
        printf '%s\n' "$(command -v ISCC)"
        return 0
    fi
    local d
    for d in \
        ${INNOSETUP_DIR:+"$INNOSETUP_DIR"} \
        "$HOME/.wine/drive_c/Program Files (x86)/Inno Setup 6" \
        "$HOME/.wine/drive_c/Program Files/Inno Setup 6" \
        "$HOME/.wine/drive_c/Program Files (x86)/Inno Setup 5" \
        /opt/inno-setup
    do
        if [[ -f $d/ISCC.exe ]]; then
            printf '%s\n' "$d/ISCC.exe"
            return 0
        fi
    done
    return 1
}

_is_windows_host() {
    local sys
    sys=$(uname -s 2>/dev/null || echo unknown)
    [[ ${OS:-} == Windows_NT || $sys == MINGW* || $sys == MSYS* || $sys == CYGWIN* ]]
}

can_local() {
    local kind=$1
    local sys
    sys=$(uname -s 2>/dev/null || echo unknown)
    case $kind in
        mingw)
            if _is_windows_host; then
                command -v meson >/dev/null 2>&1 || command -v gcc >/dev/null 2>&1
                return
            fi
            command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1 \
                || command -v i686-w64-mingw32-gcc >/dev/null 2>&1
            ;;
        innosetup)
            # Native Windows uses build.cmd / build.ps1 (cmd + PowerShell).
            if _is_windows_host; then
                return 0
            fi
            if command -v iscc >/dev/null 2>&1 || command -v ISCC >/dev/null 2>&1; then
                return 0
            fi
            if command -v wine >/dev/null 2>&1 && innosetup_iscc >/dev/null 2>&1; then
                return 0
            fi
            return 1
            ;;
        wix)
            # Native Windows uses build.cmd / build.ps1.
            if _is_windows_host; then
                return 0
            fi
            command -v wix >/dev/null 2>&1 \
                || command -v candle >/dev/null 2>&1 \
                || command -v wixl >/dev/null 2>&1
            ;;
        macos)
            [[ $sys == Darwin ]]
            ;;
        freebsd)
            [[ $sys == FreeBSD ]]
            ;;
        arch)
            [[ -f /etc/arch-release ]] && command -v makepkg >/dev/null 2>&1
            ;;
        rpm)
            command -v rpmbuild >/dev/null 2>&1
            ;;
        win32)
            can_local mingw || can_local innosetup || can_local wix
            ;;
        *)
            return 1
            ;;
    esac
}

cmd_dump() {
    local file=${1:-}
    if [[ -z $file ]]; then
        file=$(find_build_host_file "${PIDLOAD_PACKAGING:-win32}") || {
            echo "no .build-host file" >&2
            return 1
        }
    fi
    parse_build_host_file "$file"
    local i
    echo "file=$file"
    echo "count=$BH_N"
    for ((i = 0; i < BH_N; i++)); do
        echo "---"
        echo "index=$i"
        echo "name=${BH_NAME[i]}"
        echo "host=${BH_HOST[i]}"
        echo "user=${BH_USER[i]}"
        echo "identity=${BH_IDENTITY[i]}"
        echo "build_dir=${BH_BUILD_DIR[i]}"
        echo "preserved=${BH_PRESERVED[i]}"
        echo "shell=${BH_SHELL[i]}"
        echo "password_set=$([ -n "${BH_PASSWORD[i]}" ] && echo yes || echo no)"
    done
    if ((BH_N > 0)); then
        echo "---"
        echo "last_build_dir=${BH_BUILD_DIR[$((BH_N - 1))]}"
        echo "last_shell=$(last_shell "${PIDLOAD_PACKAGING:-}")"
        if last_preserved; then
            echo "last_preserved=true"
        else
            echo "last_preserved=false"
        fi
    fi
}

cmd_run() {
    local kind=$1
    shift
    if [[ ${1:-} == -- ]]; then
        shift
    fi
    if [[ ${PIDLOAD_FORCE_LOCAL:-} == 1 ]]; then
        exec "$@"
    fi
    if can_local "$kind"; then
        exec "$@"
    fi
    echo "host.sh: cannot build '$kind' on this host (native/cross tools missing)." >&2
    echo "  For remote builds, use gh-makerelease with a .build-host file:" >&2
    local k
    for k in $(_kind_fallbacks "$kind"); do
        echo "    $PIDLOAD_SRCDIR/.config/pidload/$k.build-host" >&2
        echo "    $HOME/.config/pidload/$k.build-host" >&2
    done
    return 2
}

main() {
    local cmd=${1:-}
    shift || true
    case $cmd in
        can-local)
            can_local "${1:?kind}"
            ;;
        run)
            cmd_run "$@"
            ;;
        dump)
            cmd_dump "${1:-}"
            ;;
        -h|--help|"")
            _usage
            ;;
        *)
            echo "host.sh: unknown command $cmd" >&2
            return 2
            ;;
    esac
}

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then
    main "$@"
fi
