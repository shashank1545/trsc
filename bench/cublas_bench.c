/* cuBLAS sgemm baseline for the trsc matmul benchmark.
 *
 * Two framings, matching bench/README.md methodology:
 *   e2e    — one-time setup (pinned host buffers, device buffers, handle)
 *            outside the loop; per rep times init loops + H2D + sgemm
 *            + D2H + sync. Allocation/handle costs are amortized as a
 *            real application would.
 *   kernel — persistent handle/buffers; CUDA events around sgemm only.
 *
 * trsc computes row-major C = A*B. cuBLAS is column-major, so we call
 * sgemm with swapped operands: column-major B*A == row-major (A*B)^T
 * stored column-major == row-major A*B. Verified with non-uniform
 * matrices via --selftest.
 *
 * build: cc cublas_bench.c -I$CUDA_HOME/include -L$CUDA_HOME/lib64 \
 *           -lcublas -lcudart -Wl,-rpath,$CUDA_HOME/lib64 -o cublas_bench
 * usage: cublas_bench <size> <mode:e2e|kernel> <warmup> <reps>
 *        cublas_bench --selftest
 * stdout: CSV rows  size,cublas_<mode>,rep,ms,ok
 */
#define _POSIX_C_SOURCE 199309L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cublas_v2.h>
#include <cuda_runtime.h>

#define CHECK_CUDA(call)                                                     \
  do {                                                                       \
    cudaError_t err_ = (call);                                               \
    if (err_ != cudaSuccess) {                                               \
      fprintf(stderr, "%s:%d CUDA error: %s\n", __FILE__, __LINE__,          \
              cudaGetErrorString(err_));                                     \
      exit(2);                                                               \
    }                                                                        \
  } while (0)

#define CHECK_CUBLAS(call)                                                   \
  do {                                                                       \
    cublasStatus_t st_ = (call);                                             \
    if (st_ != CUBLAS_STATUS_SUCCESS) {                                      \
      fprintf(stderr, "%s:%d cuBLAS error: %d\n", __FILE__, __LINE__,        \
              (int)st_);                                                     \
      exit(2);                                                               \
    }                                                                        \
  } while (0)

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

/* Row-major C = A*B through column-major cublasSgemm by swapping operands. */
static void sgemm_rowmajor(cublasHandle_t h, int n, const float *dA,
                           const float *dB, float *dC) {
  const float alpha = 1.0f, beta = 0.0f;
  CHECK_CUBLAS(cublasSgemm(h, CUBLAS_OP_N, CUBLAS_OP_N, n, n, n, &alpha,
                           dB, n, dA, n, &beta, dC, n));
}

static int selftest(void) {
  /* Non-uniform 2x2: A=[[1,2],[3,4]], B=[[5,6],[7,8]]
   * row-major A*B = [[19,22],[43,50]] — catches any layout mistake the
   * benchmark's uniform data cannot. */
  const int n = 2;
  float A[4] = {1, 2, 3, 4}, B[4] = {5, 6, 7, 8}, C[4] = {0};
  float want[4] = {19, 22, 43, 50};
  float *dA, *dB, *dC;
  CHECK_CUDA(cudaMalloc((void **)&dA, sizeof A));
  CHECK_CUDA(cudaMalloc((void **)&dB, sizeof B));
  CHECK_CUDA(cudaMalloc((void **)&dC, sizeof C));
  CHECK_CUDA(cudaMemcpy(dA, A, sizeof A, cudaMemcpyHostToDevice));
  CHECK_CUDA(cudaMemcpy(dB, B, sizeof B, cudaMemcpyHostToDevice));
  cublasHandle_t h;
  CHECK_CUBLAS(cublasCreate(&h));
  sgemm_rowmajor(h, n, dA, dB, dC);
  CHECK_CUDA(cudaMemcpy(C, dC, sizeof C, cudaMemcpyDeviceToHost));
  cublasDestroy(h);
  cudaFree(dA); cudaFree(dB); cudaFree(dC);
  for (int i = 0; i < 4; i++) {
    if (fabsf(C[i] - want[i]) > 1e-4f) {
      fprintf(stderr, "selftest FAIL: C[%d]=%f want %f\n", i,
              (double)C[i], (double)want[i]);
      return 1;
    }
  }
  printf("selftest OK: row-major swap trick verified\n");
  return 0;
}

static int verify_c00(const float *C, int n) {
  float expected = 2.0f * (float)n;
  return fabsf(C[0] - expected) <= 1e-3f * expected;
}

static void run_e2e(int n, int warmup, int reps) {
  size_t bytes = (size_t)n * n * sizeof(float);
  /* One-time setup a real application amortizes across many GEMMs:
   * pinned host buffers (full PCIe bandwidth, no per-rep page faults),
   * persistent device buffers, and one cuBLAS handle. The timed region
   * keeps the honest per-call work: matrix init + H2D + sgemm + D2H + sync. */
  float *A, *B, *C;
  CHECK_CUDA(cudaMallocHost((void **)&A, bytes));
  CHECK_CUDA(cudaMallocHost((void **)&B, bytes));
  CHECK_CUDA(cudaMallocHost((void **)&C, bytes));
  float *dA, *dB, *dC;
  CHECK_CUDA(cudaMalloc((void **)&dA, bytes));
  CHECK_CUDA(cudaMalloc((void **)&dB, bytes));
  CHECK_CUDA(cudaMalloc((void **)&dC, bytes));
  cublasHandle_t h;
  CHECK_CUBLAS(cublasCreate(&h));

  for (int r = -warmup; r < reps; r++) {
    double t0 = now_ms();
    for (size_t i = 0; i < (size_t)n * n; i++) {
      A[i] = 1.0f; B[i] = 2.0f; C[i] = 0.0f;
    }
    CHECK_CUDA(cudaMemcpy(dA, A, bytes, cudaMemcpyHostToDevice));
    CHECK_CUDA(cudaMemcpy(dB, B, bytes, cudaMemcpyHostToDevice));
    sgemm_rowmajor(h, n, dA, dB, dC);
    CHECK_CUDA(cudaMemcpy(C, dC, bytes, cudaMemcpyDeviceToHost));
    CHECK_CUDA(cudaDeviceSynchronize());
    double t1 = now_ms();
    int ok = verify_c00(C, n);
    if (r >= 0) {
      printf("%d,cublas_e2e,%d,%.3f,%d\n", n, r, t1 - t0, ok);
      fflush(stdout);
    }
    if (!ok) { fprintf(stderr, "VERIFY FAIL cublas_e2e n=%d\n", n); exit(1); }
  }

  cublasDestroy(h);
  cudaFree(dA); cudaFree(dB); cudaFree(dC);
  cudaFreeHost(A); cudaFreeHost(B); cudaFreeHost(C);
}

static void run_kernel(int n, int warmup, int reps) {
  size_t bytes = (size_t)n * n * sizeof(float);
  float *A = malloc(bytes), *B = malloc(bytes), *C = malloc(bytes);
  if (!A || !B || !C) { fprintf(stderr, "host alloc failed\n"); exit(2); }
  for (size_t i = 0; i < (size_t)n * n; i++) {
    A[i] = 1.0f; B[i] = 2.0f; C[i] = 0.0f;
  }
  float *dA, *dB, *dC;
  CHECK_CUDA(cudaMalloc((void **)&dA, bytes));
  CHECK_CUDA(cudaMalloc((void **)&dB, bytes));
  CHECK_CUDA(cudaMalloc((void **)&dC, bytes));
  CHECK_CUDA(cudaMemcpy(dA, A, bytes, cudaMemcpyHostToDevice));
  CHECK_CUDA(cudaMemcpy(dB, B, bytes, cudaMemcpyHostToDevice));
  cublasHandle_t h;
  CHECK_CUBLAS(cublasCreate(&h));
  cudaEvent_t start, stop;
  CHECK_CUDA(cudaEventCreate(&start));
  CHECK_CUDA(cudaEventCreate(&stop));

  for (int i = 0; i < warmup; i++)
    sgemm_rowmajor(h, n, dA, dB, dC);
  CHECK_CUDA(cudaDeviceSynchronize());

  for (int r = 0; r < reps; r++) {
    CHECK_CUDA(cudaEventRecord(start, 0));
    sgemm_rowmajor(h, n, dA, dB, dC);
    CHECK_CUDA(cudaEventRecord(stop, 0));
    CHECK_CUDA(cudaEventSynchronize(stop));
    float ms = 0.0f;
    CHECK_CUDA(cudaEventElapsedTime(&ms, start, stop));
    CHECK_CUDA(cudaMemcpy(C, dC, sizeof(float), cudaMemcpyDeviceToHost));
    int ok = verify_c00(C, n);
    printf("%d,cublas_kernel,%d,%.3f,%d\n", n, r, (double)ms, ok);
    fflush(stdout);
    if (!ok) { fprintf(stderr, "VERIFY FAIL cublas_kernel n=%d\n", n); exit(1); }
  }

  cudaEventDestroy(start); cudaEventDestroy(stop);
  cublasDestroy(h);
  cudaFree(dA); cudaFree(dB); cudaFree(dC);
  free(A); free(B); free(C);
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--selftest") == 0)
    return selftest();
  if (argc != 5) {
    fprintf(stderr,
            "usage: %s <size> <mode:e2e|kernel> <warmup> <reps> | --selftest\n",
            argv[0]);
    return 2;
  }
  int n = atoi(argv[1]);
  int warmup = atoi(argv[3]);
  int reps = atoi(argv[4]);
  if (strcmp(argv[2], "e2e") == 0)
    run_e2e(n, warmup, reps);
  else if (strcmp(argv[2], "kernel") == 0)
    run_kernel(n, warmup, reps);
  else {
    fprintf(stderr, "unknown mode: %s\n", argv[2]);
    return 2;
  }
  return 0;
}
