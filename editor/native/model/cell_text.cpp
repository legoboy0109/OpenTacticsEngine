#include "cell_text.h"

#include <QLocale>
#include <cmath>

namespace ote_native {

QString jsonToCellText(const ote::Json& v) {
  switch (v.type()) {
    case ote::Json::Type::Null: return QStringLiteral("null");
    case ote::Json::Type::Bool: return v.asBool() ? QStringLiteral("true") : QStringLiteral("false");
    case ote::Json::Type::Number: {
      const double d = v.asNumber();
      // Shortest text that reads back as exactly the same number: 0.1 shows as 0.1, 12 as 12.
      return QLocale::c().toString(d, 'g', QLocale::FloatingPointShortest);
    }
    case ote::Json::Type::String: return QString::fromStdString(v.asString());
    default: return QString::fromStdString(v.dump(0));
  }
}

ote::Json defaultValueFor(ote::Json::Type t) {
  switch (t) {
    case ote::Json::Type::Bool: return ote::Json::makeBool(false);
    case ote::Json::Type::Number: return ote::Json::makeNumber(0);
    case ote::Json::Type::String: return ote::Json::makeString("");
    case ote::Json::Type::Array: return ote::Json::makeArray();
    case ote::Json::Type::Object: return ote::Json::makeObject();
    default: return ote::Json();
  }
}

namespace {
CellParse fail(const QString& why) {
  CellParse r;
  r.error = why;
  return r;
}
CellParse good(ote::Json v) {
  CellParse r;
  r.ok = true;
  r.value = std::move(v);
  return r;
}
CellParse parseJsonText(const QString& text) {
  ote::Json parsed;
  std::string err;
  if (!ote::Json::parse(text.toStdString(), parsed, err)) return fail(QStringLiteral("Not valid JSON (%1)").arg(QString::fromStdString(err)));
  return good(std::move(parsed));
}
}  // namespace

CellParse parseCellText(const QString& text, const ote::Json* current, ote::Json::Type columnHint) {
  ote::Json::Type target = (current && !current->isNull()) ? current->type() : columnHint;
  switch (target) {
    case ote::Json::Type::Number: {
      bool ok = false;
      const QString t = text.trimmed();
      const double d = QLocale::c().toDouble(t, &ok);
      if (!ok || std::isnan(d) || std::isinf(d)) return fail(QStringLiteral("\"%1\" is not a number; this cell holds a number.").arg(text));
      return good(ote::Json::makeNumber(d));
    }
    case ote::Json::Type::Bool: {
      const QString t = text.trimmed().toLower();
      if (t == QStringLiteral("true") || t == QStringLiteral("1")) return good(ote::Json::makeBool(true));
      if (t == QStringLiteral("false") || t == QStringLiteral("0")) return good(ote::Json::makeBool(false));
      return fail(QStringLiteral("\"%1\" is not true or false; this cell holds a yes/no value.").arg(text));
    }
    case ote::Json::Type::Array:
    case ote::Json::Type::Object: return parseJsonText(text);
    case ote::Json::Type::String: return good(ote::Json::makeString(text.toStdString()));
    default: {
      // Absent cell with a column that has no usual type, or an existing null: take JSON if it is JSON, else text.
      CellParse asJson = parseJsonText(text);
      if (asJson.ok) return asJson;
      return good(ote::Json::makeString(text.toStdString()));
    }
  }
}

}  // namespace ote_native
