#ifndef CHITON_NEON_COMPAT_H
#define CHITON_NEON_COMPAT_H

// Preserve the NEON API while using native SIMD where available (including ARM)
// or SIMDe's portable implementations on other architectures.
#ifndef SIMDE_ENABLE_NATIVE_ALIASES
#define SIMDE_ENABLE_NATIVE_ALIASES
#endif
#include <simde/arm/neon.h>

#endif
