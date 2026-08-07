fn main() {
    let i8_value: i8 = 8;
    let u16_value: u16 = 16;
    let i64_value: i64 = 64;
    let f32_value: f32 = 3.5;
    let f64_value: f64 = 2.25;
    let bool_value = true;

    println!("{} {} {} {} {} {}", i8_value, u16_value, i64_value,
             f32_value, f64_value, bool_value);
    println!("{:?}", bool_value);
    println!("literal {{}} and newline\n");
    println!("quote:\" backslash:\\ tab:\tend");
    println!();

    // Floats print the shortest form that round-trips, without switching to
    // exponent notation, matching Rust's Display rather than printf's %g.
    let repeating: f64 = 0.3333333333333333;
    let large: f64 = 10000000000.0;
    let whole: f64 = 1.0;
    let small: f32 = 0.1;
    println!("{} {} {} {}", repeating, large, whole, small);

    // Interleaving with control flow and a call across function boundaries.
    let mut counter: i32 = 0;
    while counter < 3 {
        println!("iter {}", counter);
        counter += 1;
    }
    print_doubled(21);
}

fn print_doubled(n: i32) {
    println!("doubled {}", n * 2);
}
