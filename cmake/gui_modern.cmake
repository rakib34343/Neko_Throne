# cmake/gui_modern.cmake
# Modern GUI modules for Throne — included from root CMakeLists.txt

set(GUI_MODERN_SOURCES
    # Core UI infrastructure
    include/ui/core/TranslationManager.hpp
    src/ui/core/TranslationManager.cpp
    include/ui/core/TitleBar.hpp
    src/ui/core/TitleBar.cpp
    include/ui/core/AsyncBackendBridge.hpp
    src/ui/core/AsyncBackendBridge.cpp
    include/ui/core/SpeedGraph.hpp
    src/ui/core/SpeedGraph.cpp

    # Model-View models
    include/ui/models/ServerListModel.hpp
    src/ui/models/ServerListModel.cpp
    include/ui/models/LogModel.hpp
    src/ui/models/LogModel.cpp
    include/ui/models/RoutingRulesModel.hpp
    src/ui/models/RoutingRulesModel.cpp

    # Platform abstractions
    include/sys/platform/PlatformTray.hpp
    src/sys/platform/PlatformTray.cpp
    include/sys/platform/SystemThemeWatcher.hpp
    src/sys/platform/SystemThemeWatcher.cpp
)

# Platform-specific title bar backends
if (WIN32)
    list(APPEND GUI_MODERN_SOURCES src/sys/platform/TitleBar_win.cpp)
else ()
    list(APPEND GUI_MODERN_SOURCES src/sys/platform/TitleBar_linux.cpp)
endif()

# Needed for QtConcurrent in AsyncBackendBridge
find_package(Qt6 REQUIRED COMPONENTS Concurrent)
