#!/usr/bin/env bash
set -u

fail=0
PREFIX="${SMARTTYPE_PREFIX:-$HOME/.local}"
addon_dir=""
ui_library=""
addon_dirs=()
for candidate in "$PREFIX/lib/fcitx5" "$PREFIX/lib64/fcitx5"; do
    if [[ -d $candidate ]]; then
        addon_dirs+=("$candidate")
        if [[ -z $addon_dir ]]; then
            addon_dir=$candidate
        fi
        if [[ -z $ui_library && -f $candidate/smarttypeui.so ]]; then
            ui_library=$candidate/smarttypeui.so
        fi
    fi
done
check() {
    local label=$1
    shift
    if "$@" >/dev/null 2>&1; then
        printf 'OK    %s\n' "$label"
    else
        printf 'FAIL  %s\n' "$label"
        fail=1
    fi
}

check "fcitx5 runtime" command -v fcitx5
check "SmartType addon description" test -f "$PREFIX/share/fcitx5/addon/smarttype.conf"
check "SmartType input method description" test -f "$PREFIX/share/fcitx5/inputmethod/smarttype.conf"
check "SmartType library" test -n "$addon_dir" -a -f "$addon_dir/smarttype.so"
check "SmartType candidate-panel library" test -n "$ui_library" -a -f "$ui_library"
check "SmartType control tool" test -x "$PREFIX/bin/smarttypectl"
check "SmartType tray and settings" test -x "$PREFIX/bin/smarttype-tray"
check "SmartType desktop entry" test -f "$PREFIX/share/applications/smarttype-tray.desktop"
check "SmartType desktop icon" test -f "$PREFIX/share/icons/hicolor/512x512/apps/smarttype-idle.png"
check "SmartType tray service" test -f "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/smarttype-tray.service"
check "SmartType layout synchronizer" test -x "$HOME/.local/bin/fcitx5-layout-sync.py"

if [[ -n $ui_library ]] && command -v nm >/dev/null 2>&1 &&
   nm -D "$ui_library" 2>/dev/null |
       grep -q '_ZN5fcitx9classicui14XCBInputWindow6updateEPNS_12InputContextE'; then
    printf 'OK    SmartType candidate panel includes native X11 support\n'
elif [[ ${XDG_SESSION_TYPE:-} == x11 ]]; then
    printf 'FAIL  installed SmartType panel is Wayland-only, but this is an X11 session\n'
    printf '      Fix: rebuild with -DSMARTTYPE_ENABLE_X11=ON and reinstall\n'
    fail=1
else
    printf 'WARN  installed SmartType panel does not include native X11 support\n'
fi

if [[ -x "$PREFIX/bin/smarttypectl" ]]; then
    if [[ $("$PREFIX/bin/smarttypectl" get-setting enabled 2>/dev/null) == "enabled: 1" ]]; then
        printf 'OK    SmartType corrections are globally enabled\n'
    else
        printf 'FAIL  SmartType is globally disabled (tray and Fcitx can still look active)\n'
        printf '      Fix: %s/bin/smarttypectl set-setting enabled 1\n' "$PREFIX"
        fail=1
    fi
fi
# Accept install-user name (90-smarttype.conf) or hand-named drop-ins with FCITX_ADDON_DIRS.
if [[ -f "$HOME/.config/environment.d/90-smarttype.conf" ]] ||
   grep -Rqs 'FCITX_ADDON_DIRS=' "$HOME/.config/environment.d/" 2>/dev/null; then
    printf 'OK    KDE environment configuration (FCITX_ADDON_DIRS)\n'
else
    printf 'FAIL  KDE environment configuration (no FCITX_ADDON_DIRS in environment.d)\n'
    printf '      Fix: re-run scripts/install-user.sh\n'
    fail=1
fi

fcitx_pid=$(pgrep -n -x fcitx5 2>/dev/null || true)
if [[ -n $fcitx_pid ]]; then
    printf 'OK    fcitx5 process is running\n'
else
    printf 'WARN  fcitx5 is not running; select it in KDE Virtual Keyboard and relogin\n'
fi

if systemctl --user is-enabled --quiet smarttype-tray.service 2>/dev/null; then
    printf 'OK    SmartType tray service is enabled at login\n'
else
    printf 'FAIL  SmartType tray service is not enabled at login\n'
    fail=1
fi
if systemctl --user is-active --quiet smarttype-tray.service 2>/dev/null; then
    tray_main_pid=$(systemctl --user show smarttype-tray.service -p MainPID --value)
    tray_exe=$(readlink -f "/proc/$tray_main_pid/exe" 2>/dev/null || true)
    if [[ $tray_exe == "$PREFIX/bin/smarttype-tray" ]]; then
        printf 'OK    SmartType tray service owns the installed process (PID %s)\n' "$tray_main_pid"
    else
        printf 'FAIL  SmartType tray service process is stale or unowned\n'
        fail=1
    fi
else
    printf 'FAIL  SmartType tray service is not running\n'
    fail=1
fi

desktop_name=${XDG_CURRENT_DESKTOP:-}
if [[ -z $desktop_name ]]; then
    desktop_name=$(systemctl --user show-environment 2>/dev/null |
        sed -n 's/^XDG_CURRENT_DESKTOP=//p' | head -n1)
fi
session_type=${XDG_SESSION_TYPE:-}
manager_environment=$(systemctl --user show-environment 2>/dev/null || true)
if [[ -z $session_type ]]; then
    session_type=$(printf '%s\n' "$manager_environment" |
        sed -n 's/^XDG_SESSION_TYPE=//p' | head -n1)
fi
x11_session=0
if [[ ${session_type,,} == x11 ]]; then
    x11_session=1
elif [[ ${session_type,,} != wayland ]] &&
     [[ $manager_environment == *$'\nDISPLAY='* || $manager_environment == DISPLAY=* ]]; then
    # DISPLAY is also present in normal Wayland sessions for XWayland clients;
    # trust it only when the session is not explicitly Wayland. SSH may report
    # tty even though the user manager still owns an X11 desktop.
    x11_session=1
fi
if [[ ${desktop_name,,} == *kde* || ${desktop_name,,} == *plasma* ]]; then
    if systemctl --user is-active --quiet fcitx5-layout-sync.service; then
        printf 'OK    SmartType KDE layout synchronizer is running\n'
    else
        # Without this bridge, Alt+Shift only moves the KDE indicator; typing
        # stays on the current smarttype / smarttype-us input method.
        printf 'FAIL  SmartType KDE layout synchronizer is not running\n'
        printf '      Fix: systemctl --user enable --now fcitx5-layout-sync.service\n'
        printf '      Or re-run: scripts/install-user.sh --enable-kde-layout-sync\n'
        fail=1
    fi

    if systemctl --user is-enabled --quiet fcitx5-layout-sync.service 2>/dev/null; then
        printf 'OK    SmartType KDE layout synchronizer is enabled at login\n'
    else
        printf 'WARN  KDE layout synchronizer not enabled at login\n'
        printf '      Fix: systemctl --user enable fcitx5-layout-sync.service\n'
    fi
elif (( x11_session )); then
    fcitx_config="${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5/config"
    if [[ -f $fcitx_config ]] &&
       grep -qx 'ActiveByDefault=True' "$fcitx_config" &&
       grep -qx 'ShareInputState=All' "$fcitx_config" &&
       grep -qx 'EnumerateSkipFirst=True' "$fcitx_config" &&
       grep -qx '0=Alt+Shift_L' "$fcitx_config" &&
       grep -qx '1=Shift+Alt_L' "$fcitx_config" &&
       systemctl --user is-active --quiet fcitx5-layout-sync.service; then
        printf 'OK    Fcitx owns X11 Alt+Shift and skips keyboard-us\n'
    else
        printf 'FAIL  X11 Alt+Shift is not configured to stay inside SmartType\n'
        printf '      Fix: scripts/install-user.sh --enable-x11-layout-sync\n'
        fail=1
    fi
else
    printf 'OK    KDE layout synchronizer is not required on %s/%s\n' \
        "${desktop_name:-this desktop}" "${session_type:-unknown}"
fi

process_environment=""
if [[ -n $fcitx_pid && -r /proc/$fcitx_pid/environ ]]; then
    process_environment=$(tr '\0' '\n' < "/proc/$fcitx_pid/environ")
fi
running_addon_dir=""
for candidate in "${addon_dirs[@]}"; do
    if [[ $process_environment == *"$candidate"* ]]; then
        running_addon_dir=$candidate
        break
    fi
done
if [[ -n $running_addon_dir ]]; then
    printf 'OK    running fcitx5 sees the user addon directory\n'
else
    printf 'WARN  running fcitx5 has old environment; logout/login is still required\n'
fi

fcitx_session_type=""
if [[ -n $process_environment ]]; then
    fcitx_session_type=$(sed -n 's/^XDG_SESSION_TYPE=//p' <<< "$process_environment" | head -n1)
fi
if [[ $fcitx_session_type == wayland ]] && command -v kreadconfig6 >/dev/null 2>&1; then
    kwin_input_method=$(kreadconfig6 --file kwinrc --group Wayland --key InputMethod 2>/dev/null || true)
    if [[ $kwin_input_method == */org.fcitx.Fcitx5.desktop ]]; then
        printf 'OK    KWin is configured to own Fcitx Wayland input-method startup\n'
    else
        printf 'FAIL  KWin Wayland InputMethod is not the Fcitx desktop file\n'
        printf '      Fix: scripts/install-user.sh --enable-kde-wayland-ime\n'
        printf '      Then logout/login; do not restart Fcitx from its tray\n'
        fail=1
    fi
fi

if [[ -n $fcitx_pid ]] && [[ $(fcitx5-remote -n 2>/dev/null) == smarttype* ]]; then
    printf 'OK    SmartType is the active input method\n'
else
    printf 'WARN  SmartType is not active; select it in fcitx5-configtool\n'
fi

# ST-028 / ST-018: session IM must stay Fcitx-compatible for Qt Wayland
# (Telegram/AyuGram). QT_IM_MODULE=xim + XMODIFIERS=@im=none strips Preedit
# and SurroundingText; doctor should catch regressions after reboot/login.
user_env=$(systemctl --user show-environment 2>/dev/null || true)
qt_im=""
gtk_im=""
xmodifiers=""
if [[ -n $user_env ]]; then
    while IFS= read -r line; do
        case "$line" in
            QT_IM_MODULE=*) qt_im=${line#QT_IM_MODULE=} ;;
            GTK_IM_MODULE=*) gtk_im=${line#GTK_IM_MODULE=} ;;
            XMODIFIERS=*) xmodifiers=${line#XMODIFIERS=} ;;
        esac
    done <<< "$user_env"
fi

if (( x11_session )); then
    if [[ ${qt_im,,} == fcitx ]]; then
        printf 'OK    X11 session QT_IM_MODULE uses fcitx (inline application preedit)\n'
    else
        printf 'FAIL  X11 session QT_IM_MODULE=%s (expected fcitx)\n' "${qt_im:-unset}"
        printf '      Fix: scripts/install-user.sh --enable-x11-layout-sync and relogin\n'
        fail=1
    fi
    if [[ ${gtk_im,,} == fcitx ]]; then
        printf 'OK    X11 session GTK_IM_MODULE uses fcitx (inline application preedit)\n'
    else
        printf 'FAIL  X11 session GTK_IM_MODULE=%s (expected fcitx)\n' "${gtk_im:-unset}"
        printf '      Fix: scripts/install-user.sh --enable-x11-layout-sync and relogin\n'
        fail=1
    fi
elif [[ ${qt_im,,} == xim ]]; then
    printf 'FAIL  session QT_IM_MODULE=xim (Telegram loses Preedit/SurroundingText)\n'
    printf '      Fix: systemctl --user unset-environment QT_IM_MODULE\n'
    printf '      Ensure ~/.config/autostart/imsettings-start.desktop has Hidden=true\n'
    printf '      See ST-018 / docs/RUNBOOK.md (session IM durability)\n'
    fail=1
elif [[ -n $qt_im ]]; then
    printf 'WARN  session QT_IM_MODULE=%s (prefer unset on Wayland for Qt apps)\n' "$qt_im"
else
    printf 'OK    session QT_IM_MODULE is unset (not xim)\n'
fi

case "$xmodifiers" in
    *@im=none*|@im=none*)
        printf 'FAIL  session XMODIFIERS=%s (imsettings none)\n' "$xmodifiers"
        printf '      Fix: ensure environment.d sets XMODIFIERS=@im=fcitx and relogin\n'
        fail=1
        ;;
    *@im=fcitx*|@im=fcitx*)
        printf 'OK    session XMODIFIERS uses fcitx (%s)\n' "$xmodifiers"
        ;;
    "")
        printf 'WARN  session XMODIFIERS unset; Wayland apps may still work via Virtual Keyboard\n'
        ;;
    *)
        printf 'WARN  session XMODIFIERS=%s (expected @im=fcitx or @im=fcitx5)\n' "$xmodifiers"
        ;;
esac

imsettings_desktop="${XDG_CONFIG_HOME:-$HOME/.config}/autostart/imsettings-start.desktop"
if (( x11_session )) && [[ ${qt_im,,} == fcitx && ${gtk_im,,} == fcitx &&
                           $xmodifiers == *@im=fcitx* ]]; then
    printf 'OK    managed X11 toolkit environment prevents im-config fallback\n'
elif [[ -f $imsettings_desktop ]]; then
    if grep -Eqi '^[[:space:]]*Hidden[[:space:]]*=[[:space:]]*true[[:space:]]*$' "$imsettings_desktop"; then
        printf 'OK    imsettings-start autostart is Hidden (ST-018)\n'
    else
        printf 'FAIL  imsettings-start.desktop present without Hidden=true\n'
        printf '      Fix: add Hidden=true (and optionally X-GNOME-Autostart-enabled=false)\n'
        fail=1
    fi
else
    printf 'WARN  no user override for imsettings-start.desktop; imsettings may force xim on login\n'
    printf '      Optional: create ~/.config/autostart/imsettings-start.desktop with Hidden=true\n'
fi

if (( x11_session )) && [[ ${qt_im,,} == fcitx && ${gtk_im,,} == fcitx &&
                           $xmodifiers == *@im=fcitx* ]]; then
    printf 'OK    ~/.xinputrc is not required with the managed X11 environment\n'
elif [[ -e $HOME/.xinputrc ]]; then
    if grep -Eqs 'fcitx5?|XINPUT_PROFILE' "$HOME/.xinputrc" 2>/dev/null ||
       [[ $(readlink -f "$HOME/.xinputrc" 2>/dev/null) == *fcitx* ]]; then
        printf 'OK    ~/.xinputrc points at fcitx\n'
    else
        printf 'WARN  ~/.xinputrc exists but does not clearly select fcitx5\n'
    fi
else
    # Not fatal when environment.d + Hidden imsettings keep the session healthy.
    printf 'WARN  ~/.xinputrc missing (optional ST-018 hardening if imsettings re-engages)\n'
fi

exit "$fail"
