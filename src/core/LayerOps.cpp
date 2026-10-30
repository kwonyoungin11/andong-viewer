#include "core/LayerOps.h"

#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>
#include <cmath>
#include <limits>
#include <algorithm>
#include <QPainter>
#include <QScreen>
#include <QSize>
#include <QUrl>
#include <QWindow>
#include <QColor>
#include <QFont>
#include <QDir>
#include <QSet>
#include <QTemporaryFile>
#include <QPointer>
#include <QTimer>
#include <functional>
#include <qgis.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsbrightnesscontrastfilter.h>
#include <qgsmapcanvas.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgscoordinatetransform.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgslinestring.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsinvertedpolygonrenderer.h>
#include <qgssymbol.h>
#include <qgssymbollayer.h>
#include <qgsfillsymbol.h>
#include <qgsfillsymbollayer.h>
#include <qgslinesymbol.h>
#include <qgslinesymbollayer.h>
#include <qgsmarkersymbol.h>
#include <qgsrenderer.h>
#include <qgsrectangle.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsbilinearrasterresampler.h>
#include <qgsrasterresamplefilter.h>
#include <qgsrasterdataprovider.h>
#include <qgsvectordataprovider.h>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsrastershader.h>
#include <qgscolorrampshader.h>
#include <qgscolorramplegendnodesettings.h>
#include <qgshillshaderenderer.h>
#include <qgsrasterbandstats.h>
#include <cpl_conv.h>
#include <cpl_error.h>
#include <gdal.h>
#include <gdal_utils.h>
#include <ogr_api.h>
#include <qgsnetworkaccessmanager.h>
#include <qgslayertreegroup.h>

static QString stripLegendCrsSuffix(QString name) {
  int bracket = name.lastIndexOf(QStringLiteral(" [EPSG:"));
  if (bracket < 0) bracket = name.lastIndexOf(QStringLiteral(" [CRS"));
  if (bracket >= 0) name = name.left(bracket).trimmed();
  return name;
}

static QString friendlyLegendName(const QString& name) {
  const QString base = stripLegendCrsSuffix(name);
  if (base.contains(QStringLiteral("지적"))) {
    const bool bon = base.contains(QStringLiteral("본번"));
    const bool bu = base.contains(QStringLiteral("부번"));
    if (bon && !bu) return QStringLiteral("지적 본번");
    if (bu && !bon) return QStringLiteral("지적 부번");
    return QStringLiteral("지적");
  }
  if (base.contains(QStringLiteral("위성")))
    return QStringLiteral("위성");
  return base;
}

void LayerOps::applyCanvasScreenDpi(QgsMapCanvas* canvas) {
  if (!canvas) return;
  // FHD 100% → 와이드 → 4K 150–200% (DPR 1.0–2.0). 배율은 PassThrough 그대로.
  // 타일 격자는 위젯×DPR 이어야 화면을 채운다. outputSize 가 논리 크기(또는 옛 값)로
  // 남으면 줌할 때 위성만 한 덩어리 잘린 것처럼 보인다. 값이 달라질 때만 쓴다.
  const qreal dpr = canvas->devicePixelRatioF();
  const qreal pixelDpr = dpr > 0.05 ? dpr : 1.0;
  if (!qFuzzyCompare(canvas->mapSettings().devicePixelRatio(), static_cast<float>(pixelDpr)))
    canvas->mapSettings().setDevicePixelRatio(static_cast<float>(pixelDpr));
  if (canvas->width() >= 2 && canvas->height() >= 2) {
    const QSize want(qMax(1, qRound(double(canvas->width()) * pixelDpr)),
                     qMax(1, qRound(double(canvas->height()) * pixelDpr)));
    if (canvas->mapSettings().outputSize() != want)
      canvas->mapSettings().setOutputSize(want);
  }
  QWindow* wh = canvas->windowHandle();
  if (!wh && canvas->window())
    wh = canvas->window()->windowHandle();
  // 96×DPR 과 윈도우 논리 DPI(보통 같음)로 축척 분모를 모니터마다 같게 둔다.
  double dpi = 96.0 * double(pixelDpr);
  if (wh && wh->screen()) {
    const double logical = wh->screen()->logicalDotsPerInch();
    if (logical > 10.0)
      dpi = logical;
  }
  if (!qFuzzyCompare(canvas->mapSettings().outputDpi(), dpi))
    canvas->mapSettings().setOutputDpi(dpi);
}

bool LayerOps::applyNameAttributeLabels(QgsVectorLayer* layer, const QString& fieldName,
                                       double fontSizePt, bool showArea) {
  if (!layer || !layer->isValid()) return false;

  QString targetField = fieldName;
  if (targetField.isEmpty())
    targetField = detectNameField(layer);

  if (fontSizePt <= 0.0)
    fontSizePt = 5.0;

  if (!targetField.isEmpty())
    layer->setCustomProperty(QStringLiteral("ka_hgis/label_field"), targetField);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_font_size"), fontSizePt);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_show_area"), showArea);

  QgsPalLayerSettings s;
  s.drawLabels = true;

  const bool isPolygon = (layer->geometryType() == Qgis::GeometryType::Polygon);
  const bool hasField = !targetField.isEmpty() && layer->fields().indexOf(targetField) >= 0;

  if (hasField && isPolygon && showArea) {
    s.fieldName = QStringLiteral("coalesce(\"%1\", '') || '\\n(' || format_number(area($geometry), 1) || ' ㎡)'")
                      .arg(targetField);
    s.isExpression = true;
  } else if (hasField) {
    s.fieldName = QStringLiteral("\"%1\"").arg(targetField);
    s.isExpression = true;
  } else if (isPolygon && showArea) {
    s.fieldName = QStringLiteral("format_number(area($geometry), 1) || ' ㎡'");
    s.isExpression = true;
  } else if (isPolygon) {
    s.fieldName = QStringLiteral("format_number(area($geometry), 1) || ' ㎡'");
    s.isExpression = true;
  } else if (layer->fields().count() > 0) {
    s.fieldName = QStringLiteral("\"%1\"").arg(layer->fields().at(0).name());
    s.isExpression = true;
  } else {
    return false;
  }

  if (isPolygon) {
    s.placement = Qgis::LabelPlacement::OverPoint;
    s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);
  } else if (layer->geometryType() == Qgis::GeometryType::Line) {
    s.placement = Qgis::LabelPlacement::Line;
  } else {
    s.placement = Qgis::LabelPlacement::AroundPoint;
  }

  QgsLabelObstacleSettings obs = s.obstacleSettings();
  obs.setIsObstacle(false);
  s.setObstacleSettings(obs);

  QgsTextFormat fmt;
  QFont font = fmt.font();
  font.setFamily(QStringLiteral("Malgun Gothic"));
  font.setPointSizeF(fontSizePt);
  font.setBold(true);
  fmt.setFont(font);
  fmt.setSize(fontSizePt);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  fmt.setColor(QColor(31, 35, 40));

  QgsTextBufferSettings buf = fmt.buffer();
  buf.setEnabled(true);
  buf.setSize(0.8);
  buf.setColor(QColor(255, 255, 255, 230));
  fmt.setBuffer(buf);
  s.setFormat(fmt);

  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
  layer->triggerRepaint();
  return true;
}

bool LayerOps::applySimpleVectorStyle(QgsVectorLayer* layer, const QColor& fillIn, const QColor& strokeIn,
                                      double strokeWidthMm, double markerSizeMm, bool noFill,
                                      bool noStroke, bool dashed) {
  if (!layer || !layer->isValid()) return false;
  QColor fill = fillIn.isValid() ? fillIn : QColor(37, 99, 235, 90);
  QColor stroke = strokeIn.isValid() ? strokeIn : QColor(37, 99, 235, 255);
  if (strokeWidthMm <= 0.0) strokeWidthMm = 1.0;
  if (markerSizeMm <= 0.0) markerSizeMm = 3.5;
  if (noFill) fill = QColor(0, 0, 0, 0);
  if (noStroke) stroke = QColor(0, 0, 0, 0);
  if (noFill && noStroke) {
    noStroke = false;
    stroke = QColor(100, 100, 100, 255);
    strokeWidthMm = 0.4;
  }

  const Qgis::GeometryType gt = layer->geometryType();
  QgsSymbol* sym = nullptr;
  if (gt == Qgis::GeometryType::Polygon) {
    QVariantMap props{
        {QStringLiteral("color"), fill.name(QColor::HexArgb)},
        {QStringLiteral("style"), noFill ? QStringLiteral("no") : QStringLiteral("solid")},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), QString::number(noStroke ? 0.0 : strokeWidthMm)},
        {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("outline_style"),
         noStroke ? QStringLiteral("no") : (dashed ? QStringLiteral("dash") : QStringLiteral("solid"))},
    };
    auto fs = QgsFillSymbol::createSimple(props);
    sym = fs.release();
  } else if (gt == Qgis::GeometryType::Line) {
    auto ls = QgsLineSymbol::createSimple({
        {QStringLiteral("line_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("line_width"), QString::number(noStroke ? 0.0 : strokeWidthMm)},
        {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("line_style"),
         noStroke ? QStringLiteral("no") : (dashed ? QStringLiteral("dash") : QStringLiteral("solid"))},
    });
    sym = ls.release();
  } else if (gt == Qgis::GeometryType::Point) {
    auto ms = QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("circle")},
        {QStringLiteral("color"), noFill ? QStringLiteral("#00000000") : fill.name(QColor::HexArgb)},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), noStroke ? QStringLiteral("0") : QStringLiteral("0.6")},
        {QStringLiteral("outline_style"), noStroke ? QStringLiteral("no") : QStringLiteral("solid")},
        {QStringLiteral("size"), QString::number(markerSizeMm)},
        {QStringLiteral("size_unit"), QStringLiteral("MM")},
    });
    sym = ms.release();
  } else {
    return false;
  }
  if (!sym) return false;

  layer->setCustomProperty(QStringLiteral("ka_hgis/style_fill"), fill.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_stroke"), stroke.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_width_mm"), strokeWidthMm);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_marker_mm"), markerSizeMm);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_no_fill"), noFill);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_no_stroke"), noStroke);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_dashed"), dashed);
  layer->setRenderer(new QgsSingleSymbolRenderer(sym));
  layer->triggerRepaint();
  return true;
}

QString LayerOps::detectNameField(const QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return {};
  const QgsFields fds = layer->fields();
  if (fds.isEmpty()) return {};

  // 1. Cultural heritage & Archaeology (문화유적분포지도, 발굴/지표조사구역, 유적, 고고학)
  const QStringList heritageCandidates = {
    QStringLiteral("사업명"), QStringLiteral("유적명"), QStringLiteral("유적명칭"),
    QStringLiteral("조사명"), QStringLiteral("보고서명"), QStringLiteral("소재지"),
    QStringLiteral("번호"), QStringLiteral("유적번호"), QStringLiteral("site_no"),
    QStringLiteral("yujuk_nm"), QStringLiteral("yujeok_nm"), QStringLiteral("site_name"),
    QStringLiteral("hist_nm"), QStringLiteral("rem_nm"), QStringLiteral("명칭"),
    QStringLiteral("name"), QStringLiteral("title")
  };
  for (const QString& c : heritageCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 2. Cadastral (연속지적도, 지적도, 토지)
  const QStringList cadCandidates = {
    QStringLiteral("jibun"), QStringLiteral("지번"), QStringLiteral("a2"),
    QStringLiteral("a1"), QStringLiteral("pnu")
  };
  for (const QString& c : cadCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 3. Archaeology features (유구, 도면)
  const QStringList featCandidates = {
    QStringLiteral("feature_no"), QStringLiteral("유구번호"), QStringLiteral("유구명"),
    QStringLiteral("호수"), QStringLiteral("kind_ko"), QStringLiteral("kind")
  };
  for (const QString& c : featCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 4. Digital topo / buildings / roads (수치지형도, 건물, 도로, 지명)
  const QStringList topoCandidates = {
    QStringLiteral("buld_nm"), QStringLiteral("건물명"), QStringLiteral("지명"),
    QStringLiteral("road_nm"), QStringLiteral("도로명"), QStringLiteral("label"),
    QStringLiteral("kor_nm")
  };
  for (const QString& c : topoCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 5. Scan string fields containing name-related keywords
  for (int i = 0; i < fds.count(); ++i) {
    const QgsField f = fds.at(i);
    const QString n = f.name().toLower();
    if (f.type() == QMetaType::QString || f.typeName().contains(QLatin1String("char"), Qt::CaseInsensitive) ||
        f.typeName().contains(QLatin1String("string"), Qt::CaseInsensitive)) {
      if (n.contains(QStringLiteral("명")) || n.contains(QLatin1String("name")) ||
          n.contains(QLatin1String("title")) || n.contains(QLatin1String("label")) ||
          n.contains(QStringLiteral("지번")) || n.contains(QLatin1String("jibun"))) {
        return f.name();
      }
    }
  }

  // 6. First string field if available
  for (int i = 0; i < fds.count(); ++i) {
    const QgsField f = fds.at(i);
    if (f.type() == QMetaType::QString)
      return f.name();
  }

  return fds.at(0).name();
}

bool LayerOps::ensureOtfEnabled(QgsProject* project, QgsMapCanvas* canvas, const QString& workCrsAuthId) {
  const QString auth = workCrsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:5186") : workCrsAuthId.trimmed();
  const QgsCoordinateReferenceSystem crs(auth);
  if (!crs.isValid()) return false;

  QgsCoordinateTransformContext ctx;
  if (project) {
    ctx = project->transformContext();
    project->setTransformContext(ctx);
    project->setCrs(crs);
  }
  if (canvas)
    canvas->setDestinationCrs(crs);
  return true;
}

bool LayerOps::labelsVisible(const QgsMapLayer* layer) {
  const auto* vl = qobject_cast<const QgsVectorLayer*>(layer);
  return vl && vl->isValid() && vl->labelsEnabled();
}

void LayerOps::markReferenceLayer(QgsMapLayer* layer) {
  if (!layer) return;
  layer->setCustomProperty(QString::fromUtf8(kPropLayerRole), QString::fromUtf8(kRoleReference));
}

QString LayerOps::prepareShapefileEncoding(const QString& shpPath) {
  QFileInfo fi(shpPath);
  if (!fi.exists() || fi.suffix().compare(QLatin1String("shp"), Qt::CaseInsensitive) != 0)
    return QString();

  const QString dir = fi.absolutePath();
  const QString base = fi.completeBaseName();
  const QString cpgPath = QDir(dir).filePath(base + QStringLiteral(".cpg"));
  const QString dbfPath = QDir(dir).filePath(base + QStringLiteral(".dbf"));

  // 1. 이미 .cpg 파일이 존재하는 경우 해당 인코딩을 GDAL에 적용
  if (QFile::exists(cpgPath)) {
    QFile cpgFile(cpgPath);
    if (cpgFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      const QString content = QString::fromUtf8(cpgFile.readAll()).trimmed();
      cpgFile.close();
      if (!content.isEmpty()) {
        const QString upper = content.toUpper();
        if (upper.contains(QLatin1String("UTF-8")) || upper.contains(QLatin1String("UTF8"))) {
          CPLSetConfigOption("SHAPE_ENCODING", "UTF-8");
          return QStringLiteral("UTF-8");
        }
        if (upper.contains(QLatin1String("CP949")) || upper.contains(QLatin1String("949")) ||
            upper.contains(QLatin1String("EUC-KR")) || upper.contains(QLatin1String("ANSI")) ||
            upper.contains(QLatin1String("SYSTEM"))) {
          CPLSetConfigOption("SHAPE_ENCODING", "CP949");
          return QStringLiteral("CP949");
        }
        CPLSetConfigOption("SHAPE_ENCODING", content.toLatin1().constData());
        return content;
      }
    }
  }

  // 2. .cpg 파일이 없는 경우: .dbf 앞부분(16KB)을 검사하여 UTF-8 vs CP949 자동 판별
  QString detected = QStringLiteral("CP949"); // 한국 공공데이터·지적도·유적도 SHP 기본값
  if (QFile::exists(dbfPath)) {
    QFile dbfFile(dbfPath);
    if (dbfFile.open(QIODevice::ReadOnly)) {
      const QByteArray sample = dbfFile.read(32768);
      dbfFile.close();

      bool hasNonAscii = false;
      bool validUtf8 = true;
      int utf8MultiByteCount = 0;

      const unsigned char* bytes = reinterpret_cast<const unsigned char*>(sample.constData());
      const int len = sample.size();
      for (int i = 0; i < len; ++i) {
        const unsigned char c = bytes[i];
        if (c > 127) {
          hasNonAscii = true;
          if ((c & 0xE0) == 0xC0 && c >= 0xC2) { // 2-byte UTF-8
            if (i + 1 < len && (bytes[i + 1] & 0xC0) == 0x80) {
              ++utf8MultiByteCount;
              ++i;
            } else {
              validUtf8 = false;
              break;
            }
          } else if ((c & 0xF0) == 0xE0) { // 3-byte UTF-8 (한글 완성형은 EA..ED)
            if (i + 2 < len && (bytes[i + 1] & 0xC0) == 0x80 && (bytes[i + 2] & 0xC0) == 0x80) {
              ++utf8MultiByteCount;
              i += 2;
            } else {
              validUtf8 = false;
              break;
            }
          } else if ((c & 0xF8) == 0xF0 && c <= 0xF4) { // 4-byte UTF-8
            if (i + 3 < len && (bytes[i + 1] & 0xC0) == 0x80 && (bytes[i + 2] & 0xC0) == 0x80 &&
                (bytes[i + 3] & 0xC0) == 0x80) {
              ++utf8MultiByteCount;
              i += 3;
            } else {
              validUtf8 = false;
              break;
            }
          } else {
            validUtf8 = false;
            break;
          }
        }
      }

      // 비ASCII 바이트가 있고 온전한 UTF-8 멀티바이트가 충분히 존재할 때만 UTF-8
      if (hasNonAscii && validUtf8 && utf8MultiByteCount >= 4) {
        detected = QStringLiteral("UTF-8");
      } else {
        // CP949 바이트이거나 영문/숫자 헤더만 있는 경우: 한국 GIS 환경 관례상 CP949
        detected = QStringLiteral("CP949");
      }
    }
  }

  // 3. .cpg 파일이 없으면 자동 생성하여 영구 보존 (다음번 및 타 GIS 소프트웨어 호환)
  if (!QFile::exists(cpgPath)) {
    QFile outCpg(cpgPath);
    if (outCpg.open(QIODevice::WriteOnly | QIODevice::Text)) {
      outCpg.write(detected.toUtf8() + "\n");
      outCpg.close();
    }
  }

  // 4. GDAL 드라이버 레벨에서 SHAPE_ENCODING 옵션 설정
  CPLSetConfigOption("SHAPE_ENCODING", detected.toLatin1().constData());
  return detected;
}

bool LayerOps::setLabelsVisible(QgsMapLayer* layer, bool on) {
  auto* vl = qobject_cast<QgsVectorLayer*>(layer);
  if (!vl || !vl->isValid())
    return false;
  if (on && !vl->labeling()) {
    const QString nf = detectNameField(vl);
    if (!nf.isEmpty()) {
      applyNameAttributeLabels(vl, nf, 5.0, false);
      return true;
    } else if (vl->geometryType() == Qgis::GeometryType::Polygon && applyAreaM2Labels(vl)) {
      return true;
    }
  }
  vl->setLabelsEnabled(on);
  vl->triggerRepaint();
  return true;
}

bool LayerOps::setShapefileEncoding(QgsVectorLayer* layer, const QString& encoding) {
  if (!layer || !layer->isValid()) return false;
  const QString src = layer->source().split(QLatin1Char('|')).first();
  QFileInfo fi(src);
  if (fi.exists() && fi.suffix().compare(QLatin1String("shp"), Qt::CaseInsensitive) == 0) {
    const QString cpgPath = QDir(fi.absolutePath()).filePath(fi.completeBaseName() + QStringLiteral(".cpg"));
    QFile cpgFile(cpgPath);
    if (cpgFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      cpgFile.write(encoding.toUtf8() + "\n");
      cpgFile.close();
    }
  }
  CPLSetConfigOption("SHAPE_ENCODING", encoding.toLatin1().constData());
  layer->setProviderEncoding(encoding);
  layer->reload();
  layer->updateFields();
  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* LayerOps::upsertAdminEmdMask(QgsProject* project, const QgsGeometry& geom,
                                             const QgsCoordinateReferenceSystem& srcCrs,
                                             const QString& workCrsAuthId, const QString& titleKo) {
  if (!project || geom.isEmpty()) return nullptr;
  const QString destAuth =
      workCrsAuthId.isEmpty() ? QStringLiteral("EPSG:5186") : workCrsAuthId;
  QgsGeometry local = geom;
  const QgsCoordinateReferenceSystem dest(destAuth);
  if (srcCrs.isValid() && dest.isValid() && srcCrs != dest) {
    QgsCoordinateTransform xf(srcCrs, dest, project->transformContext());
    xf.setBallparkTransformsAreAppropriate(true);
    if (local.transform(xf) != Qgis::GeometryOperationResult::Success)
      return nullptr;
  }

  QgsVectorLayer* layer = nullptr;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (isAdminEmdLayer(l)) {
      layer = qobject_cast<QgsVectorLayer*>(l);
      break;
    }
  }
  if (!layer) {
    const QString title =
        titleKo.isEmpty() ? QStringLiteral("읍면동 마스크") : titleKo;
    layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=%1").arg(destAuth), title,
                               QStringLiteral("memory"));
    if (!layer->isValid()) {
      delete layer;
      return nullptr;
    }
    markReferenceLayer(layer);
    layer->setCustomProperty(QStringLiteral("ka_hgis/admin_emd"), true);
    project->addMapLayer(layer, true);
  } else if (!titleKo.isEmpty()) {
    layer->setName(titleKo);
  }
  if (layer && layerKeyOf(layer) == QLatin1String(kAdminEmdKey))
    layer->removeCustomProperty(QString::fromUtf8(kPropLayerKey));

  if (!layer->isValid()) return nullptr;
  if (layer->isEditable())
    layer->rollBack();
  if (!layer->startEditing()) return nullptr;
  QgsFeatureIds ids;
  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature existing;
  while (it.nextFeature(existing))
    ids.insert(existing.id());
  if (!ids.isEmpty())
    layer->deleteFeatures(ids);
  QgsFeature f(layer->fields());
  f.setGeometry(local);
  if (!layer->addFeature(f)) {
    layer->rollBack();
    return nullptr;
  }
  if (!layer->commitChanges()) {
    layer->rollBack();
    return nullptr;
  }
  applyInvertedPaperMask(layer);
  applyLegendCrsLabel(layer);
  return layer;
}

bool LayerOps::applyAreaM2Labels(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (layer->geometryType() != Qgis::GeometryType::Polygon) return false;

  QgsPalLayerSettings s;
  s.drawLabels = true;
  s.fieldName = QStringLiteral("format_number(area($geometry), 2) || ' ㎡'");
  s.isExpression = true;
  s.placement = Qgis::LabelPlacement::OverPoint;
  s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);

  QgsLabelObstacleSettings obs = s.obstacleSettings();
  obs.setIsObstacle(false);
  s.setObstacleSettings(obs);

  QgsTextFormat fmt;
  QFont font = fmt.font();
  font.setFamily(QStringLiteral("Malgun Gothic"));
  font.setPointSize(5);
  font.setBold(true);
  fmt.setFont(font);
  fmt.setSize(5);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  fmt.setColor(QColor(15, 23, 42));
  QgsTextBufferSettings buf = fmt.buffer();
  buf.setEnabled(true);
  buf.setSize(0.8);
  buf.setColor(QColor(255, 255, 255, 230));
  fmt.setBuffer(buf);
  s.setFormat(fmt);

  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
  layer->triggerRepaint();
  return true;
}

bool LayerOps::isAdminEmdLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  if (layer->customProperty(QStringLiteral("ka_hgis/admin_emd")).toBool()) return true;
  return layerKeyOf(layer) == QLatin1String(kAdminEmdKey);
}

QString LayerOps::layerKeyOf(const QgsMapLayer* layer) {
  if (!layer) return {};
  return layer->customProperty(QString::fromUtf8(kPropLayerKey)).toString();
}

bool LayerOps::labelShowArea(const QgsVectorLayer* layer, bool defaultShow) {
  if (!layer) return defaultShow;
  const QVariant v = layer->customProperty(QStringLiteral("ka_hgis/label_show_area"));
  if (v.isValid())
    return v.toBool();
  if (layer->labeling()) {
    const QString expr = layer->labeling()->settings().fieldName;
    if (expr.contains(QLatin1String("area($geometry)")))
      return true;
  }
  return defaultShow;
}

QString LayerOps::currentLabelField(const QgsVectorLayer* layer) {
  if (!layer) return {};
  const QString f = layer->customProperty(QStringLiteral("ka_hgis/label_field")).toString();
  if (!f.isEmpty()) return f;
  return detectNameField(layer);
}

bool LayerOps::applyInvertedPaperMask(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  auto fill = QgsFillSymbol::createSimple({
      {QStringLiteral("color"), QStringLiteral("255,255,255,255")},
      {QStringLiteral("style"), QStringLiteral("solid")},
      {QStringLiteral("outline_style"), QStringLiteral("no")},
  });
  if (!fill) return false;
  auto* embedded = new QgsSingleSymbolRenderer(fill.release());
  auto* inv = new QgsInvertedPolygonRenderer(embedded);
  inv->setPreprocessingEnabled(true);
  layer->setRenderer(inv);
  layer->triggerRepaint();
  return true;
}

void LayerOps::applyLegendCrsLabel(QgsMapLayer* layer) {
  if (!layer) return;
  const QString shown = friendlyLegendName(layer->name());
  if (!shown.isEmpty() && layer->name() != shown)
    layer->setName(shown);
  const QString auth = layer->crs().isValid() ? layer->crs().authid() : QString();
  if (!auth.isEmpty()) {
    layer->setAbstract(QStringLiteral("좌표계 %1").arg(auth));
    layer->setCustomProperty(QStringLiteral("ka_hgis/crs_label"), auth);
  }
}
