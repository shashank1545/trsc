
#include "trsc/AST/ASTContext.h"

namespace trsc {

ASTContext::ASTContext() {
  // Each builtin knows its own kind, layout, and spelling; register them all
  // by name for getTypeByName.
  for (const BuiltinType *Ty : std::initializer_list<const BuiltinType *>{
           &U8Ty, &U16Ty, &U32Ty, &U64Ty, &U128Ty, &USizeTy, &I8Ty, &I16Ty,
           &I32Ty, &I64Ty, &I128Ty, &ISizeTy, &F32Ty, &F64Ty, &BoolTy, &CharTy,
           &StringTy, &UnitTy}) {
    TypeMap[Ty->getName()] = QualType(Ty);
  }
}

QualType ASTContext::getTypeByName(const std::string &Name) const {
  auto it = TypeMap.find(Name);
  if (it != TypeMap.end()) {
    return it->second;
  }
  return QualType();
}

QualType ASTContext::getPointerType(QualType PointeeType, bool IsMutable) {
  auto [It, Inserted] = PtrTy.try_emplace({PointeeType, IsMutable});
  if (Inserted) {
    It->second = std::make_unique<PointerType>(PointeeType, IsMutable);
  }
  return QualType(It->second.get());
}

QualType ASTContext::getReferenceType(QualType ReferentType, bool IsMutable) {
  auto [It, Inserted] = RefTy.try_emplace({ReferentType, IsMutable});
  if (Inserted) {
    It->second = std::make_unique<ReferenceType>(ReferentType, IsMutable);
  }
  return QualType(It->second.get());
}

QualType ASTContext::getFunctionType(QualType ReturnType,
                                     const std::vector<QualType> &ParamsType) {
  auto [It, Inserted] = FuncTy.try_emplace({ReturnType, ParamsType});
  if (Inserted) {
    It->second = std::make_unique<FunctionType>(ReturnType, ParamsType);
  }
  return QualType(It->second.get());
}

QualType ASTContext::getArrayType(QualType ElementType, size_t Size) {
  auto [It, Inserted] = ArrayTy.try_emplace({ElementType, Size});
  if (Inserted) {
    It->second = std::make_unique<ArrayType>(ElementType, Size);
  }
  return QualType(It->second.get());
}

bool ASTContext::areTypesCompatible(QualType T1, QualType T2) const {
  if (T1.isNull() || T2.isNull()) {
    return false;
  }
  return T1.getTypePtr() == T2.getTypePtr();
}

bool ASTContext::canConvert(QualType From, QualType To, bool Explicit) const {
  if (areTypesCompatible(From, To)) {
    return true;
  }
  if (From.isIntegerType() && To.isIntegerType()) {
    return true;
  }
  if (From.isFloatingType() && To.isFloatingType()) {
    return true;
  }
  if (From.isIntegerType() && To.isFloatingType()) {
    return true;
  }
  if (Explicit && From.isFloatingType() && To.isIntegerType()) {
    return true;
  }
  if (From.isBooleanType() && To.isBooleanType()) {
    return true;
  }
  if (!Explicit && From.isArrayType() && To.isArrayType()) {
    if (canConvert(From.getBaseType(), To.getBaseType(), Explicit)) {
      return true;
    }
  }
  return false;
}

} // namespace trsc
