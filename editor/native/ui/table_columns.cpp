#include "table_columns.h"

#include "id_source.h"

namespace ote_native {

namespace {

ote::Json::Type hintFor(const SchemaNode& n) {
  switch (n.kind) {
    case SchemaNode::Kind::Int: case SchemaNode::Kind::Number: return ote::Json::Type::Number;
    case SchemaNode::Kind::Bool: return ote::Json::Type::Bool;
    case SchemaNode::Kind::List: return ote::Json::Type::Array;
    case SchemaNode::Kind::Object: case SchemaNode::Kind::Map: return ote::Json::Type::Object;
    case SchemaNode::Kind::Json: return ote::Json::Type::Null;
    default: return ote::Json::Type::String;
  }
}

}  // namespace

std::vector<ColumnSpec> planColumns(const SchemaNode& rows, IdSource* ids) {
  std::vector<ColumnSpec> out;
  for (const auto& f : rows.fields) {
    const SchemaNode& n = *f.second;
    if (n.isScalar()) {
      out.push_back({{f.first}, hintFor(n), &n});
    } else if (n.expand && n.kind == SchemaNode::Kind::Object) {
      for (const auto& sub : n.fields)
        if (sub.second->isScalar()) out.push_back({{f.first, sub.first}, hintFor(*sub.second), sub.second.get()});
    } else if (n.expand && n.kind == SchemaNode::Kind::Map && n.of && ids) {
      for (const QString& id : ids->ids(n.refs)) out.push_back({{f.first, id.toStdString()}, hintFor(*n.of), n.of.get()});
    }
  }
  return out;
}

}  // namespace ote_native
