cmake_minimum_required(VERSION 3.20)

project(Throne VERSION 0.5.0)

# ═══════════════════════════════════════════════════════════════════════════════
# Strict amd64 enforcement — MUST be checked before any compilation targets
# ═══════════════════════════════════════════════════════════════════════════════
include("cmake/arch_enforce.cmake")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Qt 6 — strictly required, no Qt 5 fallback
# ═══════════════════════════════════════════════════════════════════════════════
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent LinguistTools)
if (NOT WIN32)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()

message(STATUS "[Throne] Qt6 version: ${Qt6_VERSION}")
if (Qt6_VERSION VERSION_LESS "6.5.0")
    message(WARNING "Throne targets Qt 6.5+. Detected Qt ${Qt6_VERSION} — some features may be unavailable.")
endif()

# Detect static Qt for single-executable builds
get_target_property(_qt_type Qt6::Core TYPE)
if (_qt_type STREQUAL "STATIC_LIBRARY")
    set(QT_STATIC ON)
    message(STATUS "[Throne] Static Qt detected — single executable mode")
else()
    set(QT_STATIC OFF)
endif()

if (NKR_CROSS)
    set_property(TARGET Qt6::moc PROPERTY IMPORTED_LOCATION /usr/bin/moc)
    set_property(TARGET Qt6::uic PROPERTY IMPORTED_LOCATION /usr/bin/uic)
    set_property(TARGET Qt6::rcc PROPERTY IMPORTED_LOCATION /usr/bin/rcc)
    set_property(TARGET Qt6::lrelease PROPERTY IMPORTED_LOCATION /usr/bin/lrelease)
    set_property(TARGET Qt6::lupdate PROPERTY IMPORTED_LOCATION /usr/bin/lupdate)
endif ()

#### Platform Variables ####
if (WIN32)
    include("cmake/windows/windows.cmake")
else ()
    include("cmake/linux/linux.cmake")
endif ()

#### For Dumps ####
if (NOT MSVC)
    add_compile_options(-fno-omit-frame-pointer)
endif()

#### default prefix path ####

message("[CMAKE_PREFIX_PATH] ${CMAKE_PREFIX_PATH}")

# for some cross toolchain
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_PREFIX_PATH})
message("[CMAKE_FIND_ROOT_PATH] ${CMAKE_FIND_ROOT_PATH}")

#### NKR ####

include("cmake/print.cmake")
include("cmake/nkr.cmake")
include("cmake/QHotkey.cmake")
include("cmake/gui_modern.cmake")

find_package(Threads)

if (NKR_PACKAGE)
    nkr_add_compile_definitions(NKR_CPP_USE_APPDATA)
endif ()

include("cmake/myproto.cmake")
list(APPEND NKR_EXTERNAL_TARGETS myproto)

set(BUILD_SHARED_LIBS OFF)
list(APPEND NKR_EXTERNAL_TARGETS qhotkey)

# Sources
set(PROJECT_SOURCES
        ${PLATFORM_SOURCES}
        ${GUI_MODERN_SOURCES}

        src/main.cpp
        src/global/Configs.cpp
        src/global/Utils.cpp
        src/global/HTTPRequestHelper.cpp
        src/global/DeviceDetailsHelper.cpp
        src/global/CountryHelper.cpp

        3rdparty/base64.cpp
        3rdparty/qrcodegen.cpp
        3rdparty/QtExtKeySequenceEdit.cpp
        3rdparty/QrDecoder.cpp

        3rdparty/qv2ray/v2/ui/LogHighlighter.cpp
        3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.cpp
        3rdparty/qv2ray/v2/ui/widgets/common/QJsonModel.cpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.cpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.ui
        3rdparty/qv2ray/v2/ui/widgets/speedchart/SpeedWidget.cpp
        3rdparty/qv2ray/v2/ui/widgets/speedchart/SpeedWidget.hpp
        3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.cpp

        3rdparty/quirc/decode.c
        3rdparty/quirc/identify.c
        3rdparty/quirc/quirc.c
        3rdparty/quirc/version_db.c

        src/api/RPC.cpp
        src/api/CoreVersionParser.cpp
        include/api/CoreVersionParser.hpp

        include/sys/ProxyStateManager.hpp
        src/sys/ProxyStateManager.cpp
        include/sys/NetworkLeakGuard.hpp
        src/sys/NetworkLeakGuard.cpp

        include/sys/CoreManager.hpp
        src/sys/CoreManager.cpp

        include/configs/NextGenFeatures.hpp
        include/global/MemorySafety.hpp
        include/global/ArchGuard.hpp

        src/dataStore/Database.cpp
        src/stats/traffic/TrafficLooper.cpp
        src/dataStore/ProfileFilter.cpp

        src/configs/proxy/AbstractBean.cpp
        src/configs/proxy/Bean2CoreObj_box.cpp
        src/configs/proxy/Bean2Link.cpp
        src/configs/proxy/Link2Bean.cpp
        include/configs/proxy/ChainBean.hpp # translate

        include/configs/sub/GroupUpdater.hpp
        src/configs/sub/GroupUpdater.cpp

        include/sys/Process.hpp
        src/sys/Process.cpp
        src/sys/AutoRun.cpp

        include/ui/setting/ThemeManager.hpp
        src/ui/setting/ThemeManager.cpp
        src/ui/setting/Icon.cpp

        src/ui/mainwindow_rpc.cpp
        src/ui/mainwindow.cpp
        include/ui/mainwindow.h
        include/ui/mainwindow.ui

        include/ui/profile/dialog_edit_profile.h
        src/ui/profile/dialog_edit_profile.cpp
        include/ui/profile/dialog_edit_profile.ui
        include/ui/group/dialog_edit_group.h
        src/ui/group/dialog_edit_group.cpp
        include/ui/group/dialog_edit_group.ui

        include/ui/profile/edit_chain.h
        src/ui/profile/edit_chain.cpp
        include/ui/profile/edit_chain.ui
        include/ui/profile/edit_shadowsocks.h
        src/ui/profile/edit_shadowsocks.cpp
        include/ui/profile/edit_shadowsocks.ui
        include/ui/profile/edit_vmess.h
        src/ui/profile/edit_vmess.cpp
        include/ui/profile/edit_vmess.ui
        include/ui/profile/edit_vless.h
        src/ui/profile/edit_vless.cpp
        include/ui/profile/edit_vless.ui
        include/ui/profile/edit_anytls.h
        src/ui/profile/edit_anytls.cpp
        include/ui/profile/edit_anytls.ui
        include/ui/profile/edit_tailscale.h
        src/ui/profile/edit_tailscale.cpp
        include/ui/profile/edit_tailscale.ui
        include/configs/proxy/Tailscale.hpp
        include/ui/profile/edit_trojan.h
        src/ui/profile/edit_trojan.cpp
        include/ui/profile/edit_trojan.ui
        include/ui/profile/edit_http.h
        src/ui/profile/edit_http.cpp
        include/ui/profile/edit_http.ui
        include/ui/profile/edit_socks.h
        src/ui/profile/edit_socks.cpp
        include/ui/profile/edit_socks.ui
        include/ui/profile/edit_hysteria.h
        src/ui/profile/edit_hysteria.cpp
        include/ui/profile/edit_hysteria.ui
        include/ui/profile/edit_tuic.h
        src/ui/profile/edit_tuic.cpp
        include/ui/profile/edit_tuic.ui

        include/ui/profile/edit_custom.h
        src/ui/profile/edit_custom.cpp
        include/ui/profile/edit_custom.ui

        include/ui/profile/edit_extra_core.h
        src/ui/profile/edit_extra_core.cpp
        include/ui/profile/edit_extra_core.ui

        include/ui/profile/edit_wireguard.h
        src/ui/profile/edit_wireguard.cpp
        include/ui/profile/edit_wireguard.ui

        src/ui/setting/dialog_basic_settings.cpp
        include/ui/setting/dialog_basic_settings.h
        include/ui/setting/dialog_basic_settings.ui

        src/ui/group/dialog_manage_groups.cpp
        include/ui/group/dialog_manage_groups.h
        include/ui/group/dialog_manage_groups.ui

        src/ui/setting/dialog_manage_routes.cpp
        include/ui/setting/dialog_manage_routes.h
        include/ui/setting/dialog_manage_routes.ui

        src/ui/setting/dialog_vpn_settings.cpp
        include/ui/setting/dialog_vpn_settings.h
        include/ui/setting/dialog_vpn_settings.ui

        src/ui/setting/dialog_hotkey.cpp
        include/ui/setting/dialog_hotkey.h
        include/ui/setting/dialog_hotkey.ui

        src/ui/profile/ProxyItem.cpp
        include/ui/profile/ProxyItem.h
        include/ui/profile/ProxyItem.ui
        src/ui/group/GroupItem.cpp
        include/ui/group/GroupItem.h
        include/ui/group/GroupItem.ui
        src/ui/setting/RouteItem.cpp
        include/ui/setting/RouteItem.h
        include/ui/setting/RouteItem.ui

        res/Throne.qrc
        ${QV2RAY_RC}
        include/dataStore/RouteEntity.h
        src/dataStore/RouteEntity.cpp
        src/ui/profile/edit_ssh.cpp
        include/ui/profile/edit_ssh.h
        include/ui/profile/edit_ssh.ui
        include/configs/proxy/SSHBean.h
        src/sys/linux/LinuxCap.cpp

        src/sys/AutoRun.cpp
        src/sys/Process.cpp
        include/ui/mainwindow_interface.h
        include/stats/connections/connectionLister.hpp
        src/stats/connectionLister/connectionLister.cpp
        src/configs/proxy/Json2Bean.cpp
        include/sys/windows/eventHandler.h
        src/dataStore/Group.cpp
        src/dataStore/ProxyEntity.cpp
        include/configs/baseConfig.h
        include/configs/common/DialFields.h
        include/configs/common/TLS.h
        include/configs/common/multiplex.h
        include/configs/common/transport.h
        include/configs/common/Outbound.h
        
        # common impl
        src/configs/common/DialFields.cpp
        src/configs/common/TLS.cpp
        src/configs/common/multiplex.cpp
        src/configs/common/transport.cpp
        src/configs/common/Outbound.cpp
        include/configs/outbounds/socks.h
        include/configs/outbounds/http.h
        include/configs/outbounds/shadowsocks.h
        include/configs/outbounds/vmess.h
        include/configs/outbounds/trojan.h
        include/configs/outbounds/hysteria.h
        include/configs/outbounds/vless.h
        include/configs/outbounds/tuic.h
        include/configs/outbounds/anyTLS.h
        include/configs/outbounds/ssh.h
        
        # outbounds impl
        src/configs/outbounds/anyTLS.cpp
        src/configs/outbounds/http.cpp
        src/configs/outbounds/hysteria.cpp
        src/configs/outbounds/shadowsocks.cpp
        src/configs/outbounds/socks.cpp
        src/configs/outbounds/ssh.cpp
        src/configs/outbounds/tailscale.cpp
        src/configs/outbounds/trojan.cpp
        src/configs/outbounds/tuic.cpp
        src/configs/outbounds/vless.cpp
        src/configs/outbounds/vmess.cpp
        src/configs/outbounds/wireguard.cpp
        include/configs/generate.h
        src/configs/generate.cpp
        include/configs/common/utils.h
        src/configs/common/utils.cpp
        include/configs/outbounds/custom.h
        include/configs/outbounds/chain.h
        include/configs/outbounds/extracore.h
        include/ui/profile/edit_socks.h
        src/ui/profile/edit_socks.cpp
        include/ui/profile/edit_socks.ui
        include/ui/profile/edit_http.h
        src/ui/profile/edit_http.cpp
        include/ui/profile/edit_http.ui
        include/ui/profile/edit_trojan.h
        src/ui/profile/edit_trojan.cpp
        include/ui/profile/edit_trojan.ui
        include/ui/profile/edit_hysteria.h
        src/ui/profile/edit_hysteria.cpp
        include/ui/profile/edit_hysteria.ui
        include/ui/profile/edit_tuic.h
        src/ui/profile/edit_tuic.cpp
        include/ui/profile/edit_tuic.ui
        include/ui/profile/edit_advanced.h
        src/ui/profile/edit_advanced.cpp
        include/ui/profile/edit_advanced.ui
        include/configs/common/xrayStreamSetting.h
        include/configs/outbounds/xrayVless.h
        include/configs/common/xrayMultiplex.h
        src/configs/common/xrayMultiplex.cpp
        src/configs/common/xrayStreamSetting.cpp
        src/configs/outbounds/xrayVless.cpp
        include/ui/profile/edit_xrayvless.h
        src/ui/profile/edit_xrayvless.cpp
        include/ui/profile/edit_xrayvless.ui
)

if (Qt6_VERSION VERSION_GREATER_EQUAL 6.9.0)
    list(APPEND PROJECT_SOURCES res/EmojiFont.qrc)
    if (WIN32)
        list(APPEND PROJECT_SOURCES res/EmojiFontWin.qrc)
    endif ()
endif ()

# Qt exe
qt_add_executable(Throne
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
)

# Target

set_property(TARGET Throne PROPERTY AUTOUIC ON)
set_property(TARGET Throne PROPERTY AUTOMOC ON)
set_property(TARGET Throne PROPERTY AUTORCC ON)


set_target_properties(Throne PROPERTIES
        OUTPUT_NAME "Neko_Throne"
        WIN32_EXECUTABLE TRUE
)

# Target Source Translations

set(TS_FILES
        res/translations/zh_CN.ts
        res/translations/fa_IR.ts
        res/translations/ru_RU.ts
)
set(LUPDATE_OPTIONS
        -locations none -no-obsolete
)
qt_add_lupdate(Throne TS_FILES ${TS_FILES} OPTIONS ${LUPDATE_OPTIONS})
qt_add_lrelease(Throne TS_FILES ${TS_FILES} QM_FILES_OUTPUT_VARIABLE QM_FILES)

# Deploy .qm files to lang/ next to the binary (NOT embedded in .qrc).
# TranslationManager loads them at runtime from applicationDirPath()/lang/.
set(LANG_OUTPUT_DIR "$<TARGET_FILE_DIR:Throne>/lang")
add_custom_command(TARGET Throne POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${LANG_OUTPUT_DIR}"
    COMMENT "[i18n] Creating lang/ output directory"
)
foreach(_qm_file ${QM_FILES})
    add_custom_command(TARGET Throne POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_qm_file}" "${LANG_OUTPUT_DIR}"
        COMMENT "[i18n] Deploying ${_qm_file} → lang/"
    )
endforeach()

# Target Link

target_link_libraries(Throne PRIVATE
        Qt6::Widgets Qt6::Network Qt6::Concurrent
        $<$<NOT:$<PLATFORM_ID:Windows>>:Qt6::DBus>
        Threads::Threads
        ${NKR_EXTERNAL_TARGETS}
        ${PLATFORM_LIBRARIES}
)

# Static Qt: import all required plugins so they're linked into the single binary
if (QT_STATIC)
    qt_import_plugins(Throne
        INCLUDE_BY_TYPE platforms Qt6::QWindowsIntegrationPlugin
        INCLUDE_BY_TYPE styles Qt6::QModernWindowsStylePlugin
        EXCLUDE_BY_TYPE imageformats
    )
    if (WIN32)
        target_link_libraries(Throne PRIVATE
            ws2_32 crypt32 winmm dwmapi uxtheme version imm32
        )
    endif()
endif()

qt_finalize_executable(Throne)
