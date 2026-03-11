#include <gtest/gtest.h>
#include "trsc/Sema/SymbolTable.h"

using namespace trsc::Sema;

TEST(SymbolTableTest, BasicScoping) {
    SymbolTable Table;
    Table.enterScope();
    
    // Assume Table.insert(Name, Symbol) exists
    // Table.insert("x", Symbol(Type::i32));
    // EXPECT_TRUE(Table.lookup("x"));
    
    Table.exitScope();
    // EXPECT_FALSE(Table.lookup("x"));
}
