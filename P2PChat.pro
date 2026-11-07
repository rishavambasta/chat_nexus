QT += core gui widgets network sql

CONFIG += c++17

TARGET = P2PChat
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/db/DatabaseManager.cpp \
    src/network/PeerConnection.cpp \
    src/network/NetworkManager.cpp \
    src/ui/MainWindow.cpp \
    src/ui/PeerListWidget.cpp \
    src/ui/ChatAreaWidget.cpp \
    src/ui/MessageBubbleWidget.cpp \
    src/ui/FileTransferBubbleWidget.cpp \
    src/ui/AddPeerDialog.cpp \
    src/ui/FirstLaunchDialog.cpp \
    src/ui/SettingsDialog.cpp

HEADERS += \
    src/models/Peer.h \
    src/models/Message.h \
    src/models/FileTransfer.h \
    src/network/Protocol.h \
    src/db/DatabaseManager.h \
    src/network/PeerConnection.h \
    src/network/NetworkManager.h \
    src/ui/MainWindow.h \
    src/ui/PeerListWidget.h \
    src/ui/ChatAreaWidget.h \
    src/ui/MessageBubbleWidget.h \
    src/ui/FileTransferBubbleWidget.h \
    src/ui/AddPeerDialog.h \
    src/ui/FirstLaunchDialog.h \
    src/ui/SettingsDialog.h

RESOURCES += \
    resources/p2pchat.qrc
