typedef ushort uint16_t;
typedef uint uint32_t;
typedef ulong uint64_t;

constant uchar letters[ NUM_LETTERS ] = LETTERS;

#if NUM_INDICES
constant uint16_t indices[ NUM_INDICES ] = { INDICES };
#endif

#if NUM_INDICES2
constant uint16_t indices2[ NUM_INDICES2 ] = { INDICES2 };
#endif

inline uint32_t rotate_left( uint32_t value, uint32_t distance )
{
  return ( value << distance ) | ( value >> ( 32 - distance ) );
}

inline uint64_t hashlittle2( unsigned char* k )
{
  uint32_t a = A;
  uint32_t b = B;
  uint32_t c = C;

  for ( short i = 0; i < LENGTH; i += 12 )
  {
    a += k[ i + 0 ] + ( ( uint32_t )( k[ i + 1 ] ) << 8 ) + ( ( uint32_t )( k[ i +  2 ] ) << 16 ) + ( ( uint32_t )( k[ i +  3 ] ) << 24 );
    b += k[ i + 4 ] + ( ( uint32_t )( k[ i + 5 ] ) << 8 ) + ( ( uint32_t )( k[ i +  6 ] ) << 16 ) + ( ( uint32_t )( k[ i +  7 ] ) << 24 );
    c += k[ i + 8 ] + ( ( uint32_t )( k[ i + 9 ] ) << 8 ) + ( ( uint32_t )( k[ i + 10 ] ) << 16 ) + ( ( uint32_t )( k[ i + 11 ] ) << 24 );
    a -= c; a ^= rotate_left( c,  4 ); c += b;
    b -= a; b ^= rotate_left( a,  6 ); a += c;
    c -= b; c ^= rotate_left( b,  8 ); b += a;
    a -= c; a ^= rotate_left( c, 16 ); c += b;
    b -= a; b ^= rotate_left( a, 19 ); a += c;
    c -= b; c ^= rotate_left( b,  4 ); b += a;
  }

  a += k[ LENGTH + 0 ] + ( ( uint32_t )( k[ LENGTH + 1 ] ) << 8 ) + ( ( uint32_t )( k[ LENGTH +  2 ] ) << 16 ) + ( ( uint32_t )( k[ LENGTH +  3 ] ) << 24 );
  b += k[ LENGTH + 4 ] + ( ( uint32_t )( k[ LENGTH + 5 ] ) << 8 ) + ( ( uint32_t )( k[ LENGTH +  6 ] ) << 16 ) + ( ( uint32_t )( k[ LENGTH +  7 ] ) << 24 );
  c += k[ LENGTH + 8 ] + ( ( uint32_t )( k[ LENGTH + 9 ] ) << 8 ) + ( ( uint32_t )( k[ LENGTH + 10 ] ) << 16 ) + ( ( uint32_t )( k[ LENGTH + 11 ] ) << 24 );

  c ^= b; c -= rotate_left( b, 14 );
  a ^= c; a -= rotate_left( c, 11 );
  b ^= a; b -= rotate_left( a, 25 );
  c ^= b; c -= rotate_left( b, 16 );
  a ^= c; a -= rotate_left( c,  4 );
  b ^= a; b -= rotate_left( a, 14 );
  c ^= b; c -= rotate_left( b, 24 );

  return ( ( uint64_t )( c ) << 32 ) | b;
}

kernel void bruteforce( global const size_t* initial_counts, global uint* num_results, global size_t* result_id, global const uint64_t* hashes )
{
  size_t count = get_global_id( 0 );

  // initialize the string
  unsigned char str[ DATA_LENGTH ] = { STR,STRPAD };
  unsigned char letter;
  for ( size_t i = 0; i < NUM_INDICES; i++ )
  {
    count += initial_counts[ i ];
    letter = letters[ count % NUM_LETTERS ];
    str[ indices[ i ] ] = letter;
#if NUM_INDICES2
    if ( i < NUM_INDICES2 )
      str[ indices2[ i ] ] = letter;
#endif
    count = count / NUM_LETTERS; // carry
  }

  // hash the string and check for matches
  uint64_t hash = hashlittle2( str );
  uint32_t bucket_index = ( hash & BUCKET_MASK ) * BUCKET_SIZE;
  bool match = false;
  for ( uint32_t i = 0; i < BUCKET_SIZE; i++ )
    match |= ( hash == hashes[ bucket_index + i ] );

  // write the result if a match occurred
  if ( match )
  {
    uint result_index = atomic_inc( num_results );
    if ( result_index < MAX_RESULTS )
      result_id[ result_index ] = get_global_id( 0 );
  }
}
