
#include "trsc/AST/ASTContext.h"
#include "trsc/AST/QualType.h"
#include <memory>

namespace trsc {

ASTContext::ASTContext() { initializeBuiltinTypes(); }

void ASTContext::initializeBuiltinTypes() {
  U8Ty = std::make_unique<U8BuiltinType>();
  TypeMap[U8Ty->getName()] = QualType(U8Ty.get());

  U16Ty = std::make_unique<U16BuiltinType>();
  TypeMap[U16Ty->getName()] = QualType(U16Ty.get());

  U32Ty = std::make_unique<U32BuiltinType>();
  TypeMap[U32Ty->getName()] = QualType(U32Ty.get());

  U64Ty = std::make_unique<U64BuiltinType>();
  TypeMap[U64Ty->getName()] = QualType(U64Ty.get());

  U128Ty = std::make_unique<U128BuiltinType>();
  TypeMap[U128Ty->getName()] = QualType(U128Ty.get());

  USizeTy = std::make_unique<USizeBuiltinType>();
  TypeMap[USizeTy->getName()] = QualType(USizeTy.get());

  I8Ty = std::make_unique<I8BuiltinType>();
  TypeMap[I8Ty->getName()] = QualType(I8Ty.get());

  I16Ty = std::make_unique<I16BuiltinType>();
  TypeMap[I16Ty->getName()] = QualType(I16Ty.get());

  I32Ty = std::make_unique<I32BuiltinType>();
  TypeMap[I32Ty->getName()] = QualType(I32Ty.get());

  I64Ty = std::make_unique<I64BuiltinType>();
  TypeMap[I64Ty->getName()] = QualType(I64Ty.get());

  I128Ty = std::make_unique<I128BuiltinType>();
  TypeMap[I128Ty->getName()] = QualType(I128Ty.get());

  ISizeTy = std::make_unique<ISizeBuiltinType>();
  TypeMap[ISizeTy->getName()] = QualType(ISizeTy.get());

  F32Ty = std::make_unique<F32BuiltinType>();
  TypeMap[F32Ty->getName()] = QualType(F32Ty.get());

  F64Ty = std::make_unique<F64BuiltinType>();
  TypeMap[F64Ty->getName()] = QualType(F64Ty.get());

  BoolTy = std::make_unique<BoolBuiltinType>();
  TypeMap[BoolTy->getName()] = QualType(BoolTy.get());

  CharTy = std::make_unique<CharBuiltinType>();
  TypeMap[CharTy->getName()] = QualType(CharTy.get());

  StringTy = std::make_unique<StringBuiltinType>();
  TypeMap[StringTy->getName()] = QualType(StringTy.get());

  UnitTy = std::make_unique<UnitType>();
  TypeMap[UnitTy->getName()] = QualType(UnitTy.get());
}

QualType ASTContext::getTypeByName(const std::string &Name) const {
  auto it = TypeMap.find(Name);
  if (it != TypeMap.end()) {
    return it->second;
  }
  return QualType();
}

QualType ASTContext::getPointerType(QualType PointeeType, bool IsMutable) {
  PointerTypeKey Key = {PointeeType, IsMutable};
  auto It = PtrTy.find(Key);
  if (It != PtrTy.end()) {
    return QualType(It->second.get());
  }
  PtrTy[Key] = std::make_unique<PointerType>(PointeeType, IsMutable);
  return QualType(PtrTy[Key].get());
}

QualType ASTContext::getReferenceType(QualType ReferentType, bool IsMutable) {
  ReferenceTypeKey Key = {ReferentType, IsMutable};
  auto It = RefTy.find(Key);
  if (It != RefTy.end()) {
    return QualType(It->second.get());
  }
  RefTy[Key] = std::make_unique<ReferenceType>(ReferentType, IsMutable);
  return QualType(RefTy[Key].get());
}

QualType ASTContext::getFunctionType(QualType ReturnType, 
    const std::vector<QualType>& ParamsType) {
  FunctionTypeKey Key = {ReturnType, ParamsType};
  auto It = FuncTy.find(Key);
  if (It != FuncTy.end()) {
    return QualType(It->second.get());
  }
  FuncTy[Key] = std::make_unique<FunctionType>(ReturnType, ParamsType);
  return QualType(FuncTy[Key].get());
}

QualType ASTContext::getArrayType(QualType ElementType, bool IsMutable, 
    size_t Size) {
  ArrayTypeKey Key = {ElementType, IsMutable, Size};
  auto It = ArrayTy.find(Key);
  if(It != ArrayTy.end()) {
    return QualType(It->second.get());
  }
  ArrayTy[Key] = std::make_unique<ArrayType>(ElementType, IsMutable, Size);
  return QualType(ArrayTy[Key].get());
}

bool ASTContext::areTypesCompatible(QualType T1, QualType T2) const {
    if (T1.isNull() || T2.isNull()) {
        return false;
    }
    return T1.getTypePtr() == T2.getTypePtr();
}

bool ASTContext::canImplicitlyConvert(QualType From, QualType To) const {
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

    if (From.isBooleanType() && To.isBooleanType()) {
        return true;
    }

    return false;
}

} // namespace trsc

