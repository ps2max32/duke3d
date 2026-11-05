/* ps2_softfp.c: double arithmetic for the R5900, whose FPU has none, in place
   of compiler-rt's (linked before it). That one handles everything and raises
   the FP exception flags; here the 64-bit integer unit does the usual case,
   and the result is the same, rounded to nearest even, for normal numbers.
   Denormals are taken as zero, and a result too small for a normal number
   becomes zero (no flags, as the FPU itself). Infinities and NaNs behave. */

#include <stdint.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef union { double d; u64 u; } du;
typedef union { float f; u32 u; } fu;

#define SIGN (1ull << 63)
#define HID (1ull << 52)
#define MANT (HID - 1)
#define QNAN 0x7FF8000000000000ull
#define INF 0x7FF0000000000000ull

static inline double make(u64 u) {
  du r;
  r.u = u;
  return r.d;
}

/* m (with HID set, or just past it by rounding) and a biased exponent */
static inline double pack(u64 sign, int e, u64 m) {
  if (e >= 0x7FF)
    return make(sign | INF);
  if (e <= 0)
    return make(sign);
  return make(sign | (((u64)e << 52) + (m - HID)));
}

double __muldf3(double x, double y) {
  du a = {x}, b = {y};
  u64 sign = (a.u ^ b.u) & SIGN;
  int ea = (int)(a.u >> 52) & 0x7FF, eb = (int)(b.u >> 52) & 0x7FF;
  if (ea == 0x7FF || eb == 0x7FF) {
    if ((ea == 0x7FF && (a.u & MANT)) || (eb == 0x7FF && (b.u & MANT)))
      return make(QNAN);
    if (ea == 0 || eb == 0)
      return make(QNAN); /* infinity times zero */
    return make(sign | INF);
  }
  if (ea == 0 || eb == 0)
    return make(sign);
  u64 ma = (a.u & MANT) | HID, mb = (b.u & MANT) | HID;
  /* 53 x 53 bits, by 32-bit halves */
  u64 a0 = (u32)ma, a1 = ma >> 32, b0 = (u32)mb, b1 = mb >> 32;
  u64 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  u64 mid = p01 + p10 + (p00 >> 32);
  u64 hi = p11 + (mid >> 32), lo = (mid << 32) | (u32)p00;
  /* the product is in [2^104, 2^106): 53 bits of it */
  int top = (int)(hi >> 41) & 1, sh = 52 + top;
  u64 m = (hi << (64 - sh)) | (lo >> sh), rem = lo << (64 - sh);
  int e = ea + eb - 1023 + top;
  if (rem > SIGN || (rem == SIGN && (m & 1))) {
    m++;
    if (m == HID << 1)
      m = HID, e++;
  }
  return pack(sign, e, m);
}

/* a + b of the same sign when sub is 0, else the larger minus the smaller */
static double addsub(du a, du b) {
  int ea = (int)(a.u >> 52) & 0x7FF, eb = (int)(b.u >> 52) & 0x7FF;
  if (ea == 0x7FF || eb == 0x7FF) {
    if ((ea == 0x7FF && (a.u & MANT)) || (eb == 0x7FF && (b.u & MANT)))
      return make(QNAN);
    if (ea == 0x7FF && eb == 0x7FF)
      return (a.u ^ b.u) & SIGN ? make(QNAN) : a.d;
    return ea == 0x7FF ? a.d : b.d;
  }
  if (ea == 0)
    return eb == 0 ? make(a.u & b.u & SIGN) : b.d;
  if (eb == 0)
    return a.d;
  /* a is the larger in magnitude */
  if ((a.u & ~SIGN) < (b.u & ~SIGN)) {
    du t = a;
    int te = ea;
    a = b, b = t, ea = eb, eb = te;
  }
  u64 sign = a.u & SIGN;
  int sub = ((a.u ^ b.u) & SIGN) != 0, d = ea - eb;
  /* three bits below the mantissa: guard, round, sticky... here ten, to
     normalise after a cancellation without losing any */
  u64 ma = ((a.u & MANT) | HID) << 10, mb = ((b.u & MANT) | HID) << 10;
  if (d >= 63)
    mb = 1; /* sticky only */
  else if (d) {
    u64 lost = mb << (64 - d);
    mb = (mb >> d) | (lost != 0);
  }
  u64 m;
  int e = ea;
  if (sub) {
    m = ma - mb;
    if (!m)
      return make(0);
    int z = __builtin_clzll(m) - 1; /* the top bit back to 62 */
    if (z > 0) {
      m <<= z;
      e -= z;
    }
  } else {
    m = ma + mb;
    if (m >> 63) {
      m = (m >> 1) | (m & 1);
      e++;
    }
  }
  /* bit 62 is the hidden bit: the mantissa is m >> 10 */
  u64 rem = m & 0x3FF, r = m >> 10;
  if (rem > 0x200 || (rem == 0x200 && (r & 1))) {
    r++;
    if (r == HID << 1)
      r = HID, e++;
  }
  return pack(sign, e, r);
}

double __adddf3(double x, double y) {
  du a = {x}, b = {y};
  return addsub(a, b);
}

double __subdf3(double x, double y) {
  du a = {x}, b = {y};
  b.u ^= SIGN;
  return addsub(a, b);
}

double __extendsfdf2(float x) {
  fu a = {x};
  u64 sign = (u64)(a.u >> 31) << 63;
  int e = (int)(a.u >> 23) & 0xFF;
  u64 m = a.u & 0x7FFFFF;
  if (e == 0xFF)
    return make(sign | INF | (m << 29));
  if (e == 0)
    return make(sign);
  return make(sign | ((u64)(e + 1023 - 127) << 52) | (m << 29));
}

float __truncdfsf2(double x) {
  du a = {x};
  fu r;
  u32 sign = (u32)(a.u >> 63) << 31;
  int e = (int)(a.u >> 52) & 0x7FF;
  if (e == 0x7FF) {
    r.u = sign | 0x7F800000u | (a.u & MANT ? 0x400000u : 0);
    return r.f;
  }
  e += 127 - 1023;
  if (e <= 0) {
    r.u = sign;
    return r.f;
  }
  u64 m = a.u & MANT;
  u32 f = (u32)(m >> 29), rem = (u32)m & 0x1FFFFFFFu;
  u32 v = ((u32)e << 23) + f;
  if (rem > 0x10000000u || (rem == 0x10000000u && (f & 1)))
    v++;
  if (v >= 0x7F800000u)
    v = 0x7F800000u;
  r.u = sign | v;
  return r.f;
}
