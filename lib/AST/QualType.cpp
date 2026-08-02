#include "trsc/AST/QualType.h"

namespace trsc {

const std::string &QualType::getAsString() const {
  static const std::string Unresolved = "Unresolved Type";
  return TypePtr ? TypePtr->getName() : Unresolved;
}

QualType QualType::getReturnType() const {
  if (TypePtr && TypePtr->isFunction()) {
    return static_cast<const FunctionType *>(TypePtr)->getReturn();
  }
  return QualType();
}

const std::vector<QualType> &QualType::getParamsType() const {
  static const std::vector<QualType> EmptyParams;
  if (TypePtr && TypePtr->isFunction()) {
    return static_cast<const FunctionType *>(TypePtr)->getParams();
  }
  return EmptyParams;
}

QualType QualType::getBaseType() const {
  if (!TypePtr) {
    return QualType();
  }
  switch (TypePtr->getKind()) {
  case TypeKind::Pointer:
    return static_cast<const PointerType *>(TypePtr)->getPointee();
  case TypeKind::Reference:
    return static_cast<const ReferenceType *>(TypePtr)->getReferent();
  case TypeKind::Array:
    return static_cast<const ArrayType *>(TypePtr)->getElementType();
  default:
    return QualType();
  }
}

} // namespace trsc
