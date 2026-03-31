# minimum cmake version required
cmake_minimum_required(VERSION 3.16)

# define the project name and language
project(GeminiCliGui VERSION 1.0 LANGUAGES CXX)

# enforce c++17 standard for modern architecture
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# enable qt's automated build tools
set(CMAKE_AUTOMOC ON) # processes Q_OBJECT macros
set(CMAKE_AUTORCC ON) # processes resource files (.qrc)
set(CMAKE_AUTOUIC ON) # processes designer files (.ui)

# locate the qt6 libraries on the local system
# note: if cmake fails to find qt, you will need to set CMAKE_PREFIX_PATH
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql)

# define the executable and its source files
add_executable(GeminiCliGui
    main.cpp
    main_window.cpp
    gemini_api_client.cpp
    agent_manager.cpp
    settings_dialog.cpp
)

# link the required qt modules to the executable
target_link_libraries(GeminiCliGui PRIVATE 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)