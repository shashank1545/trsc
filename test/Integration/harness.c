/* Integration test harness: the trsc-compiled main is renamed to trsc_main
 * via objcopy (a float return in main makes the process exit code useless),
 * then linked against this file which prints the result for comparison. */
#include <stdio.h>

float trsc_main(void);

int main(void) {
  printf("%.1f\n", trsc_main());
  return 0;
}
