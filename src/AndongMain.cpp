#include "AndongWindow.h"

#include <cstdlib>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTimer>

#include <qgsapplication.h>
#include <qgsproviderregistry.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static QString exeDir() {
#ifdef Q_OS_WIN
  wchar_t buf[4096];
  const DWORD n = GetModuleFileNameW(nullptr, buf, 4096);
  if (n > 0 && n < 4096)
    return QFileInfo(QString::fromWCharArray(buf, int(n))).absolutePath();
#endif
  return QCoreApplication::applicationDirPath();
}

static void applyBundledRuntime() {
  const QDir app(exeDir());
  const QString qgis = app.filePath(QStringLiteral("apps/qgis-dev"));
  if (!QDir(qgis).exists())
    return;
  qputenv("OSGEO4W_ROOT", app.absolutePath().toUtf8());
  qputenv("QGIS_PREFIX_PATH", qgis.toUtf8());
  const QString proj = app.filePath(QStringLiteral("share/proj"));
  if (QDir(proj).exists()) {
    qputenv("PROJ_DATA", proj.toUtf8());
    qputenv("PROJ_LIB", proj.toUtf8());
  }
  const QString gdal = app.filePath(QStringLiteral("apps/gdal-dev/share/gdal"));
  if (QDir(gdal).exists())
    qputenv("GDAL_DATA", gdal.toUtf8());
  const QString qtPlug = app.filePath(QStringLiteral("apps/Qt6/plugins"));
  if (QDir(qtPlug).exists()) {
    QCoreApplication::addLibraryPath(qtPlug);
    qputenv("QT_PLUGIN_PATH", qtPlug.toUtf8());
  }
  const QString qgisPlugDir = QDir(qgis).filePath(QStringLiteral("plugins"));
  if (QDir(qgisPlugDir).exists())
    qputenv("QGIS_PLUGIN_PATH", qgisPlugDir.toUtf8());
  QStringList prepend;
  for (const QString& rel : {QString(), QStringLiteral("bin"), QStringLiteral("apps/qgis-dev/bin"),
                             QStringLiteral("apps/Qt6/bin"), QStringLiteral("apps/gdal-dev/bin")}) {
    const QString p = rel.isEmpty() ? app.absolutePath() : app.filePath(rel);
    if (QDir(p).exists())
      prepend << QDir::toNativeSeparators(p);
  }
  const QString old = QString::fromLocal8Bit(qgetenv("PATH"));
  qputenv("PATH", (prepend.join(QLatin1Char(';')) + QLatin1Char(';') + old).toLocal8Bit());
}

static QString resolvePrefix() {
  if (const char* e = std::getenv("QGIS_PREFIX_PATH")) {
    QString p = QString::fromUtf8(e);
    if (!QDir(p).exists())
      p = QString::fromLocal8Bit(e);
    if (QDir(p).exists())
      return p;
  }
  const QString bundled = exeDir() + QStringLiteral("/apps/qgis-dev");
  if (QDir(bundled).exists())
    return bundled;
  if (const char* o = std::getenv("OSGEO4W_ROOT")) {
    const QString c = QString::fromLocal8Bit(o) + QStringLiteral("/apps/qgis-dev");
    if (QDir(c).exists())
      return c;
  }
  return {};
}

int main(int argc, char** argv) {
  bool smoke = false;
  for (int i = 1; i < argc; ++i) {
    if (QString::fromLocal8Bit(argv[i]) == QLatin1String("--smoke-quit"))
      smoke = true;
  }

  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
  applyBundledRuntime();
  QgsApplication app(argc, argv, true);
  app.setApplicationName(QStringLiteral("andong-viewer"));
  app.setApplicationDisplayName(QStringLiteral("안동시 문화유산 지도"));
  app.setOrganizationName(QStringLiteral("ka-hgis"));
  app.setStyle(QStringLiteral("Fusion"));

  const QString prefix = resolvePrefix();
  if (prefix.isEmpty())
    return 2;
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::setPkgDataPath(prefix);
  QgsApplication::initQgis();

  AndongWindow w;
  w.show();
  if (smoke) {
    QTimer::singleShot(400, &app, &QCoreApplication::quit);
  }
  const int rc = app.exec();
  QgsApplication::exitQgis();
  return rc;
}
