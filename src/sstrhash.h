#pragma once

#include "hash_string.h"

// from https://wowdev.wiki/SStrHash
static uint32_t const s_hashtable[ 16 ] = {
  0x486e26ee, 0xdcaa16b3, 0xe1918eef, 0x202dafdb,
  0x341c7dc7, 0x1c365303, 0x40ef2d37, 0x65fd5e49,
  0xd6057177, 0x904ece93, 0x1c38024f, 0x98fd323b,
  0xe3061ae7, 0xa39b0fa1, 0x9797f25f, 0xe4444563,
};

inline uint32_t s_str_hash( hash_string_t& str )
{
  uint32_t seed = str.a;
  uint32_t shift = str.b;
  for ( size_t i = str.offset; i < str.size; i++ )
  {
    seed = ( s_hashtable[ str[ i ] >> 4 ] - s_hashtable[ str[ i ] & 0xf ] ) ^ ( shift + seed );
    shift = str[ i ] + seed + 33 * shift + 3;
  }

  return seed ? seed : 1;
}

inline void s_str_hash_precompute( hash_string_t& str, size_t length )
{
  uint32_t seed = 0x7fed7fed;
  uint32_t shift = 0xeeeeeeee;

  for ( size_t i = str.offset; i < length; i++ )
  {
    seed = ( s_hashtable[ str[ i ] >> 4 ] - s_hashtable[ str[ i ] & 0xf ] ) ^ ( shift + seed );
    shift = str[ i ] + seed + 33 * shift + 3;
  }

  str.a = seed;
  str.b = shift;
}
