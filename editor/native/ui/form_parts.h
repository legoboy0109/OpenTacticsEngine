// Small widgets and helpers shared by the form builders (form_view.cpp, form_build.cpp, form_other.cpp).
#pragma once
#include <QLabel>
#include <QString>

#include "json_path.h"
#include "schema.h"

class QToolButton;
class QWidget;

namespace ote_native {

// A label that shows as much of its text as fits on one line and ends it with "…" (the summary of a collapsed list item must
// never wrap or push the buttons out of view).
class ElidedLabel : public QLabel {
 public:
  explicit ElidedLabel(const QString& text, QWidget* parent = nullptr);
  QSize minimumSizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* e) override;
};

QLabel* helpLabel(const SchemaNode& n, QWidget* parent);
QLabel* keyLabel(const QString& key, bool required, QWidget* parent);
QToolButton* smallButton(const QString& text, const QString& tip, QWidget* parent, const char* name);
// The words a list item is titled with: its id / name, or the name of its variant.
QString summaryOf(const SchemaNode& itemNode, const ote::Json& item);
JsonPath childPath(const JsonPath& p, const std::string& key);
JsonPath childPath(const JsonPath& p, size_t index);
// Lists, maps, sub-objects and variants get a full-width row under their key; scalars sit beside it.
bool isCompound(const SchemaNode& n);

}  // namespace ote_native
