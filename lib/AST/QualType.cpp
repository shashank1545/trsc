#include "trsc/AST/QualType.h"
#include <vector>

namespace trsc {

  bool QualType::isIntegerType() const {
    return TypePtr ? TypePtr->isInteger() : false;
  }

  bool QualType::isFloatingType() const {
    return TypePtr ? TypePtr->isFloating() : false;
  }
  
  bool QualType::isSignedIntegerType() const {
    return TypePtr ? TypePtr->isSigned() : false;
  }

  bool QualType::isUnsignedTypeIntegerType() const {
    return isIntegerType() && !isSignedIntegerType();
  }

  bool QualType::isBooleanType() const {
    return TypePtr ? TypePtr->isBoolean() : false;
  }

  bool QualType::isNumericType() const {
    return isIntegerType() || isFloatingType();
  }

  bool QualType::isPointerType() const {
    return TypePtr ? TypePtr->isPointer() : false;
  }

  bool QualType::isReferenceType() const {
    return TypePtr ? TypePtr->isReference() : false;
  }

  bool QualType::isUnitType() const {
    return TypePtr ? TypePtr->isUnit() : false;
  }

  bool QualType::isFunctionType() const {
    return TypePtr ? TypePtr->isFunction() : false;
  }

  std::string QualType::getAsString() const {
    return TypePtr ? TypePtr->getName() : "Unresolved Type";
  }

  std::size_t QualType::getSizeInBytes() const {
    return TypePtr ? TypePtr->getSize() : 0;
  }

  std::size_t QualType::getAlignment() const {
    return TypePtr ? TypePtr->getAlignment() : 0;
  }

  TypeKind QualType::getKind() const {
    return TypePtr->getKind();
  }

  QualType QualType::getReturnType() const {
    return TypePtr->getReturn();
  }

  const std::vector<QualType>& QualType::getParamsType() const {
    return TypePtr->getParams();
  }
} // namespace trsc
