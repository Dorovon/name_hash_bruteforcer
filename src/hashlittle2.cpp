#include "hashlittle2.h"

uint32_t rotate_left( uint32_t value, size_t distance )
{
  return ( value << distance ) | ( value >> ( 32 - distance ) );
}

uint64_t hashlittle2( hash_string_t& str, size_t length, bool save_state )
{
  const unsigned char* k;
  uint32_t a, b, c;
  if ( length == 0 )
    length = str.size;
  if ( str.offset > 0 )
  {
    k = &str[ str.offset ];
    length -= str.offset;
    a = str.a;
    b = str.b;
    c = str.c;
  }
  else
  {
    k = str.data();
    a = b = c = 0xdeadbeef + static_cast<uint32_t>( str.size );
  }

  while ( length > 12 || ( length == 12 && save_state ) )
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

  if ( save_state )
  {
    str.a = a;
    str.b = b;
    str.c = c;
    return 0;
  }

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
