#pragma once

#include <cmath>
#include <cstdlib>
#include <iostream>

#define CHECK(condition)                                                         \
  do {                                                                           \
    if (!(condition)) {                                                          \
      std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: "           \
                << #condition << '\n';                                           \
      return EXIT_FAILURE;                                                       \
    }                                                                            \
  } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                                  \
  CHECK(std::fabs((actual) - (expected)) <= (tolerance))

