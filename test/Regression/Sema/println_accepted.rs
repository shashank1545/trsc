// RUN: %trsc -dump-typedast %s | %FileCheck %s

fn main() {
    let i: i32 = 1;
    let u: u8 = 2;
    let f: f32 = 3.5;
    let b = true;

    // Every scalar kind, the debug spec, brace escapes and the empty form are
    // all accepted, and the macro itself is typed as unit.
    println!("{} {} {} {}", i, u, f, b);
    println!("{:?}", i);
    println!("{{literal}}");
    println!();
}

// The wildcards skip the node address and source range that the dump prints
// between the node name and its attributes.
// CHECK: MacroCall {{.*}} 'println!' format='{} {} {} {}' '()'
// CHECK: MacroCall {{.*}} 'println!' format='{:?}' '()'
// The character classes keep FileCheck from reading {{...}} as a regex block.
// CHECK: MacroCall {{.*}} 'println!' format='{{[{][{]literal[}][}]}}' '()'
// CHECK: MacroCall {{.*}} 'println!' '()'
