#ifndef TRSC_AST_ASTCONTEXT_H
#define TRSC_AST_ASTCONTEXT_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "trsc/AST/QualType.h"

namespace trsc {

class ASTContext {
  private:
    std::unique_ptr<U8BuiltinType> U8Ty;
    std::unique_ptr<U16BuiltinType> U16Ty;
    std::unique_ptr<U32BuiltinType> U32Ty;
    std::unique_ptr<U64BuiltinType> U64Ty;
    std::unique_ptr<U128BuiltinType> U128Ty;
    std::unique_ptr<USizeBuiltinType> USizeTy;

    std::unique_ptr<I8BuiltinType> I8Ty;
    std::unique_ptr<I16BuiltinType> I16Ty;
    std::unique_ptr<I32BuiltinType> I32Ty;
    std::unique_ptr<I64BuiltinType> I64Ty;
    std::unique_ptr<I128BuiltinType> I128Ty;
    std::unique_ptr<ISizeBuiltinType> ISizeTy;

    std::unique_ptr<F32BuiltinType> F32Ty;
    std::unique_ptr<F64BuiltinType> F64Ty; 

    std::unique_ptr<BoolBuiltinType> BoolTy;

    std::unique_ptr<CharBuiltinType> CharTy;

    std::unique_ptr<StringBuiltinType> StringTy; 

    std::unique_ptr<UnitType> UnitTy;
    
    std::unordered_map<std::string, QualType> TypeMap;

    std::unordered_map<PointerTypeKey, std::unique_ptr<PointerType>, 
      PointerTypeKeyHasher> PtrTy;
    std::unordered_map<ReferenceTypeKey, std::unique_ptr<ReferenceType>, 
      ReferenceTypeKeyHasher> RefTy;
    std::unordered_map<ArrayTypeKey, std::unique_ptr<ArrayType>,
      ArrayTypeKeyHasher> ArrayTy;
    std::unordered_map<FunctionTypeKey, std::unique_ptr<FunctionType>, 
      FunctionTypeKeyHasher> FuncTy;

    void initializeBuiltinTypes();

  public:
    ASTContext();

    QualType getU8Type() const { return QualType(U8Ty.get()); }
    QualType getU16Type() const { return QualType(U16Ty.get()); }
    QualType getU32Type() const { return QualType(U32Ty.get()); }
    QualType getU64Type() const { return QualType(U64Ty.get()); }
    QualType getU128Type() const { return QualType(U128Ty.get()); }
    QualType getUSizeType() const { return QualType(USizeTy.get()); }

    QualType getI8Type() const { return QualType(I8Ty.get()); }
    QualType getI16Type() const { return QualType(I16Ty.get()); }
    QualType getI32Type() const { return QualType(I32Ty.get()); }
    QualType getI64Type() const { return QualType(I64Ty.get()); }
    QualType getI128Type() const { return QualType(I128Ty.get()); }
    QualType getISizeType() const { return QualType(ISizeTy.get()); }

    QualType getF32Type() const { return QualType(F32Ty.get()); }
    QualType getF64Type() const { return QualType(F64Ty.get()); }

    QualType getBoolType() const { return QualType(BoolTy.get()); }

    QualType getCharType() const { return QualType(CharTy.get());}

    QualType getStringType() const { return QualType(StringTy.get());}

    QualType getUnitType() const { return QualType(UnitTy.get());}

    QualType getPointerType(QualType PointeeType, bool IsMutable); 
    QualType getReferenceType(QualType ReferentType, bool IsMutable);
    QualType getArrayType(QualType ElementType, bool IsMutable, size_t Size);
    QualType getFunctionType(QualType ReturnType, 
        const std::vector<QualType>& ParamsType); 

    // Returns a nullptr where no type fits
    QualType getNullType() const { return QualType(); }

    QualType getTypeByName(const std::string& Name) const;

    bool areTypesCompatible(QualType T1, QualType T2) const;

    bool canImplicitlyConvert(QualType From, QualType To) const;
};

} // namespace trsc

#endif // TRSC_AST_ASTCONTEXT_H
