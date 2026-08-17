/* Pin glibc symbol versions so a trsc binary produced on a bleeding-edge host
 * (Arch, glibc 2.44) still loads on distros that ship an older glibc.
 *
 * This is linked into the compiler itself, not into the programs it emits -
 * those get lib/Runtime instead.  It must stay an OBJECT library; see
 * lib/Support/CMakeLists.txt for why an archive would silently defeat it.
 *
 * Two distinct problems are handled here:
 *
 *   1. glibc 2.43 and 2.44 published new versions of several libm entry
 *      points.  The LLVM/MLIR static archives reference the bare names, so the
 *      final link stamps every one of them with the host's newest version and
 *      the loader then rejects the binary on any older glibc.  The 2.2.5
 *      symbols are still exported for compatibility, so we define the bare
 *      name ourselves and forward to the old versioned alias.
 *
 *   2. The LLVM archives (and std::stoi in lib/Basic/CommandLine.cpp) reference
 *      __isoc23_strtol and friends, which only exist from glibc 2.38.  Those
 *      are real symbol names baked into the objects, not a versioning artifact,
 *      so they have to be defined rather than re-pointed.
 *
 * These wrappers remove the known 2.36+ requirements. The final floor still
 * depends on every object and shared library selected by the link, so the
 * release workflow checks that the executable requires no newer than
 * GLIBC_2.35.
 *
 * Nothing in here includes a libc header on purpose: on a C23-defaulting
 * compiler <stdlib.h> would rewrite our own strtol calls back into
 * __isoc23_strtol and the wrappers would recurse forever.
 */

/* features.h is the one exception: it defines __GLIBC__ (and is absent on musl,
 * where the guard below then compiles the whole file away) without declaring a
 * single function, so it cannot trigger the redirect described above. */
#if defined(__linux__) && defined(__has_include)
#if __has_include(<features.h>)
#include <features.h>
#endif
#endif

#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)

#define TRSC_PIN(alias, name, version)                                         \
  __asm__(".symver " #alias "," #name "@GLIBC_" version)

/* --- libm: versions bumped in 2.43 (float) and 2.44 (double) ------------- */

#define TRSC_PIN_MATH1(name, type, version)                                    \
  extern type trsc_old_##name(type);                                           \
  TRSC_PIN(trsc_old_##name, name, version);                                    \
  type name(type x);                                                           \
  type name(type x) { return trsc_old_##name(x); }

#define TRSC_PIN_MATH2(name, type, version)                                    \
  extern type trsc_old_##name(type, type);                                     \
  TRSC_PIN(trsc_old_##name, name, version);                                    \
  type name(type x, type y);                                                   \
  type name(type x, type y) { return trsc_old_##name(x, y); }

TRSC_PIN_MATH1(acosf, float, "2.2.5");
TRSC_PIN_MATH1(acoshf, float, "2.2.5");
TRSC_PIN_MATH1(asinf, float, "2.2.5");
TRSC_PIN_MATH1(atanhf, float, "2.2.5");
TRSC_PIN_MATH1(coshf, float, "2.2.5");
TRSC_PIN_MATH1(log10f, float, "2.2.5");
TRSC_PIN_MATH1(sinhf, float, "2.2.5");
TRSC_PIN_MATH1(sqrtf, float, "2.2.5");
TRSC_PIN_MATH2(atan2f, float, "2.2.5");

TRSC_PIN_MATH1(cosh, double, "2.2.5");
TRSC_PIN_MATH1(sinh, double, "2.2.5");

/* --- the C23 strtol family, new in glibc 2.38 ---------------------------- */

/* The C23 variants only differ from the classic ones by accepting a 0b/0B
 * prefix when base is 0 or 2.  Nothing in trsc or in LLVM's option parsing
 * relies on that, so forwarding to the pre-C23 parser is the whole job. */

#define TRSC_PIN_STRTO(name, type)                                             \
  extern type trsc_old_##name(const char *, char **, int);                     \
  TRSC_PIN(trsc_old_##name, name, "2.2.5");                                    \
  type __isoc23_##name(const char *nptr, char **endptr, int base);             \
  type __isoc23_##name(const char *nptr, char **endptr, int base) {            \
    return trsc_old_##name(nptr, endptr, base);                                \
  }

TRSC_PIN_STRTO(strtol, long)
TRSC_PIN_STRTO(strtoll, long long)
TRSC_PIN_STRTO(strtoul, unsigned long)
TRSC_PIN_STRTO(strtoull, unsigned long long)

/* arc4random was added to glibc in 2.36. The kernel getrandom syscall has
 * exposed a glibc 2.25 symbol for much longer and provides the same source of
 * entropy needed by LLVM's randomization helpers. */
extern long trsc_old_getrandom(void *, unsigned long, unsigned int);
TRSC_PIN(trsc_old_getrandom, getrandom, "2.25");

unsigned int arc4random(void) {
  unsigned int value = 0;
  if (trsc_old_getrandom(&value, sizeof(value), 0) == (long)sizeof(value))
    return value;

  /* getrandom is available on supported Linux kernels. Keep a deterministic
   * fallback for unusual syscall failures rather than reintroducing the newer
   * glibc dependency. */
  static unsigned int state = 0x9e3779b9u;
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

#endif /* linux && glibc && x86_64 */
