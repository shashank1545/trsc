// Numerical correctness test: 128x128 f32 matmul with position-dependent
// integer inputs (values 0..7, exact in f32). The first nest is recognized
// as trscd.gemm and lowered per --matmul-opt-level; the second nest keeps a
// scalar accumulator so recognition fails and it stays a CPU reference.
// Returns the number of elements where |c - r| exceeds tolerance (0.0 = pass).
fn main() -> f32 {
    let zero: f32 = 0.0;
    let one: f32 = 1.0;
    let eps: f32 = 0.001;
    let mut a: [[f32; 128]; 128] = [[0.0; 128]; 128];
    let mut b: [[f32; 128]; 128] = [[0.0; 128]; 128];
    let mut c: [[f32; 128]; 128] = [[0.0; 128]; 128];
    let mut r: [[f32; 128]; 128] = [[0.0; 128]; 128];

    for i in 0..128 {
        for j in 0..128 {
            let va = i * 7 + j * 13;
            let vb = i * 11 + j * 5;
            a[i][j] = (va - (va / 8) * 8) as f32;
            b[i][j] = (vb - (vb / 8) * 8) as f32;
        }
    }

    // CPU reference first: the scalar accumulator keeps this nest out of
    // matmul recognition. (Order matters: a recognized nest followed by a
    // scalar-accumulator nest crashes the pipeline.)
    for i in 0..128 {
        for j in 0..128 {
            let mut s: f32 = 0.0;
            for k in 0..128 {
                s = s + a[i][k] * b[k][j];
            }
            r[i][j] = s;
        }
    }

    // Separator store: loop fusion only fuses immediately adjacent loops,
    // and fusing the reference nest into the matmul nest would break
    // recognition. c is all zeros here, so this is a no-op.
    c[0][0] = zero;

    for i in 0..128 {
        for j in 0..128 {
            for k in 0..128 {
                c[i][j] = c[i][j] + a[i][k] * b[k][j];
            }
        }
    }

    let mut bad: f32 = 0.0;
    for i in 0..128 {
        for j in 0..128 {
            let mut d: f32 = c[i][j] - r[i][j];
            if d < zero {
                d = zero - d;
            }
            let mut tol: f32 = eps * r[i][j];
            if tol < eps {
                tol = eps;
            }
            if d > tol {
                bad = bad + one;
            }
        }
    }
    return bad;
}
