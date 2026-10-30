#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QColor>

class QgsProject;
class QgsVectorLayer;
class QgsRasterLayer;
class QgsMapLayer;
class QgsRectangle;
class QgsFeature;

namespace AndongPack {

struct ShpLayer {
  QString title;
  QString path;
  QString group;
};

struct PresentationStyle {
  QColor fill = QColor(37, 99, 235, 55);
  QColor stroke = QColor(29, 78, 216);
  double mm = 0.45;
  bool noFill = false;
  bool dashed = false;
};

QStringList listShapefiles(const QString& rootDir);
QVector<ShpLayer> catalog(const QString& rootDir);
QString resolveDataRoot();
QString resolveAssetRoot();
QString preferredLabelField(const QgsVectorLayer* layer);
bool isSiteLayer(const QString& title);
bool isCadastralLayer(const QString& title, const QgsVectorLayer* layer = nullptr);
bool isEmdLayer(const QString& title, const QgsVectorLayer* layer = nullptr);
QString desktopAndongDir();
QString findEmdShapefile();
QString findCadastralShapefile();
QString andongEmdSubset(const QgsVectorLayer* layer);
bool applyAndongEmdFilter(QgsVectorLayer* layer);
QgsVectorLayer* loadAndongEmd(const QString& path);
QgsVectorLayer* loadAndongCadastral(const QString& path);
double labelMinScaleDenom(const QString& title, const QgsVectorLayer* layer = nullptr);
double geometryMinScaleDenom(const QString& title, const QgsVectorLayer* layer = nullptr);
bool applyGeometryScaleRange(QgsMapLayer* layer, const QString& title);
QgsRectangle detailExtent(const QgsRectangle& featureExtent);
QString formatInspectLine(const QgsVectorLayer* layer, const QgsFeature& feature);
QString cityBoundaryPath(const QString& assetRoot);
QString cityMarkPath();
QString satelliteRasterPath(const QString& assetRoot);
int offlineSatelliteMaxZoom(const QString& mbtilesPath);
QString cadastralRasterPath(const QString& assetRoot);
QString jibunRasterPath(const QString& assetRoot);
PresentationStyle presentationStyleFor(const QString& title, const QgsVectorLayer* layer = nullptr);
double labelPointSize(const QString& title, const QgsVectorLayer* layer = nullptr);
bool applyRasterOpacityPercent(QgsMapLayer* layer, int percent);

QgsVectorLayer* loadShapefile(const QString& path, const QString& title);
bool applyPresentationStyle(QgsVectorLayer* layer, const QString& title);
bool applyLabels(QgsVectorLayer* layer);
QgsVectorLayer* loadCityMask(QgsProject* project, const QString& boundaryPath);
QgsRasterLayer* loadLocalRaster(const QString& path, const QString& title);
QgsRectangle cityExtent5179(QgsVectorLayer* maskLayer);

}  // namespace AndongPack
