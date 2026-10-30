#include "AndongPack.h"

#include "LayerOps.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

#include <qgsproject.h>
#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsgeometry.h>
#include <qgsfeature.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsrectangle.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>
#include <qgsfillsymbol.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsinvertedpolygonrenderer.h>

namespace AndongPack {
namespace {

bool isRemoteUri(const QString& path) {
  const QString p = path.trimmed().toLower();
  return p.startsWith(QLatin1String("type=xyz")) || p.contains(QLatin1String("wms:")) ||
         p.contains(QLatin1String("http://")) || p.contains(QLatin1String("https://")) ||
         p.contains(QLatin1String("wmts"));
}

bool skipCatalog(const QString& title) {
  return title.contains(QLatin1String("LSMD_ADM")) || title.contains(QLatin1String("LSMD_CONT")) ||
         title.contains(QStringLiteral("읍면동"));
}

QString groupOf(const QString& title) {
  if (skipCatalog(title))
    return QStringLiteral("행정");
  if (title.contains(QStringLiteral("지적")) || title.contains(QStringLiteral("지번")))
    return QStringLiteral("지적");
  if (title.contains(QStringLiteral("지정")) || title.contains(QStringLiteral("등록문화유산")) ||
      title.contains(QStringLiteral("보호구역")))
    return QStringLiteral("지정유산");
  if (title.contains(QStringLiteral("유적위치")) || title.contains(QStringLiteral("분포지도")))
    return QStringLiteral("유적");
  if (title.contains(QStringLiteral("허가구역")))
    return QStringLiteral("조사구역");
  if (title.contains(QStringLiteral("현상변경")))
    return QStringLiteral("기준");
  return QStringLiteral("기타");
}

QString firstExisting(const QStringList& cands) {
  for (const QString& p : cands) {
    if (!p.isEmpty() && QFileInfo::exists(p))
      return QFileInfo(p).absoluteFilePath();
  }
  return {};
}

}  // namespace

QStringList listShapefiles(const QString& rootDir) {
  QStringList out;
  if (rootDir.isEmpty() || !QDir(rootDir).exists())
    return out;
  QDir dir(rootDir);
  const QFileInfoList files = dir.entryInfoList(QStringList{QStringLiteral("*.shp")},
                                                QDir::Files, QDir::Name);
  for (const QFileInfo& fi : files)
    out << fi.absoluteFilePath();
  const QFileInfoList subs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QFileInfo& sub : subs)
    out << listShapefiles(sub.absoluteFilePath());
  return out;
}

QVector<ShpLayer> catalog(const QString& rootDir) {
  QVector<ShpLayer> out;
  for (const QString& path : listShapefiles(rootDir)) {
    ShpLayer s;
    s.path = path;
    s.title = QFileInfo(path).completeBaseName();
    if (skipCatalog(s.title))
      continue;
    s.group = groupOf(s.title);
    out.push_back(s);
  }
  return out;
}

QString desktopAndongDir() {
  const QString desk = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  if (desk.isEmpty())
    return {};
  const QString p = QDir(desk).filePath(QStringLiteral("안동시"));
  return QDir(p).exists() ? QFileInfo(p).absoluteFilePath() : QString();
}

QString resolveDataRoot() {
  const QString exe = QCoreApplication::applicationDirPath();
  return firstExisting({
      QDir(exe).filePath(QStringLiteral("data/andong/shp")),
      desktopAndongDir(),
      QDir(exe).filePath(QStringLiteral("안동시")),
      QDir(exe).filePath(QStringLiteral("../안동시")),
      QStringLiteral("D:/hgis/안동시"),
      QDir::current().filePath(QStringLiteral("안동시")),
  });
}

QString resolveAssetRoot() {
  const QString exe = QCoreApplication::applicationDirPath();
  const QString hit = firstExisting({
      QDir(exe).filePath(QStringLiteral("data/andong/andong_city.geojson")),
      QStringLiteral("D:/hgis/data/andong/andong_city.geojson"),
      QDir::current().filePath(QStringLiteral("data/andong/andong_city.geojson")),
  });
  if (hit.isEmpty())
    return QDir(exe).filePath(QStringLiteral("data/andong"));
  return QFileInfo(hit).absolutePath();
}

QString preferredLabelField(const QgsVectorLayer* layer) {
  if (!layer || !layer->isValid())
    return {};
  const QStringList prefer = {
      QStringLiteral("EMD_NM"),     QStringLiteral("읍면동명"),   QStringLiteral("동명"),
      QStringLiteral("유적명"),     QStringLiteral("국가유산명"), QStringLiteral("명칭"),
      QStringLiteral("사업명"),     QStringLiteral("구역명"),     QStringLiteral("지번"),
      QStringLiteral("JIBUN"),      QStringLiteral("jibun"),
  };
  for (const QString& n : prefer) {
    const int idx = layer->fields().lookupField(n);
    if (idx >= 0)
      return layer->fields().at(idx).name();
  }
  return LayerOps::detectNameField(layer);
}

bool isSiteLayer(const QString& title) {
  return title.contains(QStringLiteral("유적위치")) || title.contains(QStringLiteral("유적명"));
}

bool isCadastralLayer(const QString& title, const QgsVectorLayer* layer) {
  if (title.contains(QLatin1String("LSMD_CONT")) || title.contains(QStringLiteral("지적")) ||
      title.contains(QStringLiteral("지번")) ||
      title.contains(QStringLiteral("필지"), Qt::CaseInsensitive) ||
      title.contains(QLatin1String("cadastral"), Qt::CaseInsensitive))
    return true;
  if (!layer)
    return false;
  return layer->fields().lookupField(QStringLiteral("지번")) >= 0 ||
         layer->fields().lookupField(QStringLiteral("jibun")) >= 0 ||
         layer->fields().lookupField(QStringLiteral("pnu")) >= 0;
}

bool isEmdLayer(const QString& title, const QgsVectorLayer* layer) {
  if (title.contains(QStringLiteral("읍면동")) || title.contains(QLatin1String("LSMD_ADM")) ||
      title.contains(QLatin1String("EMD"), Qt::CaseInsensitive))
    return true;
  if (!layer)
    return false;
  return layer->fields().lookupField(QStringLiteral("EMD_NM")) >= 0 &&
         (layer->fields().lookupField(QStringLiteral("COL_ADM_SE")) >= 0 ||
          layer->fields().lookupField(QStringLiteral("EMD_CD")) >= 0);
}

QString findEmdShapefile() {
  QStringList roots = {
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/andong/emd")),
      QStringLiteral("D:/hgis/data/andong/emd"),
  };
  const QString desk = desktopAndongDir();
  if (!desk.isEmpty())
    roots << desk;
  const QString data = resolveDataRoot();
  if (!data.isEmpty())
    roots << data;
  for (const QString& root : roots) {
    for (const QString& path : listShapefiles(root)) {
      const QString base = QFileInfo(path).completeBaseName();
      if (base.contains(QLatin1String("LSMD_ADM")) || base.contains(QStringLiteral("읍면동")))
        return path;
    }
  }
  return {};
}

QString findCadastralShapefile() {
  QStringList roots = {
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/andong/cad")),
      QStringLiteral("D:/hgis/data/andong/cad"),
  };
  const QString desk = desktopAndongDir();
  if (!desk.isEmpty())
    roots << desk;
  const QString data = resolveDataRoot();
  if (!data.isEmpty())
    roots << data;
  QString fallback;
  for (const QString& root : roots) {
    for (const QString& path : listShapefiles(root)) {
      const QString base = QFileInfo(path).completeBaseName();
      if (!base.contains(QLatin1String("LSMD_CONT")) && !base.contains(QStringLiteral("지적")))
        continue;
      if (base.contains(QLatin1String("47170")) || path.contains(QStringLiteral("안동")))
        return path;
      if (fallback.isEmpty())
        fallback = path;
    }
  }
  return fallback;
}

QgsVectorLayer* loadAndongCadastral(const QString& path) {
  const QString src = path.isEmpty() ? findCadastralShapefile() : path;
  QgsVectorLayer* layer = loadShapefile(src, QStringLiteral("지적"));
  if (!layer)
    return nullptr;
  if (layer->fields().lookupField(QStringLiteral("COL_ADM_SE")) >= 0 &&
      !QFileInfo(src).completeBaseName().contains(QLatin1String("47170"))) {
    layer->setSubsetString(QStringLiteral("\"COL_ADM_SE\" = '47170'"));
    if (layer->featureCount() <= 0) {
      delete layer;
      return nullptr;
    }
  }
  applyPresentationStyle(layer, QStringLiteral("지적"));
  applyLabels(layer);
  applyGeometryScaleRange(layer, QStringLiteral("지적"));
  return layer;
}

double labelMinScaleDenom(const QString& title, const QgsVectorLayer* layer) {
  if (isCadastralLayer(title, layer))
    return 8000.0;
  if (title.contains(QStringLiteral("분포지도")))
    return 80000.0;
  if (isEmdLayer(title, layer) || isSiteLayer(title))
    return 400000.0;
  return 300000.0;
}

double geometryMinScaleDenom(const QString& title, const QgsVectorLayer* layer) {
  if (isCadastralLayer(title, layer))
    return 25000.0;
  return 0.0;
}

bool applyGeometryScaleRange(QgsMapLayer* layer, const QString& title) {
  if (!layer)
    return false;
  const auto* vl = qobject_cast<const QgsVectorLayer*>(layer);
  const double minDenom = geometryMinScaleDenom(title, vl);
  if (minDenom <= 0.0) {
    layer->setScaleBasedVisibility(false);
    return true;
  }
  layer->setScaleBasedVisibility(true);
  layer->setMinimumScale(minDenom + 1.0);
  layer->setMaximumScale(0.0);
  return true;
}

QgsRectangle detailExtent(const QgsRectangle& featureExtent) {
  if (featureExtent.isEmpty())
    return featureExtent;
  const double longer = std::max(featureExtent.width(), featureExtent.height());
  double pad = 0.0;
  if (longer > 400.0)
    pad = longer * 0.08;
  else
    pad = std::max(30.0, std::min(longer * 0.35 + 20.0, 180.0));
  return featureExtent.buffered(pad);
}

QString formatInspectLine(const QgsVectorLayer* layer, const QgsFeature& feature) {
  if (!layer)
    return {};
  QStringList parts;
  parts << layer->name();
  const QString nameField = preferredLabelField(layer);
  QSet<QString> seen;
  auto addField = [&](const QString& field) {
    const int idx = layer->fields().lookupField(field);
    if (idx < 0)
      return;
    const QString key = layer->fields().at(idx).name();
    if (seen.contains(key.toLower()))
      return;
    const QString v = feature.attribute(idx).toString().trimmed();
    if (v.isEmpty())
      return;
    seen.insert(key.toLower());
    parts << v;
  };
  if (!nameField.isEmpty())
    addField(nameField);
  const QStringList extra = {
      QStringLiteral("국가유산명"), QStringLiteral("유적명"),   QStringLiteral("명칭"),
      QStringLiteral("소재지"),     QStringLiteral("지정번호"), QStringLiteral("JIBUN"),
      QStringLiteral("지번"),       QStringLiteral("PNU"),      QStringLiteral("사업명"),
  };
  for (const QString& f : extra)
    addField(f);
  return parts.join(QStringLiteral("  ·  "));
}

QString andongEmdSubset(const QgsVectorLayer* layer) {
  if (!layer)
    return {};
  if (layer->fields().lookupField(QStringLiteral("COL_ADM_SE")) >= 0)
    return QStringLiteral("\"COL_ADM_SE\" = '47170'");
  if (layer->fields().lookupField(QStringLiteral("EMD_CD")) >= 0)
    return QStringLiteral("\"EMD_CD\" LIKE '47170%'");
  return {};
}

bool applyAndongEmdFilter(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid())
    return false;
  const QString expr = andongEmdSubset(layer);
  if (expr.isEmpty())
    return false;
  return layer->setSubsetString(expr);
}

QgsVectorLayer* loadAndongEmd(const QString& path) {
  const QString src = path.isEmpty() ? findEmdShapefile() : path;
  QgsVectorLayer* layer = loadShapefile(src, QStringLiteral("읍면동"));
  if (!layer)
    return nullptr;
  if (!applyAndongEmdFilter(layer) || layer->featureCount() <= 0) {
    delete layer;
    return nullptr;
  }
  applyPresentationStyle(layer, QStringLiteral("읍면동"));
  applyLabels(layer);
  return layer;
}

QString cityBoundaryPath(const QString& assetRoot) {
  return QDir(assetRoot).filePath(QStringLiteral("andong_city.geojson"));
}

QString cityMarkPath() {
  const QString exe = QCoreApplication::applicationDirPath();
  const QString root = resolveAssetRoot();
  return firstExisting({
      QDir(root).filePath(QStringLiteral("andong_mark.png")),
      QDir(exe).filePath(QStringLiteral("data/andong/andong_mark.png")),
      QStringLiteral("D:/hgis/data/andong/andong_mark.png"),
      QDir::current().filePath(QStringLiteral("data/andong/andong_mark.png")),
  });
}

PresentationStyle presentationStyleFor(const QString& title, const QgsVectorLayer* layer) {
  if (isEmdLayer(title, layer)) {
    PresentationStyle s;
    s.fill = QColor(0, 0, 0, 0);
    s.stroke = QColor(0, 60, 120);
    s.mm = 1.6;
    s.noFill = true;
    return s;
  }
  if (isCadastralLayer(title, layer)) {
    PresentationStyle s;
    s.fill = QColor(0, 0, 0, 0);
    s.stroke = QColor(31, 41, 55);
    s.mm = 0.18;
    s.noFill = true;
    return s;
  }

  struct Row {
    const char16_t* key;
    QRgb fill;
    QRgb stroke;
    double mm;
    bool dashed;
  };
  // Longest keys first so 국가지정유산보호구역 does not take 국가지정유산.
  static const Row rows[] = {
      {u"국가지정유산보호구역", qRgba(0xFB, 0x71, 0x85, 0x1C), qRgb(0xFB, 0x71, 0x85), 0.55, true},
      {u"시도지정유산보호구역", qRgba(0x2D, 0xD4, 0xBF, 0x1C), qRgb(0x2D, 0xD4, 0xBF), 0.55, true},
      {u"국가등록문화유산", qRgba(0xC0, 0x26, 0xD3, 0x2A), qRgb(0xC0, 0x26, 0xD3), 0.55, false},
      {u"시도등록문화유산", qRgba(0x4F, 0x46, 0xE5, 0x2A), qRgb(0x4F, 0x46, 0xE5), 0.55, false},
      {u"문화유적분포지도", qRgba(0x8B, 0x5C, 0xF6, 0x24), qRgb(0x8B, 0x5C, 0xF6), 0.35, false},
      {u"지표사업허가구역", qRgba(0xC9, 0xA2, 0x27, 0x2E), qRgb(0xC9, 0xA2, 0x27), 0.45, false},
      {u"발굴사업허가구역", qRgba(0xFF, 0x6B, 0x35, 0x2E), qRgb(0xFF, 0x6B, 0x35), 0.45, false},
      {u"지표유적위치도", qRgba(0x25, 0x63, 0xEB, 0x46), qRgb(0x25, 0x63, 0xEB), 0.50, false},
      {u"발굴유적위치도", qRgba(0xFF, 0x3D, 0x00, 0x46), qRgb(0xFF, 0x3D, 0x00), 0.50, false},
      {u"현상변경허용기준", qRgba(0xF4, 0xD3, 0x7A, 0x22), qRgb(0xF4, 0xD3, 0x7A), 0.40, true},
      {u"국가지정유산", qRgba(0xE1, 0x1D, 0x48, 0x38), qRgb(0xE1, 0x1D, 0x48), 0.55, false},
      {u"시도지정유산", qRgba(0x02, 0x84, 0xC7, 0x38), qRgb(0x02, 0x84, 0xC7), 0.55, false},
  };
  for (const Row& row : rows) {
    if (title.contains(QString::fromUtf16(row.key))) {
      PresentationStyle s;
      s.fill = QColor::fromRgba(row.fill);
      s.stroke = QColor::fromRgb(row.stroke);
      s.mm = row.mm;
      s.dashed = row.dashed;
      return s;
    }
  }
  return PresentationStyle{};
}

double labelPointSize(const QString& title, const QgsVectorLayer* layer) {
  if (isEmdLayer(title, layer))
    return 14.0;
  if (isCadastralLayer(title, layer))
    return 8.0;
  if (title.contains(QStringLiteral("분포지도")))
    return 9.0;
  return 10.0;
}

bool applyRasterOpacityPercent(QgsMapLayer* layer, int percent) {
  if (!layer)
    return false;
  layer->setOpacity(qBound(0, percent, 100) / 100.0);
  layer->triggerRepaint();
  return true;
}

int offlineSatelliteMaxZoom(const QString& mbtilesPath) {
  if (mbtilesPath.isEmpty() || !QFile::exists(mbtilesPath))
    return -1;
  QFile f(mbtilesPath);
  if (!f.open(QIODevice::ReadOnly))
    return -1;
  const QByteArray head = f.read(262144);
  const int at = head.indexOf("maxzoom");
  if (at < 0)
    return -1;
  const int begin = qMax(0, at - 8);
  const int end = qMin(int(head.size()), at + 24);
  const QByteArray win = head.mid(begin, end - begin);
  for (int i = 0; i + 1 < win.size(); ++i) {
    if (win[i] == '1' && win[i + 1] >= '2' && win[i + 1] <= '8')
      return QByteArray(1, win[i]).append(win[i + 1]).toInt();
  }
  return -1;
}

QString satelliteRasterPath(const QString& assetRoot) {
  const QString exe = QCoreApplication::applicationDirPath();
  const QString hit = firstExisting({
      QDir(assetRoot).filePath(QStringLiteral("satellite.mbtiles")),
      QDir(assetRoot).filePath(QStringLiteral("satellite.tif")),
      QDir(assetRoot).filePath(QStringLiteral("satellite.tiff")),
      QDir(exe).filePath(QStringLiteral("data/andong/satellite.mbtiles")),
      QStringLiteral("D:/hgis/data/andong/satellite.mbtiles"),
      QDir::current().filePath(QStringLiteral("data/andong/satellite.mbtiles")),
  });
  return hit.isEmpty() ? QDir(assetRoot).filePath(QStringLiteral("satellite.mbtiles")) : hit;
}

QString cadastralRasterPath(const QString& assetRoot) {
  const QString hit = firstExisting({
      QDir(assetRoot).filePath(QStringLiteral("cadastral.mbtiles")),
      QDir(assetRoot).filePath(QStringLiteral("cadastral.tif")),
  });
  return hit.isEmpty() ? QDir(assetRoot).filePath(QStringLiteral("cadastral.mbtiles")) : hit;
}

QString jibunRasterPath(const QString& assetRoot) {
  const QString hit = firstExisting({
      QDir(assetRoot).filePath(QStringLiteral("jibun.mbtiles")),
      QDir(assetRoot).filePath(QStringLiteral("jibun.tif")),
  });
  return hit.isEmpty() ? QDir(assetRoot).filePath(QStringLiteral("jibun.mbtiles")) : hit;
}

QgsVectorLayer* loadShapefile(const QString& path, const QString& title) {
  if (path.isEmpty() || !QFile::exists(path))
    return nullptr;
  const QString enc = LayerOps::prepareShapefileEncoding(path);
  auto* layer = new QgsVectorLayer(path, title, QStringLiteral("ogr"));
  if (!layer->isValid()) {
    delete layer;
    return nullptr;
  }
  if (!enc.isEmpty())
    LayerOps::setShapefileEncoding(layer, enc);
  if (!layer->crs().isValid())
    layer->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  return layer;
}

bool applyPresentationStyle(QgsVectorLayer* layer, const QString& title) {
  if (!layer || !layer->isValid())
    return false;
  const PresentationStyle s = presentationStyleFor(title, layer);
  return LayerOps::applySimpleVectorStyle(layer, s.fill, s.stroke, s.mm, 3.2, s.noFill, false, s.dashed);
}

bool applyLabels(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid())
    return false;
  const QString field = preferredLabelField(layer);
  if (field.isEmpty())
    return false;
  const QString title = layer->name();
  const double pt = labelPointSize(title, layer);
  if (!LayerOps::applyNameAttributeLabels(layer, field, pt, false))
    return false;
  if (!layer->labeling())
    return true;
  QgsPalLayerSettings s = layer->labeling()->settings();
  s.scaleVisibility = true;
  s.minimumScale = labelMinScaleDenom(title, layer);
  s.maximumScale = 0.0;
  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
  return true;
}

QgsVectorLayer* loadCityMask(QgsProject* project, const QString& boundaryPath) {
  if (!project || boundaryPath.isEmpty() || !QFile::exists(boundaryPath))
    return nullptr;
  QgsVectorLayer src(boundaryPath, QStringLiteral("andong_city_src"), QStringLiteral("ogr"));
  if (!src.isValid())
    return nullptr;
  QgsFeature f;
  if (!src.getFeatures().nextFeature(f) || f.geometry().isEmpty())
    return nullptr;
  const QgsCoordinateReferenceSystem srcCrs =
      src.crs().isValid() ? src.crs() : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326"));
  return LayerOps::upsertAdminEmdMask(project, f.geometry(), srcCrs, QStringLiteral("EPSG:5179"),
                                      QStringLiteral("안동시"));
}

QgsRasterLayer* loadLocalRaster(const QString& path, const QString& title) {
  if (path.isEmpty() || isRemoteUri(path) || !QFile::exists(path))
    return nullptr;
  auto* rl = new QgsRasterLayer(path, title, QStringLiteral("gdal"));
  if (!rl->isValid()) {
    delete rl;
    return nullptr;
  }
  LayerOps::markReferenceLayer(rl);
  return rl;
}

QgsRectangle cityExtent5179(QgsVectorLayer* maskLayer) {
  if (maskLayer && maskLayer->isValid() && !maskLayer->extent().isEmpty())
    return maskLayer->extent();
  return QgsRectangle(1084000, 1812000, 1134000, 1869000);
}

}  // namespace AndongPack
