// Tab-separated text, the format spreadsheet programs put on and read from the clipboard. A cell that holds a tab,
// a line break or a quote is wrapped in double quotes with inner quotes doubled, like Excel and LibreOffice do.
#pragma once
#include <QList>
#include <QString>
#include <QStringList>

namespace ote_native {

QString toTsv(const QList<QStringList>& rows);
// Rows of cells. A trailing line break does not make an extra row. Line breaks may be \n or \r\n.
QList<QStringList> parseTsv(const QString& text);

}  // namespace ote_native
