#include <QApplication>
#include <QFile>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QDir>
#include "db/DatabaseManager.h"
#include "network/NetworkManager.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("P2PChat");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("P2P");

    // Command line parser for testing convenience
    QCommandLineParser parser;
    parser.setApplicationDescription("P2P Chat and File Exchange");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(QStringList() << "p" << "port", "Local listening port (default: 42424)", "port");
    QCommandLineOption dbOption(QStringList() << "d" << "db", "Database file path (default: p2p_chat.db)", "db");
    QCommandLineOption aliasOption(QStringList() << "a" << "alias", "Set user alias directly", "alias");

    parser.addOption(portOption);
    parser.addOption(dbOption);
    parser.addOption(aliasOption);
    parser.process(app);

    // Apply Dark Theme
    QFile qssFile(":/styles/dark_theme.qss");
    if (qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(qssFile.readAll()));
        qssFile.close();
    } else {
        // Fallback to local filesystem path
        QFile localQss("resources/styles/dark_theme.qss");
        if (localQss.open(QIODevice::ReadOnly | QIODevice::Text)) {
            app.setStyleSheet(QString::fromUtf8(localQss.readAll()));
            localQss.close();
        }
    }

    // 1. Initialize SQLite Database
    QString dbPath = parser.isSet(dbOption) ? parser.value(dbOption) : "p2p_chat.db";
    if (!DatabaseManager::instance().initialize(dbPath)) {
        qCritical() << "Fatal: Failed to initialize SQLite database at" << dbPath;
        return 1;
    }

    // If alias passed via CLI, set it
    if (parser.isSet(aliasOption)) {
        DatabaseManager::instance().setUserAlias(parser.value(aliasOption));
    }

    // 2. Determine listen port
    quint16 listenPort = 42424;
    if (parser.isSet(portOption)) {
        bool ok = false;
        quint16 p = parser.value(portOption).toUShort(&ok);
        if (ok && p > 0) {
            listenPort = p;
        }
    } else {
        listenPort = DatabaseManager::instance().getListenPort();
    }

    // 3. Start P2P TCP Server
    if (!NetworkManager::instance().startServer(listenPort)) {
        qWarning() << "Warning: Could not bind TCP server to port" << listenPort;
    }

    // 4. Start Background Polling (default 15 seconds)
    int pollSec = DatabaseManager::instance().getPollingInterval();
    NetworkManager::instance().startPolling(pollSec);

    // 5. Create and Launch Main Window
    MainWindow mainWindow;
    mainWindow.checkFirstLaunch();
    mainWindow.show();

    return app.exec();
}
