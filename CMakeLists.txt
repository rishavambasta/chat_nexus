cmake_minimum_required(VERSION 3.16)
project(P2PChat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Default to user-specified Qt directory if CMAKE_PREFIX_PATH not set
if(NOT DEFINED CMAKE_PREFIX_PATH AND EXISTS "/run/media/rishav/DS1/qt/qt5.15/5.14.2/gcc_64")
    set(CMAKE_PREFIX_PATH "/run/media/rishav/DS1/qt/qt5.15/5.14.2/gcc_64")
endif()

find_package(Qt5 COMPONENTS Core Widgets Network Sql REQUIRED)

set(SOURCES
    src/main.cpp
    src/db/DatabaseManager.cpp
    src/network/PeerConnection.cpp
    src/network/NetworkManager.cpp
    src/ui/MainWindow.cpp
    src/ui/PeerListWidget.cpp
    src/ui/ChatAreaWidget.cpp
    src/ui/MessageBubbleWidget.cpp
    src/ui/FileTransferBubbleWidget.cpp
    src/ui/AddPeerDialog.cpp
    src/ui/FirstLaunchDialog.cpp
    src/ui/SettingsDialog.cpp
)

set(HEADERS
    src/models/Peer.h
    src/models/Message.h
    src/models/FileTransfer.h
    src/network/Protocol.h
    src/db/DatabaseManager.h
    src/network/PeerConnection.h
    src/network/NetworkManager.h
    src/ui/MainWindow.h
    src/ui/PeerListWidget.h
    src/ui/ChatAreaWidget.h
    src/ui/MessageBubbleWidget.h
    src/ui/FileTransferBubbleWidget.h
    src/ui/AddPeerDialog.h
    src/ui/FirstLaunchDialog.h
    src/ui/SettingsDialog.h
)

set(RESOURCES
    resources/p2pchat.qrc
)

add_executable(P2PChat ${SOURCES} ${HEADERS} ${RESOURCES})

target_link_libraries(P2PChat PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    Qt5::Sql
)

# Tests
enable_testing()
find_package(Qt5 COMPONENTS Test REQUIRED)
add_executable(test_p2p
    tests/test_p2p.cpp
    src/db/DatabaseManager.cpp
    src/network/PeerConnection.cpp
    src/network/NetworkManager.cpp
)
target_link_libraries(test_p2p PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    Qt5::Sql
    Qt5::Test
)
add_test(NAME test_p2p COMMAND test_p2p)
