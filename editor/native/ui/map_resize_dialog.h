// "Resize map": new width and height, which part of the map stays put (a 3 x 3 anchor grid), and what the new squares are
// filled with. It lists, live, the units / structures that the chosen size would cut off.
#pragma once
#include <QDialog>

#include "map_grid.h"

class QComboBox;
class QLabel;
class QSpinBox;
class QButtonGroup;

namespace ote_native {

class MapPainter;

class MapResizeDialog : public QDialog {
  Q_OBJECT
 public:
  MapResizeDialog(MapPainter* painter, QWidget* parent = nullptr);
  ote_editor::ResizeSpec spec() const;

  // Plain-words list of what a resize removes ("2 units, 1 structure: knight (gilda) at 9, 3 ..."), empty when nothing.
  static QString describeLost(const std::vector<ote_editor::LostThing>& lost);

 private:
  void refreshWarning();

  MapPainter* painter_;
  QSpinBox *width_, *height_;
  QButtonGroup* anchors_;
  QComboBox* fill_;
  QLabel* warning_;
};

}  // namespace ote_native
