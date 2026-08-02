#include "trsc/AST/AST.h"

#include <type_traits>

namespace trsc {

// Arena-allocated nodes are never destroyed, only freed wholesale. A node
// that grows an owning member (std::string, std::vector, ...) would silently
// leak; catch it at compile time instead.
#define TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(NODE)                               \
  static_assert(std::is_trivially_destructible<NODE>::value,                   \
                #NODE " must stay trivially destructible: AST nodes live in "  \
                      "the ASTContext arena and their destructors never run")

TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(Program);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(TypeName);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(PointerTypeName);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(ReferenceTypeName);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(ArrayTypeName);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(IntExpr);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(FloatExpr);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(BoolExpr);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(VarExpr);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(RefrExpr);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(BinExpr);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(ASExpr);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(ArrayExpr);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(ArrayAccessExpr);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(RangeExpr);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(FunCall);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(LetStmt);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(BlockStmt);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(IfStmt);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(ExprStmt);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(ForStmt);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(WhileStmt);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(FuncDecl);
TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE(ReturnStmt);

#undef TRSC_ASSERT_TRIVIALLY_DESTRUCTIBLE

} // namespace trsc
