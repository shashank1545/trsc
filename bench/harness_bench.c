/* Timing harness for trsc-compiled matmul objects.
 *
 * The trsc program's `main` is renamed to `trsc_main` via
 * `objcopy --redefine-sym main=trsc_main` and linked against this file.
 * Each call to trsc_main() performs the FULL program: host matrix
 * allocation + initialization, (GPU levels) gpu.host_register pinning,
 * kernel launch and sync — so the measured time is honest end-to-end.
 *
 * usage: mm_bench <size> <level-label> <warmup> <reps>
 * stdout: CSV rows  size,level,rep,ms,ok
 * exit: nonzero if any rep's result != 2.0f * size
 */
#define _POSIX_C_SOURCE 199309L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

float trsc_main(void);

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

int main(int argc, char **argv) {
  if (argc != 5) {
    fprintf(stderr, "usage: %s <size> <level-label> <warmup> <reps>\n", argv[0]);
    return 2;
  }
  int n = atoi(argv[1]);
  const char *level = argv[2];
  int warmup = atoi(argv[3]);
  int reps = atoi(argv[4]);
  /* sum of n terms of 1.0*2.0 — exact in f32 for every bench size */
  float expected = 2.0f * (float)n;

  /* First calls pay CUDA context init + module load; exclude them. */
  for (int i = 0; i < warmup; i++)
    (void)trsc_main();

  for (int r = 0; r < reps; r++) {
    double t0 = now_ms();
    float c00 = trsc_main();
    double t1 = now_ms();
    int ok = fabsf(c00 - expected) <= 1e-3f * expected;
    printf("%d,%s,%d,%.3f,%d\n", n, level, r, t1 - t0, ok);
    fflush(stdout);
    if (!ok) {
      fprintf(stderr, "VERIFY FAIL n=%d level=%s got %.3f want %.1f\n",
              n, level, (double)c00, (double)expected);
      return 1;
    }
  }
  return 0;
}
