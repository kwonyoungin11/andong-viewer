#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QImage>
#include <QList>
#include <QPainter>
#include <QSet>

#include "core/AndongPack.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsfields.h>
#include <qgsfield.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsinvertedpolygonrenderer.h>
#include <qgsmaprenderercustompainterjob.h>
#include <qgsmapsettings.h>
#include <qgspointxy.h>
#include <qgspallabeling.h>
#include <qgsrasterlayer.h>
#include <qgsvectorlayerlabeling.h>

class TestAndong : public QObject {
  Q_OBJECT
private slots:
  void listShapefiles_findsAndongHeritage();
  void preferredLabelField_siteThenHeritageThenLot();
  void isSiteAndCadastral_byTitleOrJibun();
  void cityMask_isInvertedWhiteOn5179();
  void applyLabels_canToggleOff();
  void localRaster_rejectsRemoteXyz();
  void emdFilter_keepsAndongOnly();
  void preferredLabelField_emdName();
  void presentationStyleFor_splitsNationalVsSidoAndSurfaceVsExcavation();
  void presentationStyleFor_keepsEmdAndCadastralOutOfHeritagePalette();
  void cityMarkPath_findsPng();
  void applyRasterOpacityPercent_clamps();
  void themeQss_usesOfficialAndongViColors();
  void satelliteRasterPath_fallsBackToRepoPack();
  void labelPointSize_isTwoPointsLarger();
  void isCadastralLayer_matchesLsmdContTitle();
  void preferredLabelField_readsJibunUppercase();
  void labelMinScaleDenom_cityVsCloseDetail();
  void geometryMinScaleDenom_hidesCadastralUntilClose();
  void detailExtent_capsPadSoSitesFillTheView();
  void formatInspectLine_showsLayerAndName();
  void applyLabels_setsHeritageAndCadastralScaleVisibility();
  void findCadastralShapefile_andong47170();
  void offlineSatelliteMaxZoom_is16();
  void satelliteCloseView_rendersLocalDetail();
  void scaleViews_emdAtCity_cadastralOnlyWhenClose();
};

static QImage renderLayers(const QList<QgsMapLayer*>& layers, const QgsRectangle& ext, const QSize& size) {
  QgsMapSettings s;
  s.setLayers(layers);
  s.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  s.setExtent(ext);
  s.setOutputSize(size);
  s.setOutputDpi(96);
  s.setBackgroundColor(Qt::white);
  QImage img(s.outputSize(), QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::white);
  QPainter painter(&img);
  QgsMapRendererCustomPainterJob job(s, &painter);
  job.start();
  job.waitForFinished();
  painter.end();
  return img;
}

static double meanAbsDiff(const QImage& a, const QImage& b) {
  if (a.size() != b.size())
    return 1e9;
  qint64 s = 0;
  int n = 0;
  for (int y = 0; y < a.height(); ++y) {
    const QRgb* ra = reinterpret_cast<const QRgb*>(a.constScanLine(y));
    const QRgb* rb = reinterpret_cast<const QRgb*>(b.constScanLine(y));
    for (int x = 0; x < a.width(); ++x) {
      s += qAbs(qRed(ra[x]) - qRed(rb[x])) + qAbs(qGreen(ra[x]) - qGreen(rb[x])) +
           qAbs(qBlue(ra[x]) - qBlue(rb[x]));
      ++n;
    }
  }
  return n ? double(s) / double(n) : 0.0;
}

static int countNearColor(const QImage& img, const QColor& c, int tol) {
  int n = 0;
  for (int y = 0; y < img.height(); ++y) {
    const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
    for (int x = 0; x < img.width(); ++x) {
      if (qAbs(qRed(row[x]) - c.red()) <= tol && qAbs(qGreen(row[x]) - c.green()) <= tol &&
          qAbs(qBlue(row[x]) - c.blue()) <= tol)
        ++n;
    }
  }
  return n;
}

static QgsVectorLayer* memLayer(const QString& name, const QStringList& fields) {
  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5179"), name, QStringLiteral("memory"));
  Q_ASSERT(vl->isValid());
  QList<QgsField> add;
  for (const QString& n : fields)
    add.append(QgsField(n, QMetaType::QString));
  vl->dataProvider()->addAttributes(add);
  vl->updateFields();
  return vl;
}

void TestAndong::listShapefiles_findsAndongHeritage() {
  const QString root = QStringLiteral("안동시");
  QVERIFY2(QDir(root).exists(), "안동시");
  const QStringList shps = AndongPack::listShapefiles(root);
  QVERIFY2(shps.size() >= 10, qPrintable(QString::number(shps.size())));
  bool hasSite = false;
  bool hasDist = false;
  for (const QString& p : shps) {
    if (p.contains(QStringLiteral("지표유적위치도"))) hasSite = true;
    if (p.contains(QStringLiteral("문화유적분포지도"))) hasDist = true;
  }
  QVERIFY(hasSite);
  QVERIFY(hasDist);
}

void TestAndong::preferredLabelField_siteThenHeritageThenLot() {
  auto* site = memLayer(QStringLiteral("발굴유적위치도"),
                        {QStringLiteral("소재지"), QStringLiteral("사업명"), QStringLiteral("유적명")});
  QCOMPARE(AndongPack::preferredLabelField(site), QStringLiteral("유적명"));
  delete site;

  auto* heritage = memLayer(QStringLiteral("국가지정유산"),
                            {QStringLiteral("지정번호"), QStringLiteral("국가유산명"), QStringLiteral("소재지")});
  QCOMPARE(AndongPack::preferredLabelField(heritage), QStringLiteral("국가유산명"));
  delete heritage;

  auto* dist = memLayer(QStringLiteral("문화유적분포지도"),
                        {QStringLiteral("유산코드"), QStringLiteral("명칭")});
  QCOMPARE(AndongPack::preferredLabelField(dist), QStringLiteral("명칭"));
  delete dist;

  auto* lot = memLayer(QStringLiteral("연속지적"),
                       {QStringLiteral("pnu"), QStringLiteral("지번")});
  QCOMPARE(AndongPack::preferredLabelField(lot), QStringLiteral("지번"));
  delete lot;
}

void TestAndong::isSiteAndCadastral_byTitleOrJibun() {
  QVERIFY(AndongPack::isSiteLayer(QStringLiteral("지표유적위치도")));
  QVERIFY(AndongPack::isSiteLayer(QStringLiteral("발굴유적위치도")));
  QVERIFY(!AndongPack::isSiteLayer(QStringLiteral("지표사업허가구역")));

  auto* cad = memLayer(QStringLiteral("필지"), {QStringLiteral("지번")});
  QVERIFY(AndongPack::isCadastralLayer(QStringLiteral("안동 지적"), cad));
  QVERIFY(AndongPack::isCadastralLayer(QStringLiteral("필지"), cad));
  delete cad;

  auto* other = memLayer(QStringLiteral("구역"), {QStringLiteral("명칭")});
  QVERIFY(!AndongPack::isCadastralLayer(QStringLiteral("현상변경허용기준"), other));
  delete other;
}

void TestAndong::cityMask_isInvertedWhiteOn5179() {
  const QString geo = QStringLiteral("data/andong/andong_city.geojson");
  QVERIFY2(QFile::exists(geo), qPrintable(geo));
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  QgsVectorLayer* mask = AndongPack::loadCityMask(&proj, geo);
  QVERIFY(mask);
  QVERIFY(mask->isValid());
  QVERIFY(dynamic_cast<QgsInvertedPolygonRenderer*>(mask->renderer()));
  QCOMPARE(int(mask->featureCount()), 1);
  const QgsRectangle e = mask->extent();
  QVERIFY2(e.width() > 10000 && e.height() > 10000, qPrintable(e.toString()));
  QVERIFY2(e.xMinimum() > 1000000 && e.xMaximum() < 1200000, "mask should be in EPSG:5179");
}

void TestAndong::applyLabels_canToggleOff() {
  auto* vl = memLayer(QStringLiteral("지표유적위치도"), {QStringLiteral("유적명")});
  QVERIFY(vl->startEditing());
  QgsFeature f(vl->fields());
  f.setAttribute(QStringLiteral("유적명"), QStringLiteral("하회마을"));
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(1100000, 1840000, 1100100, 1840100)));
  QVERIFY(vl->addFeature(f));
  QVERIFY(vl->commitChanges());
  QVERIFY(AndongPack::applyLabels(vl));
  QVERIFY(LayerOps::labelsVisible(vl));
  QVERIFY(LayerOps::setLabelsVisible(vl, false));
  QVERIFY(!LayerOps::labelsVisible(vl));
  QVERIFY(LayerOps::setLabelsVisible(vl, true));
  QVERIFY(LayerOps::labelsVisible(vl));
  delete vl;
}

void TestAndong::emdFilter_keepsAndongOnly() {
  auto* vl = memLayer(QStringLiteral("읍면동"),
                      {QStringLiteral("COL_ADM_SE"), QStringLiteral("EMD_NM"), QStringLiteral("EMD_CD")});
  QVERIFY(vl->startEditing());
  QgsFeature a(vl->fields());
  a.setAttribute(QStringLiteral("COL_ADM_SE"), QStringLiteral("47170"));
  a.setAttribute(QStringLiteral("EMD_NM"), QStringLiteral("풍산읍"));
  a.setAttribute(QStringLiteral("EMD_CD"), QStringLiteral("47170250"));
  a.setGeometry(QgsGeometry::fromRect(QgsRectangle(1100000, 1840000, 1100100, 1840100)));
  QVERIFY(vl->addFeature(a));
  QgsFeature b(vl->fields());
  b.setAttribute(QStringLiteral("COL_ADM_SE"), QStringLiteral("47150"));
  b.setAttribute(QStringLiteral("EMD_NM"), QStringLiteral("김천"));
  b.setAttribute(QStringLiteral("EMD_CD"), QStringLiteral("47150101"));
  b.setGeometry(QgsGeometry::fromRect(QgsRectangle(1200000, 1840000, 1200100, 1840100)));
  QVERIFY(vl->addFeature(b));
  QVERIFY(vl->commitChanges());
  QCOMPARE(int(vl->featureCount()), 2);
  QVERIFY(AndongPack::isEmdLayer(QStringLiteral("LSMD_ADM_SECT_UMD_47_202608"), vl));
  QVERIFY2(AndongPack::andongEmdSubset(vl).contains(QLatin1String("47170")),
           qPrintable(AndongPack::andongEmdSubset(vl)));
  QVERIFY(AndongPack::applyAndongEmdFilter(vl));
  QCOMPARE(int(vl->featureCount()), 1);
  QgsFeature only;
  QVERIFY(vl->getFeatures().nextFeature(only));
  QCOMPARE(only.attribute(QStringLiteral("EMD_NM")).toString(), QStringLiteral("풍산읍"));
  QVERIFY(AndongPack::applyPresentationStyle(vl, QStringLiteral("읍면동")));
  delete vl;
}

void TestAndong::preferredLabelField_emdName() {
  auto* vl = memLayer(QStringLiteral("읍면동"),
                      {QStringLiteral("COL_ADM_SE"), QStringLiteral("EMD_NM")});
  QCOMPARE(AndongPack::preferredLabelField(vl), QStringLiteral("EMD_NM"));
  delete vl;
}

void TestAndong::presentationStyleFor_splitsNationalVsSidoAndSurfaceVsExcavation() {
  const QStringList titles = {
      QStringLiteral("문화유적분포지도"),   QStringLiteral("지표유적위치도"),
      QStringLiteral("발굴유적위치도"),     QStringLiteral("지표사업허가구역"),
      QStringLiteral("발굴사업허가구역"),   QStringLiteral("국가지정유산"),
      QStringLiteral("국가등록문화유산"),   QStringLiteral("국가지정유산보호구역"),
      QStringLiteral("시도지정유산"),       QStringLiteral("시도등록문화유산"),
      QStringLiteral("시도지정유산보호구역"), QStringLiteral("현상변경허용기준"),
  };
  QSet<QString> strokes;
  for (const QString& title : titles) {
    auto* vl = memLayer(title, {QStringLiteral("명칭")});
    QVERIFY(AndongPack::applyPresentationStyle(vl, title));
    const AndongPack::PresentationStyle st = AndongPack::presentationStyleFor(title, vl);
    QVERIFY2(st.stroke.isValid(), qPrintable(title));
    const QString hex = st.stroke.name(QColor::HexRgb);
    QVERIFY2(!strokes.contains(hex), qPrintable(title + QLatin1Char(' ') + hex));
    strokes.insert(hex);
    delete vl;
  }
  QCOMPARE(strokes.size(), titles.size());
}

void TestAndong::presentationStyleFor_keepsEmdAndCadastralOutOfHeritagePalette() {
  const AndongPack::PresentationStyle emd =
      AndongPack::presentationStyleFor(QStringLiteral("읍면동"), nullptr);
  QVERIFY(emd.noFill);
  QCOMPARE(emd.stroke.name(QColor::HexRgb).toUpper(), QStringLiteral("#003C78"));
  QVERIFY(emd.mm >= 1.5);

  auto* cad = memLayer(QStringLiteral("안동 지적"), {QStringLiteral("지번")});
  const AndongPack::PresentationStyle lot =
      AndongPack::presentationStyleFor(QStringLiteral("안동 지적"), cad);
  QVERIFY(lot.noFill);
  QVERIFY(lot.mm < 0.5);
  QVERIFY(lot.stroke != emd.stroke);
  delete cad;
}

void TestAndong::cityMarkPath_findsPng() {
  const QString bundled = QStringLiteral("data/andong/andong_mark.png");
  if (!QFile::exists(bundled))
    QSKIP("andong_mark.png not bundled");
  const QString p = AndongPack::cityMarkPath();
  QVERIFY2(QFile::exists(p), qPrintable(p));
  QVERIFY(p.endsWith(QStringLiteral("andong_mark.png"), Qt::CaseInsensitive));
}

void TestAndong::applyRasterOpacityPercent_clamps() {
  auto* vl = memLayer(QStringLiteral("위성"), {QStringLiteral("명칭")});
  QVERIFY(AndongPack::applyRasterOpacityPercent(vl, 40));
  QCOMPARE(vl->opacity(), 0.4);
  QVERIFY(AndongPack::applyRasterOpacityPercent(vl, 250));
  QCOMPARE(vl->opacity(), 1.0);
  QVERIFY(AndongPack::applyRasterOpacityPercent(vl, -10));
  QCOMPARE(vl->opacity(), 0.0);
  delete vl;
}

void TestAndong::themeQss_usesOfficialAndongViColors() {
  QFile f(QStringLiteral("data/theme/andong.qss"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "andong.qss");
  const QString qss = QString::fromUtf8(f.readAll());
  QVERIFY2(qss.contains(QLatin1String("#003C78"), Qt::CaseInsensitive), "AD Blue R0 G60 B120");
  QVERIFY2(qss.contains(QLatin1String("#B41446"), Qt::CaseInsensitive), "AD Red R180 G20 B70");
  QVERIFY2(!qss.contains(QLatin1String("#1B4D3E"), Qt::CaseInsensitive), "old pine leftover");
  QVERIFY2(!qss.contains(QLatin1String("#C9A227"), Qt::CaseInsensitive), "old gold leftover");
  QVERIFY2(qss.contains(QLatin1String("font-size: 15px")), "+2pt body");
  QVERIFY2(!qss.contains(QLatin1String("font-size: 13px")), "old body size");
}

void TestAndong::labelPointSize_isTwoPointsLarger() {
  QCOMPARE(AndongPack::labelPointSize(QStringLiteral("지표유적위치도"), nullptr), 10.0);
  QCOMPARE(AndongPack::labelPointSize(QStringLiteral("읍면동"), nullptr), 14.0);
  QCOMPARE(AndongPack::labelPointSize(QStringLiteral("문화유적분포지도"), nullptr), 9.0);
  auto* cad = memLayer(QStringLiteral("안동 지적"), {QStringLiteral("지번")});
  QCOMPARE(AndongPack::labelPointSize(QStringLiteral("안동 지적"), cad), 8.0);
  delete cad;
}

void TestAndong::satelliteRasterPath_fallsBackToRepoPack() {
  const QString bundled = QStringLiteral("data/andong/satellite.mbtiles");
  if (!QFile::exists(bundled))
    QSKIP("satellite.mbtiles not packed");
  const QString p = AndongPack::satelliteRasterPath(QStringLiteral("data/andong/missing"));
  QVERIFY2(QFile::exists(p), qPrintable(p));
  QVERIFY(p.endsWith(QStringLiteral("satellite.mbtiles"), Qt::CaseInsensitive));
}

void TestAndong::isCadastralLayer_matchesLsmdContTitle() {
  QVERIFY(AndongPack::isCadastralLayer(QStringLiteral("LSMD_CONT_LDREG_47170_202608"), nullptr));
  QVERIFY(AndongPack::isCadastralLayer(QStringLiteral("LSMD_CONT_LDREG_경북_안동시"), nullptr));
  QVERIFY(!AndongPack::isCadastralLayer(QStringLiteral("LSMD_ADM_SECT_UMD_47_202608"), nullptr));
}

void TestAndong::preferredLabelField_readsJibunUppercase() {
  auto* cad = memLayer(QStringLiteral("지적"), {QStringLiteral("PNU"), QStringLiteral("JIBUN")});
  QCOMPARE(AndongPack::preferredLabelField(cad), QStringLiteral("JIBUN"));
  delete cad;
}

void TestAndong::labelMinScaleDenom_cityVsCloseDetail() {
  QCOMPARE(AndongPack::labelMinScaleDenom(QStringLiteral("읍면동"), nullptr), 400000.0);
  QCOMPARE(AndongPack::labelMinScaleDenom(QStringLiteral("국가지정유산"), nullptr), 300000.0);
  QCOMPARE(AndongPack::labelMinScaleDenom(QStringLiteral("지표유적위치도"), nullptr), 400000.0);
  QCOMPARE(AndongPack::labelMinScaleDenom(QStringLiteral("문화유적분포지도"), nullptr), 80000.0);
  auto* cad = memLayer(QStringLiteral("LSMD_CONT_LDREG_47170"), {QStringLiteral("JIBUN")});
  QCOMPARE(AndongPack::labelMinScaleDenom(QStringLiteral("LSMD_CONT_LDREG_47170"), cad), 8000.0);
  delete cad;
}

void TestAndong::geometryMinScaleDenom_hidesCadastralUntilClose() {
  QCOMPARE(AndongPack::geometryMinScaleDenom(QStringLiteral("국가지정유산"), nullptr), 0.0);
  auto* cad = memLayer(QStringLiteral("지적"), {QStringLiteral("JIBUN")});
  QCOMPARE(AndongPack::geometryMinScaleDenom(QStringLiteral("LSMD_CONT_LDREG_47170"), cad), 25000.0);
  delete cad;
}

void TestAndong::detailExtent_capsPadSoSitesFillTheView() {
  const QgsRectangle tiny(1100000, 1840000, 1100040, 1840040);
  const QgsRectangle zoom = AndongPack::detailExtent(tiny);
  QVERIFY(zoom.contains(tiny));
  QVERIFY2(zoom.width() <= 500.0, qPrintable(QString::number(zoom.width())));
  const QgsRectangle wide(1100000, 1840000, 1102000, 1841500);
  const QgsRectangle wideZoom = AndongPack::detailExtent(wide);
  QVERIFY(wideZoom.contains(wide));
  QVERIFY2(wideZoom.width() < wide.width() * 1.3, qPrintable(QString::number(wideZoom.width())));
}

void TestAndong::formatInspectLine_showsLayerAndName() {
  auto* vl = memLayer(QStringLiteral("국가지정유산"),
                      {QStringLiteral("국가유산명"), QStringLiteral("소재지")});
  QVERIFY(vl->startEditing());
  QgsFeature f(vl->fields());
  f.setAttribute(QStringLiteral("국가유산명"), QStringLiteral("하회마을"));
  f.setAttribute(QStringLiteral("소재지"), QStringLiteral("풍천면"));
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(1100000, 1840000, 1100100, 1840100)));
  QVERIFY(vl->addFeature(f));
  QVERIFY(vl->commitChanges());
  QgsFeature got;
  QVERIFY(vl->getFeatures().nextFeature(got));
  const QString line = AndongPack::formatInspectLine(vl, got);
  QVERIFY2(line.contains(QStringLiteral("하회마을")), qPrintable(line));
  QVERIFY2(line.contains(QStringLiteral("국가지정유산")), qPrintable(line));
  QVERIFY2(line.contains(QStringLiteral("풍천면")), qPrintable(line));
  delete vl;
}

void TestAndong::applyLabels_setsHeritageAndCadastralScaleVisibility() {
  auto* site = memLayer(QStringLiteral("지표유적위치도"), {QStringLiteral("유적명")});
  QVERIFY(AndongPack::applyLabels(site));
  QVERIFY(site->labeling());
  const QgsPalLayerSettings siteS = site->labeling()->settings();
  QVERIFY(siteS.scaleVisibility);
  QCOMPARE(siteS.minimumScale, 400000.0);
  delete site;

  auto* cad = memLayer(QStringLiteral("LSMD_CONT_LDREG_47170"), {QStringLiteral("JIBUN")});
  QVERIFY(AndongPack::applyLabels(cad));
  QVERIFY(AndongPack::applyGeometryScaleRange(cad, QStringLiteral("LSMD_CONT_LDREG_47170")));
  const QgsPalLayerSettings cadS = cad->labeling()->settings();
  QVERIFY(cadS.scaleVisibility);
  QCOMPARE(cadS.minimumScale, 8000.0);
  QVERIFY(cad->hasScaleBasedVisibility());
  QCOMPARE(cad->minimumScale(), 25001.0);
  delete cad;
}

void TestAndong::findCadastralShapefile_andong47170() {
  const QString p = AndongPack::findCadastralShapefile();
  if (p.isEmpty())
    QSKIP("Andong cadastral SHP not on this PC");
  QVERIFY2(QFile::exists(p), qPrintable(p));
  const QString base = QFileInfo(p).completeBaseName();
  QVERIFY2(base.contains(QLatin1String("LSMD_CONT")) || base.contains(QStringLiteral("지적")),
           qPrintable(p));
  QVERIFY2(p.contains(QLatin1String("47170")) || p.contains(QStringLiteral("안동")),
           qPrintable(p));
}

void TestAndong::offlineSatelliteMaxZoom_is16() {
  const QString p = AndongPack::satelliteRasterPath(QStringLiteral("data/andong"));
  if (!QFile::exists(p))
    QSKIP("satellite.mbtiles not packed");
  QCOMPARE(AndongPack::offlineSatelliteMaxZoom(p), 16);
}

void TestAndong::satelliteCloseView_rendersLocalDetail() {
  const QString p = AndongPack::satelliteRasterPath(QStringLiteral("data/andong"));
  if (!QFile::exists(p))
    QSKIP("satellite.mbtiles not packed");
  QgsRasterLayer* sat = AndongPack::loadLocalRaster(p, QStringLiteral("위성"));
  QVERIFY(sat);
  QVERIFY(sat->isValid());
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  QgsVectorLayer* mask =
      AndongPack::loadCityMask(&proj, QStringLiteral("data/andong/andong_city.geojson"));
  const QgsRectangle city = AndongPack::cityExtent5179(mask);
  const QgsPointXY c = city.center();
  QgsRectangle close(c.x() - 160.0, c.y() - 160.0, c.x() + 160.0, c.y() + 160.0);
  QgsMapSettings s;
  s.setLayers(QList<QgsMapLayer*>{sat});
  s.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  s.setExtent(close);
  s.setOutputSize(QSize(512, 512));
  s.setOutputDpi(96);
  s.setBackgroundColor(Qt::white);
  QImage img(s.outputSize(), QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::white);
  QPainter painter(&img);
  QgsMapRendererCustomPainterJob job(s, &painter);
  job.start();
  job.waitForFinished();
  painter.end();
  const QString out = QStringLiteral("build/andong-close-detail.png");
  QVERIFY(img.save(out));
  int dark = 0;
  qint64 edge = 0;
  int n = 0;
  for (int y = 1; y < img.height() - 1; ++y) {
    const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
    const QRgb* next = reinterpret_cast<const QRgb*>(img.constScanLine(y + 1));
    for (int x = 1; x < img.width() - 1; ++x) {
      const int g = qGray(row[x]);
      if (g < 240)
        ++dark;
      edge += qAbs(g - qGray(row[x + 1])) + qAbs(g - qGray(next[x]));
      ++n;
    }
  }
  delete sat;
  QVERIFY2(dark > 8000, qPrintable(QStringLiteral("dark=%1").arg(dark)));
  const double contrast = double(edge) / double(n);
  QVERIFY2(contrast > 3.0, qPrintable(QStringLiteral("contrast=%1").arg(contrast)));
}

void TestAndong::scaleViews_emdAtCity_cadastralOnlyWhenClose() {
  const QString satPath = AndongPack::satelliteRasterPath(QStringLiteral("data/andong"));
  if (!QFile::exists(satPath))
    QSKIP("satellite.mbtiles not packed");
  QgsRasterLayer* sat = AndongPack::loadLocalRaster(satPath, QStringLiteral("위성"));
  QVERIFY(sat);
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  QgsVectorLayer* mask =
      AndongPack::loadCityMask(&proj, QStringLiteral("data/andong/andong_city.geojson"));
  const QgsRectangle city = AndongPack::cityExtent5179(mask);
  const QgsPointXY c = city.center();
  QgsRectangle close(c.x() - 180.0, c.y() - 180.0, c.x() + 180.0, c.y() + 180.0);

  QgsVectorLayer* emd = AndongPack::loadAndongEmd(QString());
  QVERIFY2(emd, "읍면동 SHP");
  const QImage cityEmd = renderLayers(QList<QgsMapLayer*>{emd, mask, sat}, city, QSize(640, 480));
  QVERIFY(cityEmd.save(QStringLiteral("build/andong-city-emd.png")));
  const int emdBlue = countNearColor(cityEmd, QColor(0, 60, 120), 28);
  QVERIFY2(emdBlue > 200, qPrintable(QStringLiteral("emdBlue=%1").arg(emdBlue)));

  QgsVectorLayer* cad = AndongPack::loadAndongCadastral(QString());
  if (!cad) {
    delete emd;
    delete sat;
    QSKIP("cadastral SHP not on this PC");
  }
  const QImage citySat = renderLayers(QList<QgsMapLayer*>{sat}, city, QSize(400, 400));
  const QImage cityCad = renderLayers(QList<QgsMapLayer*>{cad, sat}, city, QSize(400, 400));
  const QImage closeSat = renderLayers(QList<QgsMapLayer*>{sat}, close, QSize(400, 400));
  const QImage closeCad = renderLayers(QList<QgsMapLayer*>{cad, sat}, close, QSize(400, 400));
  QVERIFY(closeCad.save(QStringLiteral("build/andong-close-cad.png")));
  const double cityDiff = meanAbsDiff(citySat, cityCad);
  const double closeDiff = meanAbsDiff(closeSat, closeCad);
  QVERIFY2(cityDiff < 2.5, qPrintable(QStringLiteral("cityDiff=%1").arg(cityDiff)));
  QVERIFY2(closeDiff > 4.0, qPrintable(QStringLiteral("closeDiff=%1").arg(closeDiff)));

  const QImage hidpi = renderLayers(QList<QgsMapLayer*>{sat}, close, QSize(1024, 1024));
  QVERIFY(hidpi.save(QStringLiteral("build/andong-close-hidpi.png")));
  QCOMPARE(hidpi.width(), 1024);

  delete cad;
  delete emd;
  delete sat;
}

void TestAndong::localRaster_rejectsRemoteXyz() {
  QVERIFY(AndongPack::satelliteRasterPath(QStringLiteral("data/andong")).contains(QStringLiteral("satellite")));
  auto* remote = AndongPack::loadLocalRaster(
      QStringLiteral("type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/nokey/Satellite/{z}/{y}/{x}.jpeg"),
      QStringLiteral("위성"));
  QVERIFY2(remote == nullptr, "offline viewer must not open XYZ/WMS");
}

#include "test_andong.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("D:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("D:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestAndong tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
