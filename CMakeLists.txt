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

# ── SQLite3 (optional — Qt6::Sql provides SQLite driver) ──────────
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
    src/ui/CrawlerPanel.cpp
    src/ui/CrawlerPanel.h
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
    src/search/WebCrawler.cpp
    src/search/WebCrawler.h
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
)

# Add SQLite3 include dirs only if found
if(SQLITE3_INCLUDE_DIRS)
    target_include_directories(oynix PRIVATE ${SQLITE3_INCLUDE_DIRS})
endif()

# ── Link libraries (platform-aware) ────────────────────────────────
target_link_libraries(oynix PRIVATE
    Qt6::Core Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebEngineCore
    Qt6::WebEngineQuick Qt6::Network Qt6::Sql Qt6::Concurrent
    Qt6::WebChannel
)

# SQLite3 — optional, Qt6::Sql already bundles an SQLite driver
if(SQLITE3_LIBRARIES)
    target_link_libraries(oynix PRIVATE ${SQLITE3_LIBRARIES})
endif()

# dlopen/dlsym — Linux/macOS only (Windows uses LoadLibrary via Win32 API)
if(UNIX)
    target_link_libraries(oynix PRIVATE dl)
endif()

if(Qt6Multimedia_FOUND)
    target_link_libraries(oynix PRIVATE Qt6::Multimedia)
    target_compile_definitions(oynix PRIVATE HAS_MULTIMEDIA)
endif()

# ── C# NativeAOT Core Library ──────────────────────────────────────
# Build the C# core as a native shared library.
# Detects platform to produce .so (Linux), .dylib (macOS), or .dll (Windows).
set(DOTNET_DIR "${CMAKE_SOURCE_DIR}/core/OyNIx.Core")
set(DOTNET_OUTPUT "${CMAKE_BINARY_DIR}/dotnet")

if(WIN32)
    set(DOTNET_RID "win-x64")
    set(DOTNET_LIB_NAME "OyNIxCore.dll")
elseif(APPLE)
    set(DOTNET_RID "osx-x64")
    set(DOTNET_LIB_NAME "OyNIxCore.dylib")
else()
    set(DOTNET_RID "linux-x64")
    set(DOTNET_LIB_NAME "OyNIxCore.so")
endif()

# Find dotnet CLI
find_program(DOTNET_EXE dotnet
    HINTS "/opt/dotnet" "$ENV{DOTNET_ROOT}" "$ENV{HOME}/.dotnet"
    PATHS "/usr/bin" "/usr/local/bin" "/usr/share/dotnet"
)
if(NOT DOTNET_EXE)
    set(DOTNET_EXE "dotnet")
endif()

# Use --output instead of -o to avoid MSBuild argument parsing issues
# with newer .NET SDK versions (10+).
add_custom_command(
    OUTPUT "${DOTNET_OUTPUT}/${DOTNET_LIB_NAME}"
    COMMAND ${DOTNET_EXE} publish
        "${DOTNET_DIR}/OyNIx.Core.csproj"
        -c Release
        -r ${DOTNET_RID}
        --output "${DOTNET_OUTPUT}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS "${DOTNET_DIR}/OyNIx.Core.csproj"
    COMMENT "Building C# NativeAOT core library (${DOTNET_RID})"
)

add_custom_target(dotnet_core DEPENDS "${DOTNET_OUTPUT}/${DOTNET_LIB_NAME}")
add_dependencies(oynix dotnet_core)

# ── Install ─────────────────────────────────────────────────────────
install(TARGETS oynix DESTINATION bin)
install(FILES "${DOTNET_OUTPUT}/${DOTNET_LIB_NAME}" DESTINATION lib/oynix)
if(EXISTS "${CMAKE_SOURCE_DIR}/themes")
    install(DIRECTORY themes/ DESTINATION share/oynix/themes)
endif()

# Platform-specific (Linux)
if(UNIX AND NOT APPLE)
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/oynix.desktop")
        install(FILES resources/oynix.desktop DESTINATION share/applications)
    endif()

    # Icons — install all available sizes from assets/
    foreach(SIZE 256 128 64 48)
        set(_icon "${CMAKE_SOURCE_DIR}/assets/icon-${SIZE}.png")
        if(EXISTS "${_icon}")
            install(FILES "${_icon}"
                DESTINATION "share/icons/hicolor/${SIZE}x${SIZE}/apps"
                RENAME oynix.png)
        endif()
    endforeach()
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/icon.svg")
        install(FILES "${CMAKE_SOURCE_DIR}/assets/icon.svg"
            DESTINATION share/icons/hicolor/scalable/apps
            RENAME oynix.svg)
    endif()
endif()

message(STATUS "OyNIx Browser v${PROJECT_VERSION}")
message(STATUS "  Qt6: ${Qt6_VERSION}")
message(STATUS "  SQLite3: ${SQLITE3_LIBRARIES}")
message(STATUS "  C# Core: NativeAOT (.NET 8) → ${DOTNET_LIB_NAME}")
message(STATUS "  Platform RID: ${DOTNET_RID}")
if(Qt6Multimedia_FOUND)
    message(STATUS "  Multimedia: YES")
else()
    message(STATUS "  Multimedia: NO (audio player disabled)")
endif()
