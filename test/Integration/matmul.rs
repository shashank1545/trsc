fn main() -> f32 {
    let a: [[f32; 32]; 32] = [[1.0; 32]; 32];
    let b: [[f32; 32]; 32] = [[2.0; 32]; 32];
    let mut c: [[f32; 32]; 32] = [[0.0; 32]; 32];

    for i in 0..32 {
        for j in 0..32 {
            for k in 0..32 {
                c[i][j] = c[i][j] + a[i][k] * b[k][j];
            }
        }
    }
    return c[0][0];
}
