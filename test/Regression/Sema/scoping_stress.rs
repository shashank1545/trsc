// RUN: %trsc -dump-symbol %s | %FileCheck %s

fn main() {
    let x = 1;
    {
        let y = 2;
        {
            let z = 3;
            let res = x + y + z;
        }
    }
}

// CHECK: SYMBOL TABLE DUMP
// CHECK: Total scopes: 4
// 
// CHECK: Scope #0 - Global (Depth: 0)
// CHECK:   main                 : () -> () [Function]
//
// CHECK: Scope #1 - Function (Depth: 1)
// CHECK:   x                    : i32 [Variable]
// 
// CHECK: Scope #2 - Blockstmt (Depth: 2)
// CHECK:   y                    : i32 [Variable]
// 
// CHECK: Scope #3 - Blockstmt (Depth: 3)
// CHECK:   res                  : i32 [Variable]
// CHECK:   z                    : i32 [Variable]
//
