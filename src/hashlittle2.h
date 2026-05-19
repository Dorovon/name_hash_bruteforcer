#pragma once

#include "hash_string.h"

inline uint32_t rotate_left( uint32_t value, size_t distance )
{
  return ( value << distance ) | ( value >> ( 32 - distance ) );
}

inline void mix_block( const unsigned char*& k, size_t& length, uint32_t& a, uint32_t& b, uint32_t& c )
{
  a += k[ 0 ] + ( static_cast<uint32_t>( k[ 1 ] ) << 8 ) + ( static_cast<uint32_t>( k[  2 ] ) << 16 ) + ( static_cast<uint32_t>( k[  3 ] ) << 24 );
  b += k[ 4 ] + ( static_cast<uint32_t>( k[ 5 ] ) << 8 ) + ( static_cast<uint32_t>( k[  6 ] ) << 16 ) + ( static_cast<uint32_t>( k[  7 ] ) << 24 );
  c += k[ 8 ] + ( static_cast<uint32_t>( k[ 9 ] ) << 8 ) + ( static_cast<uint32_t>( k[ 10 ] ) << 16 ) + ( static_cast<uint32_t>( k[ 11 ] ) << 24 );
  a -= c; a ^= rotate_left( c,  4 ); c += b;
  b -= a; b ^= rotate_left( a,  6 ); a += c;
  c -= b; c ^= rotate_left( b,  8 ); b += a;
  a -= c; a ^= rotate_left( c, 16 ); c += b;
  b -= a; b ^= rotate_left( a, 19 ); a += c;
  c -= b; c ^= rotate_left( b,  4 ); b += a;
  length -= 12;
  k += 12;
}

inline uint64_t hashlittle2( hash_string_t& str )
{
  const unsigned char* k;
  uint32_t a, b, c;
  size_t length = str.current_size;
  if ( str.offset > 0 )
  {
    k = &str[ str.offset ];
    length -= str.offset;
    const hash_state_t& state = str.state();
    a = state.a;
    b = state.b;
    c = state.c;
  }
  else
  {
    k = str.data();
    a = b = c = 0xdeadbeef + static_cast<uint32_t>( str.current_size );
  }

  while ( length > 12 )
    mix_block( k, length, a, b, c );

  a += k[ 0 ] + ( static_cast<uint32_t>( k[ 1 ] ) << 8 ) + ( static_cast<uint32_t>( k[  2 ] ) << 16 ) + ( static_cast<uint32_t>( k[  3 ] ) << 24 );
  b += k[ 4 ] + ( static_cast<uint32_t>( k[ 5 ] ) << 8 ) + ( static_cast<uint32_t>( k[  6 ] ) << 16 ) + ( static_cast<uint32_t>( k[  7 ] ) << 24 );
  c += k[ 8 ] + ( static_cast<uint32_t>( k[ 9 ] ) << 8 ) + ( static_cast<uint32_t>( k[ 10 ] ) << 16 ) + ( static_cast<uint32_t>( k[ 11 ] ) << 24 );

  c ^= b; c -= rotate_left( b, 14 );
  a ^= c; a -= rotate_left( c, 11 );
  b ^= a; b -= rotate_left( a, 25 );
  c ^= b; c -= rotate_left( b, 16 );
  a ^= c; a -= rotate_left( c,  4 );
  b ^= a; b -= rotate_left( a, 14 );
  c ^= b; c -= rotate_left( b, 24 );

  return ( static_cast<uint64_t>( c ) << 32 ) | b;
}

inline hash_state_t hashlittle2_precompute( hash_string_t& str, size_t length, size_t string_size )
{
  const unsigned char* k;
  uint32_t a, b, c;
  k = str.data();
  a = b = c = 0xdeadbeef + static_cast<uint32_t>( string_size );

  while ( length >= 12 )
    mix_block( k, length, a, b, c );

  return { string_size, a, b, c };
}
