//===- PrintRuntime.cpp - trsc println runtime ----------------------------===//
//
// Runtime entry points used by trsc-generated println! calls.
//
// Formatting follows Rust's `{}` (Display) output rather than printf's, most
// visibly for floats: Rust prints the shortest decimal form that round-trips
// back to the same value and never switches to exponent notation, whereas
// "%g" caps at six significant digits and prints 1e10 as "1e+10".
//
//===----------------------------------------------------------------------===//

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <system_error>

namespace {

template <typename T> void printFloat(T Value) {
  if (std::isnan(Value)) {
    std::fputs("NaN", stdout);
    return;
  }
  if (std::isinf(Value)) {
    std::fputs(Value < 0 ? "-inf" : "inf", stdout);
    return;
  }

  // chars_format::fixed asks for the shortest non-exponential form that parses
  // back to Value, which is exactly Rust's Display contract. The buffer has to
  // hold the widest such form: a denormal f64 needs ~1080 characters.
  char Buffer[1200];
  std::to_chars_result Result = std::to_chars(Buffer, Buffer + sizeof(Buffer),
                                              Value, std::chars_format::fixed);
  if (Result.ec != std::errc()) {
    // Unreachable with the buffer above; degrade rather than print nothing.
    std::printf("%f", static_cast<double>(Value));
    return;
  }

  std::fwrite(Buffer, 1, static_cast<std::size_t>(Result.ptr - Buffer), stdout);
}

} // namespace

extern "C" void trsc_print_i64(int64_t Value) {
  std::printf("%lld", static_cast<long long>(Value));
}

extern "C" void trsc_print_u64(uint64_t Value) {
  std::printf("%llu", static_cast<unsigned long long>(Value));
}

extern "C" void trsc_print_f32(float Value) { printFloat(Value); }

extern "C" void trsc_print_f64(double Value) { printFloat(Value); }

extern "C" void trsc_print_bool(int32_t Value) {
  std::fputs(Value ? "true" : "false", stdout);
}

// A literal chunk of a format string. Not NUL-terminated: the length comes
// from the generated code, so a "\0" escape inside the literal still prints.
extern "C" void trsc_print_str(const char *Data, uint64_t Length) {
  if (Data != nullptr && Length != 0)
    std::fwrite(Data, 1, static_cast<std::size_t>(Length), stdout);
}

extern "C" void trsc_print_newline() {
  std::fputc('\n', stdout);
  std::fflush(stdout);
}
