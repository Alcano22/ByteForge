#include <Engine/Core/Log.h>
#include <Engine/Core/RuntimePaths.h>

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include <array>
#include <cstdlib>

namespace
{
    QIcon MakeWindowIcon()
    {
        constexpr std::array Sizes{ 16, 24, 32, 48, 64, 128, 256 };

        QIcon icon;
        for (const int size : Sizes)
            icon.addFile(QStringLiteral(":/branding/%1/byteforge.png").arg(size), QSize(size, size));
        return icon;
    }
}

int main(int argc, char* argv[])
{
    ByteForge::Log::Init();

    ByteForge::RuntimePaths::SetLayout(ByteForge::RuntimePaths::Layout::Install);

#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME"))
        qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
#endif

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ByteForge Hub"));
    QGuiApplication::setOrganizationName(QStringLiteral("ByteForge"));

    QGuiApplication::setWindowIcon(MakeWindowIcon());
    QGuiApplication::setDesktopFileName(QStringLiteral("byteforge-hub"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);

    engine.loadFromModule("ByteForge.Hub", "Main");

    return QGuiApplication::exec();
}
