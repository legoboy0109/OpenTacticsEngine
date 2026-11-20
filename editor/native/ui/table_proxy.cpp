#include "table_proxy.h"

#include "json_table_model.h"

namespace ote_native {

TableProxy::TableProxy(QObject* parent) : QSortFilterProxyModel(parent) {
  setSortRole(JsonTableModel::SortRole);
  setDynamicSortFilter(true);
}

void TableProxy::setFilterText(const QString& text) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  beginFilterChange();
  filter_ = text.trimmed();
  endFilterChange();
#else
  filter_ = text.trimmed();
  invalidateFilter();
#endif
}

bool TableProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const {
  const QVariant a = sourceModel()->data(left, JsonTableModel::SortRole);
  const QVariant b = sourceModel()->data(right, JsonTableModel::SortRole);
  // Absent cells count as greater than anything: last when ascending.
  if (!a.isValid() || !b.isValid()) return a.isValid() && !b.isValid();
  const bool an = a.typeId() == QMetaType::Double || a.typeId() == QMetaType::Int;
  const bool bn = b.typeId() == QMetaType::Double || b.typeId() == QMetaType::Int;
  if (an && bn) return a.toDouble() < b.toDouble();
  if (an != bn) return an;  // numbers before text
  return a.toString().compare(b.toString(), Qt::CaseInsensitive) < 0;
}

bool TableProxy::filterAcceptsRow(int sourceRow, const QModelIndex& parent) const {
  if (filter_.isEmpty()) return true;
  const int cols = sourceModel()->columnCount(parent);
  for (int c = 0; c < cols; ++c)
    if (sourceModel()->index(sourceRow, c, parent).data(Qt::DisplayRole).toString().contains(filter_, Qt::CaseInsensitive)) return true;
  return false;
}

}  // namespace ote_native
