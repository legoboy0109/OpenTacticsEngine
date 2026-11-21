// The grip at the left of every list item of a form. Dragging it (press, move past the drag distance, release) moves the item
// within its list or into another list of the same kind; the drag is made of plain mouse events (no QDrag, whose blocking loop
// cannot be scripted or photographed), so a test can press, move, look at the picture and release. The keyboard does the same
// with Alt+Up / Alt+Down. The handle only reports; FormView (form_drag.cpp) decides where the item may go and moves it.
#pragma once
#include <QPoint>
#include <QToolButton>

namespace ote_native {

class DragHandle : public QToolButton {
  Q_OBJECT
 public:
  explicit DragHandle(QWidget* parent = nullptr);
  bool isDragging() const { return dragging_; }

 signals:
  void dragStarted(QPoint global);
  void dragMoved(QPoint global);
  void dragDropped(QPoint global);
  void dragCancelled();
  void stepRequested(int delta);  // Alt+Up -1, Alt+Down +1

 protected:
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;

 private:
  bool pressed_ = false, dragging_ = false;
  QPoint pressAt_;
};

}  // namespace ote_native
