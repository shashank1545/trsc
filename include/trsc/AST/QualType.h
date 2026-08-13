#ifndef TRSC_AST_QUALTYPE_H
#define TRSC_AST_QUALTYPE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace trsc {

class BuiltinType;
class ASTContext;

enum class TypeKind {
  // Unsigned integers (contiguous: U8..USize).
  U8,
  U16,
  U32,
  U64,
  U128,
  USize,

  // Signed integers (contiguous: I8..ISize).
  I8,
  I16,
  I32,
  I64,
  I128,
  ISize,

  // Floats.
  F32,
  F64,

  Bool,
  Char,
  String,

  // Unit Type (equivalent to void in Rust; makes every function's return
  // complete).
  Unit,

  Pointer,
  Reference,
  Function,
  Array,
};

class QualType {
private:
  const BuiltinType *TypePtr;

public:
  QualType() : TypePtr(nullptr) {}
  QualType(const BuiltinType *Ty) : TypePtr(Ty) {}

  bool isNull() const { return TypePtr == nullptr; }
  const BuiltinType *getTypePtr() const { return TypePtr; }

  // Type queries. All are kind comparisons; no virtual dispatch involved.
  bool isIntegerType() const;
  bool isFloatingType() const;
  bool isSignedIntegerType() const;
  bool isUnsignedTypeIntegerType() const;
  bool isBooleanType() const;
  bool isStringType() const;
  bool isNumericType() const;
  bool isPointerType() const;
  bool isReferenceType() const;
  bool isUnitType() const;
  bool isArrayType() const;
  bool isFunctionType() const;

  QualType getReturnType() const;
  const std::vector<QualType> &getParamsType() const;

  const std::string &getAsString() const;
  std::size_t getSizeInBytes() const;
  std::size_t getAlignment() const;
  TypeKind getKind() const;

  QualType getBaseType() const;

  bool operator==(const QualType &Other) const {
    return TypePtr == Other.TypePtr;
  }

  bool operator!=(const QualType &Other) const {
    return !(TypePtr == Other.TypePtr);
  }
};

/// Concrete base for every type. The kind, layout, and cached spelling live
/// here; composite subclasses only add their component types. There is no
/// virtual dispatch: every query is a comparison against Kind, and getName()
/// returns the spelling built once at construction time.
class BuiltinType {
protected:
  TypeKind Kind;
  std::size_t SizeInBytes;
  std::size_t Alignment;
  // Built exactly once; composite types would otherwise reconstruct (and
  // re-allocate) their spelling on every query.
  std::string Name;

public:
  BuiltinType(TypeKind K, std::size_t Size, std::size_t Align, std::string Name)
      : Kind(K), SizeInBytes(Size), Alignment(Align), Name(std::move(Name)) {}

  TypeKind getKind() const { return Kind; }
  std::size_t getSize() const { return SizeInBytes; }
  std::size_t getAlignment() const { return Alignment; }
  const std::string &getName() const { return Name; }

  bool isInteger() const {
    return Kind >= TypeKind::U8 && Kind <= TypeKind::ISize;
  }
  bool isSigned() const {
    return Kind >= TypeKind::I8 && Kind <= TypeKind::ISize;
  }
  bool isFloating() const {
    return Kind == TypeKind::F32 || Kind == TypeKind::F64;
  }
  bool isBoolean() const { return Kind == TypeKind::Bool; }
  bool isCharacter() const { return Kind == TypeKind::Char; }
  bool isString() const { return Kind == TypeKind::String; }
  bool isPointer() const { return Kind == TypeKind::Pointer; }
  bool isReference() const { return Kind == TypeKind::Reference; }
  bool isUnit() const { return Kind == TypeKind::Unit; }
  bool isFunction() const { return Kind == TypeKind::Function; }
  bool isArray() const { return Kind == TypeKind::Array; }
};

// The builtin leaf types. They add no state and no virtuals - only the
// canonical kind/size/spelling and, for the fixed-width integers, their
// representable range.
class U8BuiltinType : public BuiltinType {
public:
  U8BuiltinType() : BuiltinType(TypeKind::U8, 1, 1, "u8") {}
  static constexpr std::uint8_t min() { return 0; }
  static constexpr std::uint8_t max() { return UINT8_MAX; }
};

class U16BuiltinType : public BuiltinType {
public:
  U16BuiltinType() : BuiltinType(TypeKind::U16, 2, 2, "u16") {}
  static constexpr std::uint16_t min() { return 0; }
  static constexpr std::uint16_t max() { return UINT16_MAX; }
};

class U32BuiltinType : public BuiltinType {
public:
  U32BuiltinType() : BuiltinType(TypeKind::U32, 4, 4, "u32") {}
  static constexpr std::uint32_t min() { return 0; }
  static constexpr std::uint32_t max() { return UINT32_MAX; }
};

class U64BuiltinType : public BuiltinType {
public:
  U64BuiltinType() : BuiltinType(TypeKind::U64, 8, 8, "u64") {}
  static constexpr std::uint64_t min() { return 0; }
  static constexpr std::uint64_t max() { return UINT64_MAX; }
};

class U128BuiltinType : public BuiltinType {
public:
  U128BuiltinType() : BuiltinType(TypeKind::U128, 16, 16, "u128") {}
};

class USizeBuiltinType : public BuiltinType {
public:
  USizeBuiltinType()
      : BuiltinType(TypeKind::USize, sizeof(size_t), sizeof(size_t), "usize") {}
};

class I8BuiltinType : public BuiltinType {
public:
  I8BuiltinType() : BuiltinType(TypeKind::I8, 1, 1, "i8") {}
  static constexpr std::int8_t min() { return INT8_MIN; }
  static constexpr std::int8_t max() { return INT8_MAX; }
};

class I16BuiltinType : public BuiltinType {
public:
  I16BuiltinType() : BuiltinType(TypeKind::I16, 2, 2, "i16") {}
  static constexpr std::int16_t min() { return INT16_MIN; }
  static constexpr std::int16_t max() { return INT16_MAX; }
};

class I32BuiltinType : public BuiltinType {
public:
  I32BuiltinType() : BuiltinType(TypeKind::I32, 4, 4, "i32") {}
  static constexpr std::int32_t min() { return INT32_MIN; }
  static constexpr std::int32_t max() { return INT32_MAX; }
};

class I64BuiltinType : public BuiltinType {
public:
  I64BuiltinType() : BuiltinType(TypeKind::I64, 8, 8, "i64") {}
  static constexpr std::int64_t min() { return INT64_MIN; }
  static constexpr std::int64_t max() { return INT64_MAX; }
};

class I128BuiltinType : public BuiltinType {
public:
  I128BuiltinType() : BuiltinType(TypeKind::I128, 16, 16, "i128") {}
};

class ISizeBuiltinType : public BuiltinType {
public:
  ISizeBuiltinType()
      : BuiltinType(TypeKind::ISize, sizeof(std::ptrdiff_t),
                    sizeof(std::ptrdiff_t), "isize") {}
};

class F32BuiltinType : public BuiltinType {
public:
  F32BuiltinType() : BuiltinType(TypeKind::F32, 4, 4, "f32") {}
};

class F64BuiltinType : public BuiltinType {
public:
  F64BuiltinType() : BuiltinType(TypeKind::F64, 8, 8, "f64") {}
};

class BoolBuiltinType : public BuiltinType {
public:
  BoolBuiltinType() : BuiltinType(TypeKind::Bool, 1, 1, "bool") {}
};

class CharBuiltinType : public BuiltinType {
public:
  CharBuiltinType() : BuiltinType(TypeKind::Char, 4, 4, "char") {}
};

class StringBuiltinType : public BuiltinType {
public:
  StringBuiltinType()
      : BuiltinType(TypeKind::String, sizeof(void *) + 2 * sizeof(size_t),
                    alignof(void *), "string") {}
};

class UnitType : public BuiltinType {
public:
  UnitType() : BuiltinType(TypeKind::Unit, 0, 0, "()") {}
};

// Pointer Type
class PointerType : public BuiltinType {
  QualType PointeeType;
  bool IsMutable;

public:
  PointerType(QualType PointeeType, bool IsMutable)
      : BuiltinType(TypeKind::Pointer, sizeof(void *), alignof(void *),
                    (IsMutable ? "*mut " : "*const ") +
                        PointeeType.getAsString()),
        PointeeType(PointeeType), IsMutable(IsMutable) {}
  bool isMutable() const { return IsMutable; }
  QualType getPointee() const { return PointeeType; }
};

// Reference Type
class ReferenceType : public BuiltinType {
  QualType ReferentType;
  bool IsMutable;

public:
  ReferenceType(QualType ReferentType, bool IsMutable)
      : BuiltinType(TypeKind::Reference, sizeof(void *), alignof(void *),
                    (IsMutable ? "&mut" : "&") + ReferentType.getAsString()),
        ReferentType(ReferentType), IsMutable(IsMutable) {}
  bool isMutable() const { return IsMutable; }
  QualType getReferent() const { return ReferentType; }
};

// Function Type or Function Signature
class FunctionType : public BuiltinType {
  QualType ReturnType;
  std::vector<QualType> ParamTypes;

  static std::string buildName(QualType ReturnType,
                               const std::vector<QualType> &ParamTypes) {
    std::string Name = "(";
    for (const auto &Param : ParamTypes) {
      Name += Param.getAsString();
      Name += ',';
    }
    Name += ") -> ";
    Name += ReturnType.getAsString();
    return Name;
  }

public:
  FunctionType(QualType ReturnType, std::vector<QualType> ParamTypes)
      : BuiltinType(TypeKind::Function, 0, 0,
                    buildName(ReturnType, ParamTypes)),
        ReturnType(ReturnType), ParamTypes(std::move(ParamTypes)) {}
  QualType getReturn() const { return ReturnType; }
  const std::vector<QualType> &getParams() const { return ParamTypes; }
};

// Array Type
class ArrayType : public BuiltinType {
  QualType ElementType;
  std::size_t Size;

public:
  ArrayType(QualType ET, std::size_t Size)
      : BuiltinType(TypeKind::Array, ET.getSizeInBytes() * Size,
                    ET.getAlignment(),
                    "[" + ET.getAsString() + "; " + std::to_string(Size) + "]"),
        ElementType(ET), Size(Size) {}
  QualType getElementType() const { return ElementType; }
  std::size_t getArraySize() const { return Size; }
};

inline TypeKind QualType::getKind() const { return TypePtr->getKind(); }

inline bool QualType::isIntegerType() const {
  return TypePtr && TypePtr->isInteger();
}
inline bool QualType::isFloatingType() const {
  return TypePtr && TypePtr->isFloating();
}
inline bool QualType::isSignedIntegerType() const {
  return TypePtr && TypePtr->isSigned();
}
inline bool QualType::isUnsignedTypeIntegerType() const {
  return TypePtr && TypePtr->isInteger() && !TypePtr->isSigned();
}
inline bool QualType::isBooleanType() const {
  return TypePtr && TypePtr->isBoolean();
}
inline bool QualType::isStringType() const {
  return TypePtr && TypePtr->isString();
}
inline bool QualType::isNumericType() const {
  return TypePtr && (TypePtr->isInteger() || TypePtr->isFloating());
}
inline bool QualType::isPointerType() const {
  return TypePtr && TypePtr->isPointer();
}
inline bool QualType::isReferenceType() const {
  return TypePtr && TypePtr->isReference();
}
inline bool QualType::isUnitType() const {
  return TypePtr && TypePtr->isUnit();
}
inline bool QualType::isArrayType() const {
  return TypePtr && TypePtr->isArray();
}
inline bool QualType::isFunctionType() const {
  return TypePtr && TypePtr->isFunction();
}

inline std::size_t QualType::getSizeInBytes() const {
  return TypePtr ? TypePtr->getSize() : 0;
}
inline std::size_t QualType::getAlignment() const {
  return TypePtr ? TypePtr->getAlignment() : 0;
}

struct QualTypeHasher {
  std::size_t operator()(const QualType &Qt) const {
    return std::hash<const BuiltinType *>{}(Qt.getTypePtr());
  }
};

struct FunctionTypeKey {
  QualType ReturnType;
  std::vector<QualType> ParamTypes;

  bool operator==(const FunctionTypeKey &other) const {
    if (ReturnType != other.ReturnType ||
        ParamTypes.size() != other.ParamTypes.size()) {
      return false;
    }
    for (size_t i = 0; i < ParamTypes.size(); ++i) {
      if (ParamTypes[i] != other.ParamTypes[i]) {
        return false;
      }
    }
    return true;
  }
};

struct FunctionTypeKeyHasher {
  std::size_t operator()(const FunctionTypeKey &Key) const {
    std::size_t H = QualTypeHasher{}(Key.ReturnType);
    for (const auto &ParamType : Key.ParamTypes) {
      H ^= QualTypeHasher{}(ParamType) + 0x9e3779b9 + (H << 6) + (H >> 2);
    }
    return H;
  }
};

struct PointerTypeKey {
  QualType PointeeType;
  bool IsMutable;

  bool operator==(const PointerTypeKey &other) const {
    return PointeeType == other.PointeeType && IsMutable == other.IsMutable;
  }
};

struct PointerTypeKeyHasher {
  std::size_t operator()(const PointerTypeKey &Key) const {
    std::size_t H = QualTypeHasher{}(Key.PointeeType);
    H ^= std::hash<bool>{}(Key.IsMutable) + 0x9e3779b9 + (H << 6) + (H >> 2);
    return H;
  }
};

struct ReferenceTypeKey {
  QualType ReferentType;
  bool IsMutable;

  bool operator==(const ReferenceTypeKey &other) const {
    return ReferentType == other.ReferentType && IsMutable == other.IsMutable;
  }
};

struct ReferenceTypeKeyHasher {
  std::size_t operator()(const ReferenceTypeKey &Key) const {
    std::size_t H = QualTypeHasher{}(Key.ReferentType);
    H ^= std::hash<bool>{}(Key.IsMutable) + 0x9e3779b9 + (H << 6) + (H >> 2);
    return H;
  }
};

struct ArrayTypeKey {
  QualType ElementType;
  size_t Size;

  bool operator==(const ArrayTypeKey &other) const {
    return ElementType == other.ElementType && Size == other.Size;
  }
};

struct ArrayTypeKeyHasher {
  std::size_t operator()(const ArrayTypeKey &Key) const {
    std::size_t H = QualTypeHasher{}(Key.ElementType);
    H ^= std::hash<size_t>{}(Key.Size) + 0x9e3779b9 + (H << 6) + (H >> 2);
    return H;
  }
};

} // namespace trsc

#endif // TRSC_AST_QUALTYPE_H
