cmake_minimum_required(VERSION 3.16)
project(OyNIx VERSION 3.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Find Qt6 ────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets WebEngineWidgets WebEngineCore WebEngineQuick
    Network Sql Concurrent WebChannel
)

# Optional multimedia
find_package(Qt6 COMPONENTS Multimedia QUIET)

# ── SQLite3 (system or bundled) ─────────────────────────────────────
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 QUIET sqlite3)
endif()
if(NOT SQLITE3_FOUND)
    find_path(SQLITE3_INCLUDE_DIRS sqlite3.h)
    find_library(SQLITE3_LIBRARIES sqlite3)
endif()

# ── Source files ────────────────────────────────────────────────────
set(OYNIX_SOURCES
    src/main.cpp
    # Core
    src/core/BrowserWindow.cpp
    src/core/BrowserWindow.h
    src/core/TabWidget.cpp
    src/core/TabWidget.h
    src/core/WebView.cpp
    src/core/WebView.h
    src/core/WebPage.cpp
    src/core/WebPage.h
    src/core/UrlBar.cpp
    src/core/UrlBar.h
    src/core/FindBar.cpp
    src/core/FindBar.h
    # Interop (C# NativeAOT bridge)
    src/interop/CoreBridge.cpp
    src/interop/CoreBridge.h
    # AI
    src/ai/AiManager.cpp
    src/ai/AiManager.h
    src/ai/AiChatPanel.cpp
    src/ai/AiChatPanel.h
    # UI
    src/ui/CommandPalette.cpp
    src/ui/CommandPalette.h
    src/ui/DownloadManager.cpp
    src/ui/DownloadManager.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/TreeTabSidebar.cpp
    src/ui/TreeTabSidebar.h
    src/ui/BookmarkPanel.cpp
    src/ui/BookmarkPanel.h
    src/ui/ExtensionPanel.cpp
    src/ui/ExtensionPanel.h
    # Media
    src/media/AudioPlayer.cpp
    src/media/AudioPlayer.h
    # Theme
    src/theme/ThemeEngine.cpp
    src/theme/ThemeEngine.h
    # Data
    src/data/Database.cpp
    src/data/Database.h
    src/data/BookmarkManager.cpp
    src/data/BookmarkManager.h
    src/data/HistoryManager.cpp
    src/data/HistoryManager.h
    src/data/ProfileManager.cpp
    src/data/ProfileManager.h
    src/data/CredentialManager.cpp
    src/data/CredentialManager.h
    src/data/ChromeImport.cpp
    src/data/ChromeImport.h
    src/data/NydtaFormat.cpp
    src/data/NydtaFormat.h
    src/data/SessionManager.cpp
    src/data/SessionManager.h
    # Search
    src/search/NyxSearch.cpp
    src/search/NyxSearch.h
    src/search/WebResultsFetcher.cpp
    src/search/WebResultsFetcher.h
    src/search/SearchEngineBuilder.cpp
    src/search/SearchEngineBuilder.h
    # Security
    src/security/SecurityManager.cpp
    src/security/SecurityManager.h
    # Extensions
    src/extensions/ExtensionManager.cpp
    src/extensions/ExtensionManager.h
    src/extensions/ExtensionBridge.cpp
    src/extensions/ExtensionBridge.h
    src/extensions/NpiCompiler.cpp
    src/extensions/NpiCompiler.h
    # Sync
    src/sync/GitHubSync.cpp
    src/sync/GitHubSync.h
    # Pages
    src/pages/InternalPages.cpp
    src/pages/InternalPages.h
)

# ── Qt Resources ────────────────────────────────────────────────────
qt6_add_resources(OYNIX_RESOURCES resources/oynix.qrc)

# ── Build executable ────────────────────────────────────────────────
if(WIN32)
    add_executable(oynix WIN32 ${OYNIX_SOURCES} ${OYNIX_RESOURCES})
else()
    add_executable(oynix ${OYNIX_SOURCES} ${OYNIX_RESOURCES})
endif()

target_include_directories(oynix PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${SQLITE3_INCLUDE_DIRS}
)

target_link_libraries(oynix PRIVATE
    Qt6::Core Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebEngineCore
    Qt6::WebEngineQuick Qt6::Network Qt6::Sql Qt6::Concurrent
    Qt6::WebChannel
    ${SQLITE3_LIBRARIES}
    dl  # for dlopen/dlsym (CoreBridge)
)

if(Qt6Multimedia_FOUND)
    target_link_libraries(oynix PRIVATE Qt6::Multimedia)
    target_compile_definitions(oynix PRIVATE HAS_MULTIMEDIA)
endif()

# ── C# NativeAOT Core Library ──────────────────────────────────────
# Build the C# core as a native shared library
set(DOTNET_DIR "${CMAKE_SOURCE_DIR}/core/OyNIx.Core")
set(DOTNET_OUTPUT "${CMAKE_BINARY_DIR}/dotnet")

add_custom_command(
    OUTPUT "${DOTNET_OUTPUT}/OyNIxCore.so"
    COMMAND ${CMAKE_COMMAND} -E env PATH="/opt/dotnet:$ENV{PATH}"
        dotnet publish "${DOTNET_DIR}/OyNIx.Core.csproj"
        -c Release -r linux-x64 -o "${DOTNET_OUTPUT}"
    WORKING_DIRECTORY "${DOTNET_DIR}"
    DEPENDS "${DOTNET_DIR}/OyNIx.Core.csproj"
    COMMENT "Building C# NativeAOT core library"
)

add_custom_target(dotnet_core DEPENDS "${DOTNET_OUTPUT}/OyNIxCore.so")
add_dependencies(oynix dotnet_core)

# Copy the .so next to the executable for runtime loading
add_custom_command(TARGET oynix POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dotnet"
    COMMENT "C# core library available at ${DOTNET_OUTPUT}/OyNIxCore.so"
)

# ── Install ─────────────────────────────────────────────────────────
install(TARGETS oynix DESTINATION bin)
install(FILES "${DOTNET_OUTPUT}/OyNIxCore.so" DESTINATION lib)
install(DIRECTORY themes/ DESTINATION share/oynix/themes)

# Platform-specific
if(UNIX AND NOT APPLE)
    install(FILES resources/oynix.desktop DESTINATION share/applications)
    install(FILES resources/oynix.png DESTINATION share/icons/hicolor/256x256/apps)
endif()

message(STATUS "OyNIx Browser v${PROJECT_VERSION}")
message(STATUS "  Qt6: ${Qt6_VERSION}")
message(STATUS "  SQLite3: ${SQLITE3_LIBRARIES}")
message(STATUS "  C# Core: NativeAOT (.NET 8)")
if(Qt6Multimedia_FOUND)
    message(STATUS "  Multimedia: YES")
else()
    message(STATUS "  Multimedia: NO (audio player disabled)")
endif()
