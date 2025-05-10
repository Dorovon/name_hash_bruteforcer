// Necessary Defines
// NUM_LETTERS: length of the alphabet to use
// LETTERS: the alphabet to use
// STR: the initial bytes of the string fragment that will be hashed
//      each global id will permute this to a different value
// LEN: length of the string for hashlittle2 to compute up to the start of the final block
//      the real length o the string will be LEN + 12 including the final block
// NUM_INDICES: the number of wildcards that need to be permuted over in the string
// NUM_INDICES2: the number of wildcards that mirror the first set in the string
// INDICES: the string offset for each wildcard
// INDICES2: the string offset for each wildcard that mirrors the first set
// A, B, C: initial state for hashlittle2
// NUM_HASHES: the size of the bucketed array of hashes
// BUCKET_SIZE: the number of hashes in each bucket
// MAX_RESULTS: the maximum number of results that are allowed

typedef uint uint32_t;
typedef ulong uint64_t;

constant unsigned char letters[ NUM_LETTERS ] = { LETTERS };
constant size_t indices[ NUM_INDICES ] = { INDICES };
constant size_t indices2[ NUM_INDICES2 ] = { INDICES2 };

inline uint32_t rotate_left( uint32_t value, size_t distance )
{
  return ( value << distance ) | ( value >> ( 32 - distance ) );
}

inline uint64_t hashlittle2( unsigned char* k )
{
  uint32_t a = A;
  uint32_t b = B;
  uint32_t c = C;

  for ( short i = 0; i < LEN; i += 12 )
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

  a += k[ LEN + 0 ] + ( ( uint32_t )( k[ LEN + 1 ] ) << 8 ) + ( ( uint32_t )( k[ LEN +  2 ] ) << 16 ) + ( ( uint32_t )( k[ LEN +  3 ] ) << 24 );
  b += k[ LEN + 4 ] + ( ( uint32_t )( k[ LEN + 5 ] ) << 8 ) + ( ( uint32_t )( k[ LEN +  6 ] ) << 16 ) + ( ( uint32_t )( k[ LEN +  7 ] ) << 24 );
  c += k[ LEN + 8 ] + ( ( uint32_t )( k[ LEN + 9 ] ) << 8 ) + ( ( uint32_t )( k[ LEN + 10 ] ) << 16 ) + ( ( uint32_t )( k[ LEN + 11 ] ) << 24 );

  c ^= b; c -= rotate_left( b, 14 );
  a ^= c; a -= rotate_left( c, 11 );
  b ^= a; b -= rotate_left( a, 25 );
  c ^= b; c -= rotate_left( b, 16 );
  a ^= c; a -= rotate_left( c,  4 );
  b ^= a; b -= rotate_left( a, 14 );
  c ^= b; c -= rotate_left( b, 24 );

  return ( ( uint64_t )( c ) << 32 ) | b;
}

kernel void bruteforce( global size_t* initial_counts, global uint* num_results, global size_t* result_id, global uint64_t* hashes )
{
  size_t id = get_global_id( 0 );

  // initialize the string
  unsigned char str[ LEN + 12 ] = { STR };
  size_t count = id;
  unsigned char letter;
  for ( size_t i = 0; i < NUM_INDICES; i++ )
  {
    count += initial_counts[ i ];
    letter = letters[ count % NUM_LETTERS ];
    str[ indices[ i ] ] = letter;
    if ( i < NUM_INDICES2 ) // TODO: Test getting rid of this branch by having indices2 padded with an index past the end of the string
      str[ indices2[ i ] ] = letter;
    count = count / NUM_LETTERS; // carry
  }

  // hash the string and check for matches
  uint64_t hash = hashlittle2( str );
  size_t bucket_index = ( hash & BUCKET_MASK ) * BUCKET_SIZE;
  bool match = false;
  for ( size_t i = 0; i < BUCKET_SIZE; i++ )
    match |= ( hash == hashes[ bucket_index + i ] );

  // write the result if a match occurred
  if ( match )
  {
    uint result_index = atomic_inc( num_results );
    if ( result_index < MAX_RESULTS )
      result_id[ result_index ] = id;
  }
}
