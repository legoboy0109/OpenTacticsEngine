#include "tsv.h"

namespace ote_native {

QString toTsv(const QList<QStringList>& rows) {
  QString out;
  for (int r = 0; r < rows.size(); ++r) {
    if (r) out += '\n';
    for (int c = 0; c < rows[r].size(); ++c) {
      if (c) out += '\t';
      const QString& cell = rows[r][c];
      if (cell.contains('\t') || cell.contains('\n') || cell.contains('\r') || cell.contains('"')) {
        QString q = cell;
        q.replace('"', QStringLiteral("\"\""));
        out += '"' + q + '"';
      } else {
        out += cell;
      }
    }
  }
  return out;
}

QList<QStringList> parseTsv(const QString& text) {
  QList<QStringList> rows;
  QStringList row;
  QString cell;
  bool inQuotes = false, cellWasQuoted = false, any = false;
  auto endCell = [&] { row << cell; cell.clear(); cellWasQuoted = false; };
  auto endRow = [&] { endCell(); rows << row; row.clear(); any = false; };
  for (int i = 0; i < text.size(); ++i) {
    const QChar ch = text[i];
    if (inQuotes) {
      if (ch == '"') {
        if (i + 1 < text.size() && text[i + 1] == '"') { cell += '"'; ++i; }
        else inQuotes = false;
      } else {
        cell += ch;
      }
      continue;
    }
    any = true;
    if (ch == '"' && cell.isEmpty() && !cellWasQuoted) { inQuotes = true; cellWasQuoted = true; }
    else if (ch == '\t') endCell();
    else if (ch == '\n') endRow();
    else if (ch == '\r') { if (i + 1 < text.size() && text[i + 1] == '\n') ++i; endRow(); }
    else cell += ch;
  }
  if (any || !cell.isEmpty() || !row.isEmpty() || cellWasQuoted) endRow();
  return rows;
}

}  // namespace ote_native
