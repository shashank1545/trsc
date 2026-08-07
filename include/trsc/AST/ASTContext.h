#ifndef TRSC_AST_ASTCONTEXT_H
#define TRSC_AST_ASTCONTEXT_H

#include "trsc/AST/ASTAllocator.h"
#include "trsc/AST/QualType.h"
#include "trsc/Basic/ArrayRef.h"
#include <cstring>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace trsc {

class ASTContext {
private:
  mutable ASTAllocator Allocator;

  U8BuiltinType U8Ty;
  U16BuiltinType U16Ty;
  U32BuiltinType U32Ty;
  U64BuiltinType U64Ty;
  U128BuiltinType U128Ty;
  USizeBuiltinType USizeTy;

  I8BuiltinType I8Ty;
  I16BuiltinType I16Ty;
  I32BuiltinType I32Ty;
  I64BuiltinType I64Ty;
  I128BuiltinType I128Ty;
  ISizeBuiltinType ISizeTy;

  F32BuiltinType F32Ty;
  F64BuiltinType F64Ty;

  BoolBuiltinType BoolTy;
  CharBuiltinType CharTy;
  StringBuiltinType StringTy;
  UnitType UnitTy;

  std::unordered_map<std::string, QualType> TypeMap;

  std::unordered_map<PointerTypeKey, std::unique_ptr<PointerType>,
                     PointerTypeKeyHasher>
      PtrTy;
  std::unordered_map<ReferenceTypeKey, std::unique_ptr<ReferenceType>,
                     ReferenceTypeKeyHasher>
      RefTy;
  std::unordered_map<ArrayTypeKey, std::unique_ptr<ArrayType>,
                     ArrayTypeKeyHasher>
      ArrayTy;
  std::unordered_map<FunctionTypeKey, std::unique_ptr<FunctionType>,
                     FunctionTypeKeyHasher>
      FuncTy;

  bool canConvert(QualType From, QualType To, bool Explicit) const;

public:
  ASTContext();

  ASTContext(const ASTContext &) = delete;
  ASTContext &operator=(const ASTContext &) = delete;

  void *Allocate(size_t Bytes, size_t Align = 8) const {
    return Allocator.Allocate(Bytes, Align);
  }

  template <typename T> ArrayRef<T> allocateArray(const std::vector<T> &Src) {
    if (Src.empty()) {
      return ArrayRef<T>();
    }
    T *Mem = static_cast<T *>(Allocate(sizeof(T) * Src.size(), alignof(T)));
    for (size_t I = 0; I < Src.size(); ++I) {
      new (static_cast<void *>(Mem + I)) T(Src[I]);
    }
    return ArrayRef<T>(Mem, Src.size());
  }

  /// Copies \p Src into the arena and returns a view of the copy. Used for
  /// strings that are synthesised rather than carved out of the source buffer
  /// (e.g. escape-decoded string literals).
  std::string_view allocateString(std::string_view Src) {
    if (Src.empty()) {
      return {};
    }
    char *Mem = static_cast<char *>(Allocate(Src.size(), alignof(char)));
    std::memcpy(Mem, Src.data(), Src.size());
    return std::string_view(Mem, Src.size());
  }

  size_t getASTMemory() const { return Allocator.getTotalMemory(); }

  QualType getU8Type() const { return QualType(&U8Ty); }
  QualType getU16Type() const { return QualType(&U16Ty); }
  QualType getU32Type() const { return QualType(&U32Ty); }
  QualType getU64Type() const { return QualType(&U64Ty); }
  QualType getU128Type() const { return QualType(&U128Ty); }
  QualType getUSizeType() const { return QualType(&USizeTy); }

  QualType getI8Type() const { return QualType(&I8Ty); }
  QualType getI16Type() const { return QualType(&I16Ty); }
  QualType getI32Type() const { return QualType(&I32Ty); }
  QualType getI64Type() const { return QualType(&I64Ty); }
  QualType getI128Type() const { return QualType(&I128Ty); }
  QualType getISizeType() const { return QualType(&ISizeTy); }

  QualType getF32Type() const { return QualType(&F32Ty); }
  QualType getF64Type() const { return QualType(&F64Ty); }

  QualType getBoolType() const { return QualType(&BoolTy); }

  QualType getCharType() const { return QualType(&CharTy); }

  QualType getStringType() const { return QualType(&StringTy); }

  QualType getUnitType() const { return QualType(&UnitTy); }

  QualType getPointerType(QualType PointeeType, bool IsMutable);
  QualType getReferenceType(QualType ReferentType, bool IsMutable);
  QualType getArrayType(QualType ElementType, size_t Size);
  QualType getFunctionType(QualType ReturnType,
                           const std::vector<QualType> &ParamsType);

  QualType getNullType() const { return QualType(); }

  QualType getTypeByName(const std::string &Name) const;

  bool areTypesCompatible(QualType T1, QualType T2) const;

  bool canImplicitlyConvert(QualType From, QualType To) const {
    return canConvert(From, To, /*Explicit=*/false);
  }
  bool canExplicitlyConvert(QualType From, QualType To) const {
    return canConvert(From, To, /*Explicit=*/true);
  }
};

} // namespace trsc

#endif // TRSC_AST_ASTCONTEXT_H
