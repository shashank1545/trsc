#include <gtest/gtest.h>
#include "trsc/Sema/SymbolTable.h"
#include "trsc/Sema/Scope.h"

using namespace trsc;

TEST(SymbolTableTest, BasicScoping) {
    SymbolTable Table;
    ScopeKind Kind;
    Table.enterScope(Kind);
    
    // Assume Table.insert(Name, Symbol) exists
    // Table.insert("x", Symbol(Type::i32));
    // EXPECT_TRUE(Table.lookup("x"));
    
    Table.exitScope();
    // EXPECT_FALSE(Table.lookup("x"));
}
