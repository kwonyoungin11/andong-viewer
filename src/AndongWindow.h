#pragma once

#include <QMainWindow>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QString>

class QEvent;
class QResizeEvent;
class QShowEvent;

class QgsMapCanvas;
class QgsMapToolPan;
class QgsMapLayer;
class QgsVectorLayer;
class QVBoxLayout;
class QLineEdit;
class QLabel;
class QCheckBox;
class QSlider;

class AndongWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit AndongWindow(QWidget* parent = nullptr);
  ~AndongWindow() override;

protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
  void findName();
  void zoomCity();
  void rebuildCanvas();
  void onSatOpacity(int percent);
  void onScaleChanged();

private:
  struct Row {
    QgsMapLayer* layer = nullptr;
    QCheckBox* vis = nullptr;
    QCheckBox* labels = nullptr;
  };

  void setupUi();
  void loadPack();
  void addGroup(const QString& title);
  void addLayerRow(const QString& title, QgsMapLayer* layer, bool hasLabels);
  void applyTheme();
  void refreshCanvasDpi();
  void inspectAt(const QPoint& widgetPos);
  void setStatusBase(const QString& text);
  void ensureCadastralLoaded();

  QgsMapCanvas* m_canvas = nullptr;
  QgsMapToolPan* m_pan = nullptr;
  QWidget* m_panel = nullptr;
  QVBoxLayout* m_panelLay = nullptr;
  QLineEdit* m_search = nullptr;
  QLabel* m_status = nullptr;
  QLabel* m_detail = nullptr;
  QString m_statusBase;
  QPoint m_pressPos;
  bool m_dragging = false;
  QCheckBox* m_satBox = nullptr;
  QSlider* m_satOpacity = nullptr;
  QLabel* m_satOpacityValue = nullptr;
  QCheckBox* m_cadBox = nullptr;
  QCheckBox* m_jibunBox = nullptr;
  QgsMapLayer* m_satellite = nullptr;
  QgsMapLayer* m_cadastral = nullptr;
  QgsMapLayer* m_jibun = nullptr;
  QgsVectorLayer* m_mask = nullptr;
  QList<Row> m_rows;
};
