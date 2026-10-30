#pragma once

#include <QColor>
#include <QString>

class QgsCoordinateReferenceSystem;
class QgsGeometry;
class QgsMapCanvas;
class QgsMapLayer;
class QgsProject;
class QgsVectorLayer;

// 안동뷰어가 쓰는 지도 도우미.
//
// 원래 ka-hgis 의 ka_core(LayerOps, 4600줄)를 통째로 링크했는데, 안동 코드가 실제로
// 쓰는 것은 아래 함수들뿐이었다. 2026-09-08 프로젝트를 나누면서 그것만 옮겨
// hgis 소스 의존을 끊었다. 이름과 서명은 그대로라 호출부는 고치지 않았다.
class LayerOps {
public:
  static constexpr const char* kGroupSurveyData = "조사 데이터";
  static constexpr const char* kGroupReference = "참조 지도";
  static constexpr const char* kPropLayerKey = "ka_hgis/layer_key";
  static constexpr const char* kPropLayerRole = "ka_hgis/layer_role";
  static constexpr const char* kAdminEmdKey = "admin_emd";
  static constexpr const char* kRoleSurvey = "survey";
  static constexpr const char* kRoleReference = "reference";
  static constexpr const char* kPropAlignPending = "ka_hgis/align_pending";

  static void applyCanvasScreenDpi(QgsMapCanvas* canvas);
  static bool applyNameAttributeLabels(QgsVectorLayer* layer, const QString& fieldName = QString(),
                                       double fontSizePt = 5.0, bool showArea = false);
  static bool applySimpleVectorStyle(QgsVectorLayer* layer, const QColor& fill, const QColor& stroke,
                                     double strokeWidthMm, double markerSizeMm = 3.5,
                                     bool noFill = false, bool noStroke = false, bool dashed = false);
  static QString detectNameField(const QgsVectorLayer* layer);
  static bool ensureOtfEnabled(QgsProject* project, QgsMapCanvas* canvas, const QString& workCrsAuthId);
  static bool labelsVisible(const QgsMapLayer* layer);
  static void markReferenceLayer(QgsMapLayer* layer);
  // 외부에서 받은 SHP 파일(.cpg 없는 경우)의 한글 깨짐을 방지하기 위한 인코딩 준비 및 변경
  static QString prepareShapefileEncoding(const QString& shpPath);
  static bool setLabelsVisible(QgsMapLayer* layer, bool on);
  static bool setShapefileEncoding(QgsVectorLayer* layer, const QString& encoding);
  static QgsVectorLayer* upsertAdminEmdMask(QgsProject* project, const QgsGeometry& geom,
                                            const QgsCoordinateReferenceSystem& srcCrs,
                                            const QString& workCrsAuthId, const QString& titleKo);
  static bool applyAreaM2Labels(QgsVectorLayer* layer);
  static bool isAdminEmdLayer(const QgsMapLayer* layer);
  static QString layerKeyOf(const QgsMapLayer* layer);
  static bool labelShowArea(const QgsVectorLayer* layer, bool defaultShow = false);
  static QString currentLabelField(const QgsVectorLayer* layer);
  static bool applyInvertedPaperMask(QgsVectorLayer* layer);
  static void applyLegendCrsLabel(QgsMapLayer* layer);
};
