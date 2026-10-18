#pragma once

#include <string>

namespace bh {

// Appends v with exactly two decimal places, rounding the exact binary64
// value half to even. Locale-independent; never uses printf or iostreams.
void append_fixed2(std::string& out, double v);

}  // namespace bh
