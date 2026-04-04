# minimum cmake version required
cmake_minimum_required(VERSION 3.16)

# define the project name and language
project(GeminiNativeAgent VERSION 1.1.0 LANGUAGES CXX)

# set project output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# enforce c++17 standard for modern architecture
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# tell cmake to generate the json map that IDE linters need to find headers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# enable qt's automated build tools
set(CMAKE_AUTOMOC ON) # processes Q_OBJECT macros
set(CMAKE_AUTORCC ON) # processes resource files (.qrc)
set(CMAKE_AUTOUIC ON) # processes designer files (.ui)

# locate the qt6 libraries on the local system
# note: if cmake fails to find qt, you will need to set CMAKE_PREFIX_PATH
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql)

# Override specifically for RELEASE builds
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/release/${PROJECT_VERSION})

# define the executable and its source files using relative paths
add_executable(GeminiNativeAgent WIN32
    main.cpp
    main_window.cpp
    
    # Agent Core & API
    agent/gemini_api_client.cpp
    agent/agent_manager.cpp
    agent/agent_prompt_builder.cpp
    agent/gemini_payload_builder.cpp
    
    # Settings & Theme
    settings/settings_dialog.cpp
    settings/theme_manager.h
    
    # Data & Sessions
    data/session_dialog.cpp
    data/database_manager.cpp
    data/chat_formatter.cpp
    
    # Tools & Actions
    tools/base_agent_action.h
    tools/tool_schema_provider.cpp
    tools/write_file_action.cpp
    tools/ftp_upload_action.cpp
    tools/execute_shell_action.cpp
    tools/take_screenshot_action.cpp
    tools/git_manager_action.cpp
)

# --- CRITICAL ADDITION ---
# Tell CMake to search these subdirectories for Header (.h) files.
# This prevents you from having to rewrite every #include in your .cpp files!
target_include_directories(GeminiNativeAgent PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/agent
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
    ${CMAKE_CURRENT_SOURCE_DIR}/data
    ${CMAKE_CURRENT_SOURCE_DIR}/settings
)

# --- AUTOMATIC QT DLL DEPLOYMENT ---

# 1. Find the path to Qt's built-in deployment tool
if(TARGET Qt6::qmake)
    get_target_property(QMAKE_EXE Qt6::qmake IMPORTED_LOCATION)
elseif(TARGET Qt5::qmake)
    get_target_property(QMAKE_EXE Qt5::qmake IMPORTED_LOCATION)
endif()

get_filename_component(QT_BIN_DIR "${QMAKE_EXE}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

# 2. Tell CMake to run windeployqt on the compiled .exe immediately after a successful build
if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET GeminiNativeAgent POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
        # We add flags to prevent it from copying unnecessary bloat (like foreign language translations)
        ARGS --no-translations --no-opengl-sw "$<TARGET_FILE:GeminiNativeAgent>"
        COMMENT "Running windeployqt to automatically copy required Qt DLLs..."
    )
else()
    message(WARNING "windeployqt tool not found! Qt DLLs will not be copied automatically.")
endif()

# link the required qt modules to the executable
target_link_libraries(GeminiNativeAgent PRIVATE 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)