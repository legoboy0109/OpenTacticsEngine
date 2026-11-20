// Sorting and filtering of the table, for looking only: the JSON array keeps the order the file has, whatever the
// header click or the filter box says. Numbers sort as numbers (2 before 10), text without regard to case,
// empty cells last.
#pragma once
#include <QSortFilterProxyModel>

namespace ote_native {

class TableProxy : public QSortFilterProxyModel {
  Q_OBJECT
 public:
  explicit TableProxy(QObject* parent = nullptr);

  void setFilterText(const QString& text);
  const QString& filterText() const { return filter_; }
  // True while the rows on screen are not in file order (a sort or a filter is active).
  bool isViewOnlyOrder() const { return sortColumn() >= 0 || !filter_.isEmpty(); }

 protected:
  bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
  bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

 private:
  QString filter_;
};

}  // namespace ote_native
