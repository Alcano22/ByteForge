#include <Engine/Core/Log.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include <cstdlib>

int main(int argc, char* argv[])
{
    ByteForge::Log::Init();

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ByteForge Hub"));
    QGuiApplication::setOrganizationName(QStringLiteral("ByteForge"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);

    engine.loadFromModule("ByteForge.Hub", "Main");

    return QGuiApplication::exec();
}
