// RUN: %trsc -dump-symbol %s | %FileCheck %s

fn main() {
    let x = 1;
    {
        let x = 2.0;
        {
            let x = true;
        }
    }
}

// CHECK: SYMBOL TABLE DUMP
// CHECK: Scope #0 - Global (Depth: 0)
// CHECK: main                 : () -> () [Function]
//
// CHECK: Scope #1 - Function (Depth: 1)
// CHECK: x                    : i32 [Variable]
//
// CHECK: Scope #2 - Blockstmt (Depth: 2)
// CHECK: x                    : f64 [Variable]
//
// CHECK: Scope #3 - Blockstmt (Depth: 3)
// CHECK: x                    : bool [Variable]
