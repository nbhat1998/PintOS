#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <stdint.h>

/* Fractional Offset */
#define f (int32_t)(1 << 14)

/* Functions for fixed point arithmetic */
inline int32_t convert_to_fixed_point(int n)
{
  return ((int32_t)n) * f;
}


inline int truncate_to_integer(int32_t x)
{
  return (int)(x / f);
}

inline int convert_to_nearest_integer(int32_t x)
{
    return (x >= 0) ? (int)(x + (int32_t)(f / (int32_t)2)) / f
                    : (int)(x - (int32_t)(f / (int32_t)2)) / f;
}

inline int32_t add(int32_t x, int32_t y)
{
  return x + y;
}

inline int32_t subtract(int32_t x, int32_t y)
{
  return x - y;
}

inline int32_t add_int_to_fixed(int n, int32_t x)
{
  int32_t y = convert_to_fixed_point(n);
  return x + y;
}

inline int32_t subtract_int_from_fixed(int n, int32_t x)
{
  int32_t y = convert_to_fixed_point(n);
  return x - y;
}

inline int32_t multiply(int32_t x, int32_t y)
{
  return (((int64_t)x) * y) / f;
}

inline int32_t multiply_fixed_by_int(int32_t x, int m)
{
  int32_t y = convert_to_fixed_point(m);
  return (((int64_t)x) * y) / f;
}

inline int32_t divide(int32_t x, int32_t y)
{
  return (((int64_t)x) * f) / y;
}

inline int32_t divide_fixed_by_int(int32_t x, int m)
{
  int32_t y = convert_to_fixed_point(m);
  return (((int64_t)x) * f) / y;
}

#endif 