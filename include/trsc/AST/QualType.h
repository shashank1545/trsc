#ifndef TRSC_AST_QUALTYPE_H
#define TRSC_AST_QUALTYPE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace trsc {

  class BuiltinType;
  class ASTContext;

  enum class TypeKind {
    // Unsigned Types
    U8, U16, U32, U64, U128, USize,

    // Signed Types
    I8, I16, I32, I64, I128, ISize,

    // Float Types
    F32, F64, 

    // Boolean 
    Bool, 

    // Character
    Char,

    // String
    String, 

    // Unit Type (This is the equivalent to void in rust, it makes the return
    // complete and ensures that every function will have a return.)
    Unit,

    // Rust distinguishes between pointer and Deference in its naming
    // so it will be easy to seperate them during Parsing and Sema.
    // Pointer
    Pointer,

    // Reference 
    Reference,

    // Function Signature
    Function,

    // Array
    Array,

    // Complex Types
    // Array, tuple, Struct, Enum,
  };

  class QualType {
    private:
      const BuiltinType* TypePtr;

    public:
      QualType() : TypePtr(nullptr) {}
      QualType(const BuiltinType* Ty) : TypePtr(Ty) {}

      bool isNull() const { return TypePtr == nullptr; }
      const BuiltinType* getTypePtr() const { return TypePtr; }

      // Type queries
      bool isIntegerType() const;
      bool isFloatingType() const;
      bool isSignedIntegerType() const;
      bool isUnsignedTypeIntegerType() const;
      bool isBooleanType() const;
      bool isNumericType() const;
      bool isPointerType() const;
      bool isReferenceType() const;
      bool isUnitType() const;
      bool isArrayType() const;
      bool isFunctionType() const;

      QualType getReturnType() const;
      const std::vector<QualType>& getParamsType() const; 
      const std::vector<int>& getShapeVec() const;

      std::string getAsString() const;
      size_t getSizeInBytes() const;
      size_t getAlignment() const;
      TypeKind getKind() const;

      bool operator==(const QualType& Other) const {
        return TypePtr == Other.TypePtr;
      }

      bool operator!=(const QualType& Other) const {
        return !(TypePtr == Other.TypePtr);
      }
  };

  class BuiltinType {
    protected:
      TypeKind Kind;
      size_t SizeInBytes;
      size_t Alignment;

      BuiltinType(TypeKind K, size_t Size, size_t Align): 
        Kind(K), SizeInBytes(Size), Alignment(Align) {}

    public:
      virtual ~BuiltinType() = default;

      TypeKind getKind() const { return Kind; }
      size_t getSize() const { return SizeInBytes; }
      size_t getAlignment() const { return Alignment; }

      virtual std::string getName() const = 0;
      virtual bool isInteger() const {return false;}
      virtual bool isFloating() const {return false;}
      virtual bool isSigned() const {return false;}
      virtual bool isBoolean() const {return false;}
      virtual bool isCharacter() const {return false;}
      virtual bool isString() const {return false;}
      virtual bool isPointer() const {return false;}
      virtual bool isReference() const {return false;}
      virtual bool isUnit() const {return false;}
      virtual bool isFunction() const {return false;}
      virtual bool isArray() const {return false;}

      virtual const std::vector<int>& getShape() const { 
        static const std::vector<int> EmptyShape;
        return EmptyShape;
      }
      
      virtual QualType getReturn() const { return QualType(); }
      virtual const std::vector<QualType>& getParams() const {
        static const std::vector<QualType> EmptyParams;
        return EmptyParams;
      }
  };

  class U8BuiltinType : public BuiltinType {
    public:
      U8BuiltinType() : BuiltinType(TypeKind::U8, 1, 1) {}
      std::string getName() const override { return "u8"; }
      bool isInteger() const override { return true; }
      static constexpr std::uint8_t min() { return 0; }
      static constexpr std::uint8_t max() { return UINT8_MAX; }
  };


  class U16BuiltinType : public BuiltinType {
    public:
      U16BuiltinType() : BuiltinType(TypeKind::U16, 2, 2) {}
      std::string getName() const override { return "u16"; }
      bool isInteger() const override { return true; }
      static constexpr std::uint16_t min() { return 0; }
      static constexpr std::uint16_t max() { return UINT16_MAX; }
  };

  class U32BuiltinType : public BuiltinType {
    public:
      U32BuiltinType() : BuiltinType(TypeKind::U32, 4, 4) {}
      std::string getName() const override { return "u32"; }
      bool isInteger() const override { return true; }
      static constexpr std::uint32_t min() { return 0; }
      static constexpr std::uint32_t max() { return UINT32_MAX; }
  };

  class U64BuiltinType : public BuiltinType {
    public:
      U64BuiltinType() : BuiltinType(TypeKind::U64, 8, 8) {}
      std::string getName() const override { return "u64"; }
      bool isInteger() const override { return true; }
      static constexpr std::uint64_t min() { return 0; }
      static constexpr std::uint64_t max() { return UINT64_MAX; }
  };

  class U128BuiltinType : public BuiltinType {
    public:
      U128BuiltinType() : BuiltinType(TypeKind::U128, 16, 16) {}
      std::string getName() const override { return "u128"; }
      bool isInteger() const override { return true; }
  };

  class USizeBuiltinType : public BuiltinType {
    public:
      USizeBuiltinType() : BuiltinType(TypeKind::USize, sizeof(size_t), 
          sizeof(size_t)) {}
      std::string getName() const override { return "usize"; }
      bool isInteger() const override { return true; }
  };

  // Signed Type
  class I8BuiltinType : public BuiltinType {
    public:
      I8BuiltinType() : BuiltinType(TypeKind::I8, 1, 1) {}
      std::string getName() const override { return "i8"; }
      bool isInteger() const override { return true; }
      bool isSigned() const override { return true; }
      static constexpr std::int8_t min() { return INT8_MIN; }
      static constexpr std::int8_t max() { return INT8_MAX; }
  };

  class I16BuiltinType : public BuiltinType {
    public:
      I16BuiltinType() : BuiltinType(TypeKind::I16, 2, 2) {}
      std::string getName() const override { return "i16"; }
      bool isInteger() const override { return true; }
      bool isSigned() const override { return true; }
      static constexpr std::int16_t min() { return INT16_MIN; }
      static constexpr std::int16_t max() { return INT16_MAX; }
  };

  class I32BuiltinType : public BuiltinType {
    public:
      I32BuiltinType() : BuiltinType(TypeKind::I32, 4, 4) {}
      std::string getName() const override { return "i32"; }
      bool isInteger() const override { return true; }
      bool isSigned() const override { return true; }
      static constexpr std::int32_t min() { return INT16_MIN; }
      static constexpr std::int32_t max() { return INT32_MAX; }
  };

  class I64BuiltinType : public BuiltinType {
    public:
      I64BuiltinType() : BuiltinType(TypeKind::I64, 8, 8) {}
      std::string getName() const override { return "i64"; }
      bool isInteger() const override { return true; }
      bool isSigned() const override { return true; }
      static constexpr std::int64_t min() { return 0; }
      static constexpr std::int64_t max() { return INT64_MAX; }
  };

  class I128BuiltinType : public BuiltinType {
    public:
      I128BuiltinType() : BuiltinType(TypeKind::I128, 16, 16) {}
      std::string getName() const override { return "i128"; }
      bool isInteger() const override { return true; }
      bool isSigned() const override { return true; }
  };

  class ISizeBuiltinType : public BuiltinType {
    public:
      ISizeBuiltinType() : BuiltinType(TypeKind::ISize, sizeof(std::ptrdiff_t), 
          sizeof(std::ptrdiff_t)) {}
      std::string getName() const override { return "isize"; }
      bool isInteger() const override { return true; }
      bool isSigned() const override { return true; }
  };

  // Floating Type
  class F32BuiltinType : public BuiltinType {
    public: 
      F32BuiltinType() : BuiltinType(TypeKind::F32, 4, 4) {}
      std::string getName() const override { return "f32"; }         
      bool isFloating() const override { return true; }
  };

  class F64BuiltinType : public BuiltinType {
    public: 
      F64BuiltinType() : BuiltinType(TypeKind::F64, 8, 8) {}
      std::string getName() const override { return "f64"; }         
      bool isFloating() const override { return true; }
  };

  // Boolean Type
  class BoolBuiltinType : public BuiltinType {
    public:
      BoolBuiltinType() : BuiltinType(TypeKind::Bool, 1, 1) {}
      std::string getName() const override { return "bool";}
      bool isBoolean() const override { return true; }
  }; 

  // Char Type
  class CharBuiltinType : public BuiltinType {
    public:
      CharBuiltinType() : BuiltinType(TypeKind::Char, 4, 4) {}
      std::string getName() const override { return "char";}
      bool isCharacter() const override { return true;}
  };

  // String Type
  class StringBuiltinType : public BuiltinType {
    public:
      StringBuiltinType() : BuiltinType(TypeKind::String, 
          sizeof(void*) + 2*sizeof(size_t), alignof(void*)) {}
      std::string getName() const override { return "string";}
      bool isString() const override {return true;}
  };

  // Unit Type
  class UnitType : public BuiltinType {
    public:
      UnitType() : BuiltinType(TypeKind::Unit, 0, 0) {}
      std::string getName() const override { return "()";}
      bool isUnit() const override { return true;}
  };

  // Pointer Type
  class PointerType : public BuiltinType {
    QualType PointeeType;
    bool IsMutable;
    public:
    PointerType(QualType PointeeType, bool IsMutable) : BuiltinType(
        TypeKind::Pointer, sizeof(void*), alignof(void*)), 
    PointeeType(PointeeType), IsMutable(IsMutable) {}
    std::string getName() const override { 
      std::string Name = (IsMutable ? "*mut ":"*const ") + 
        PointeeType.getAsString();
      return Name;}
    bool isPointer() const override {return true;}
  };

  // Reference Type
  class ReferenceType: public BuiltinType {
    QualType ReferentType;
    bool IsMutable;
    public:
    ReferenceType(QualType ReferentType, bool IsMutable) : BuiltinType(
        TypeKind::Pointer, sizeof(void*), alignof(void*)), 
    ReferentType(ReferentType), IsMutable(IsMutable) {}
    std::string getName() const override {
      std::string Name = (IsMutable ? "&mut":"&")+ ReferentType.getAsString();
      return Name;}
    bool isReference() const override {return true;}
  };

  // Function Type or Function Signature
  class FunctionType : public BuiltinType {
    QualType ReturnType;
    std::vector<QualType> ParamTypes;
    public:
    FunctionType(QualType ReturnType, std::vector<QualType> ParamTypes):
      BuiltinType(TypeKind::Function, 0, 0), ReturnType(ReturnType),
      ParamTypes(std::move(ParamTypes)) {}
    std::string getName() const override {
      std::string Name = "(";
      for (const auto& Param: ParamTypes) {
        Name= Name + Param.getAsString() +  "," ; 
      }
      Name = Name + ") -> " + ReturnType.getAsString();
      return Name;
    } 
    bool isFunction() const override {return true;}
    QualType getReturn() const override { return ReturnType; }
    const std::vector<QualType>& getParams() const override { return 
      ParamTypes; }
  };

  // Array Type
  class ArrayType : public BuiltinType {
    QualType ElementType;
    std::vector<int> Shape;
    bool IsRefrence;
    bool IsMutable;
    public:
    ArrayType(QualType ET, bool IsRefrence, bool IsMutable): 
      BuiltinType(ET.getKind(), ET.getSizeInBytes(), ET.getAlignment()), 
      ElementType(ET), IsRefrence(IsRefrence), IsMutable(IsMutable) {}
    std::string getName() const override {
      std::string Name = (IsRefrence ? (IsMutable ? "&mut":"&") : 
          (IsMutable ? "mut" : "")); 
      std::string Start(Shape.size(), '[');
      Name= Name + Start + ElementType.getAsString();
      for(int I = 0; I < Shape.size(); ++I) {
        Name = Name + ';' + std::to_string(Shape[I])  + ']';
      }
      return Name;
    }
    bool isArray() const override { return true; }
    const std::vector<int>& getShape() const override { return Shape; }
  };

  struct QualTypeHasher {
    std::size_t operator()(const QualType& Qt) const {
      return std::hash<const BuiltinType*>{}(Qt.getTypePtr());
    }
  };

  struct FunctionTypeKey {
    QualType ReturnType;
    std::vector<QualType> ParamTypes;

    bool operator==(const FunctionTypeKey& other) const {
      if (ReturnType != other.ReturnType || ParamTypes.size() != other.ParamTypes.size()) {
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
    std::size_t operator()(const FunctionTypeKey& Key) const {
      std::size_t H = QualTypeHasher{}(Key.ReturnType);
      for (const auto& ParamType: Key.ParamTypes) {
        H ^= QualTypeHasher{}(ParamType) + 0x9e3779b9 + (H << 6) + (H >> 2);
      }
      return H;
    }
  };

  struct PointerTypeKey {
    QualType PointeeType;
    bool IsMutable;

    bool operator==(const PointerTypeKey& other) const {
      return PointeeType == other.PointeeType && IsMutable == other.IsMutable;
    }
  };

  struct PointerTypeKeyHasher {
    std::size_t operator()(const PointerTypeKey& Key) const {
      std::size_t H = QualTypeHasher{}(Key.PointeeType);
      H ^= std::hash<bool>{}(Key.IsMutable) + 0x9e3779b9 + (H << 6) + (H >> 2);
      return H;
    }
  };

  struct ReferenceTypeKey {
    QualType ReferentType;
    bool IsMutable;

    bool operator==(const ReferenceTypeKey& other) const {
      return ReferentType == other.ReferentType && IsMutable == other.IsMutable;
    }
  };

  struct ReferenceTypeKeyHasher {
    std::size_t operator()(const ReferenceTypeKey& Key) const {
      std::size_t H = QualTypeHasher{}(Key.ReferentType);
      H ^= std::hash<bool>{}(Key.IsMutable) + 0x9e3779b9 + (H << 6) + (H >> 2);
      return H;
    }
  };

  struct ArrayTypeKey {
    QualType ElementType;
    bool IsMutable;
    size_t Size;

    bool operator==(const ArrayTypeKey& other) const {
      return ElementType == other.ElementType && 
        IsMutable == other.IsMutable && Size == other.Size;
    }
  };

  struct ArrayTypeKeyHasher {
    std::size_t operator()(const ArrayTypeKey& Key) const {
      std::size_t H = QualTypeHasher{}(Key.ElementType);
      H ^= std::hash<bool>{}(Key.IsMutable) + 0x9e3779b9 + (H << 6) + (H >> 2);
      H ^= std::hash<size_t>{}(Key.Size) + 0x9e3779b9 + (H << 6) + (H >> 2);
      return H;
    }
  };

};

#endif // TRSC_AST_QUALTYPE_H
