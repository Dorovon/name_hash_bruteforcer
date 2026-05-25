typedef uchar uint8_t;
typedef ushort uint16_t;
typedef uint uint32_t;
typedef ulong uint64_t;

constant uchar letters[ NUM_LETTERS ] = LETTERS;
#if NUM_DICTIONARY_INDICES
constant uchar base_str[ LEN ] = { STR };
#endif
constant uint32_t state_a[ MAX_LENGTH - MIN_LENGTH + 1 ] = { A };
constant uint32_t state_b[ MAX_LENGTH - MIN_LENGTH + 1 ] = { B };
constant uint32_t state_c[ MAX_LENGTH - MIN_LENGTH + 1 ] = { C };
#if NUM_DICTIONARIES
constant uint32_t dictionary_lengths[ NUM_DICTIONARIES ] = { DICTIONARY_LENGTHS };
constant uint32_t dictionary_offsets[ NUM_DICTIONARIES ] = { DICTIONARY_OFFSETS };
#endif

#if NUM_INDICES
constant uint16_t indices[ NUM_INDICES ] = { INDICES };
#endif

#if NUM_INDICES2
constant uint16_t indices2[ NUM_INDICES2 ] = { INDICES2 };
#endif

#if NUM_DICTIONARY_INDICES
constant uint16_t dictionary_indices[ NUM_DICTIONARY_INDICES ] = { DICTIONARY_INDICES };
#endif

#if NUM_DICTIONARY_INDICES_MIRRORED > 0
constant uint16_t dictionary_indices_mirrored[ NUM_DICTIONARY_INDICES_MIRRORED ] = { DICTIONARY_INDICES_MIRRORED };
#endif

#if NUM_DICTIONARY_SELECTORS > 0
constant uint8_t dictionary_selectors[ NUM_DICTIONARY_SELECTORS ] = { DICTIONARY_SELECTORS };
#endif

inline uint32_t rotate_left( uint32_t value, uint8_t distance )
{
  return ( value << distance ) | ( value >> ( 32 - distance ) );
}

inline uint64_t hashlittle2( uchar* k, uint16_t length, uint16_t state_index )
{
  uint32_t a = state_a[ state_index ];
  uint32_t b = state_b[ state_index ];
  uint32_t c = state_c[ state_index ];

  for ( short i = 0; i < length; i += 12 )
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

  a += k[ length + 0 ] + ( ( uint32_t )( k[ length + 1 ] ) << 8 ) + ( ( uint32_t )( k[ length +  2 ] ) << 16 ) + ( ( uint32_t )( k[ length +  3 ] ) << 24 );
  b += k[ length + 4 ] + ( ( uint32_t )( k[ length + 5 ] ) << 8 ) + ( ( uint32_t )( k[ length +  6 ] ) << 16 ) + ( ( uint32_t )( k[ length +  7 ] ) << 24 );
  c += k[ length + 8 ] + ( ( uint32_t )( k[ length + 9 ] ) << 8 ) + ( ( uint32_t )( k[ length + 10 ] ) << 16 ) + ( ( uint32_t )( k[ length + 11 ] ) << 24 );

  c ^= b; c -= rotate_left( b, 14 );
  a ^= c; a -= rotate_left( c, 11 );
  b ^= a; b -= rotate_left( a, 25 );
  c ^= b; c -= rotate_left( b, 16 );
  a ^= c; a -= rotate_left( c,  4 );
  b ^= a; b -= rotate_left( a, 14 );
  c ^= b; c -= rotate_left( b, 24 );

  return ( ( uint64_t )( c ) << 32 ) | b;
}

#if NUM_DICTIONARY_INDICES
kernel void bruteforce( global const size_t* initial_counts, global uint* num_results, global size_t* result_id, global const uint64_t* hashes,
                        global const uchar* dictionary_words, global const uint32_t* word_offsets, global const uint16_t* word_lengths )
#else
kernel void bruteforce( global const size_t* initial_counts, global uint* num_results, global size_t* result_id, global const uint64_t* hashes )
#endif
{
  size_t count = get_global_id( 0 );
#if NUM_INDICES
  uchar index_char[ NUM_INDICES ];
  for ( uint16_t i = 0; i < NUM_INDICES; i++ )
  {
    count += initial_counts[ i ];
    index_char[ i ] = letters[ count % NUM_LETTERS ];
    count = count / NUM_LETTERS; // carry
  }
#endif
#if NUM_DICTIONARY_INDICES
  uint32_t word_indices[ NUM_DICTIONARY_INDICES ];
  for ( uint16_t i = 0; i < NUM_DICTIONARY_INDICES; i++ )
  {
    count += initial_counts[ NUM_INDICES + i ];
    uint8_t d = dictionary_selectors[ i ];
    word_indices[ i ] = ( count % dictionary_lengths[ d ] ) + dictionary_offsets[ d ];
    count = count / dictionary_lengths[ d ]; // carry
  }
#endif

  // write the string for the current combination
  uint16_t string_index = 0;
#if NUM_INDICES
  uint16_t index_index = 0;
#endif
#if NUM_INDICES2
  uint16_t index2_index = 0;
#endif
#if NUM_DICTIONARY_INDICES
  uint16_t dictionary_index = 0;
#endif
#if NUM_DICTIONARY_INDICES_MIRRORED
  uint16_t dictionary_index_mirrored = 0;
#endif
  uint16_t write_index = 0;
#if NUM_DICTIONARY_INDICES
  uchar new_str[ DATA_LENGTH ];
#else
  uchar new_str[ DATA_LENGTH ] = { STR };
#endif
  while ( string_index < LEN )
  {
#if NUM_INDICES
    if ( index_index < NUM_INDICES && string_index == indices[ index_index ] )
    {
      new_str[ write_index++ ] = index_char[ index_index ];
      index_index++;
    } else
#endif
#if NUM_INDICES2
    if ( index2_index < NUM_INDICES2 && string_index == indices2[ index2_index ] )
    {
      new_str[ write_index++ ] = index_char[ index2_index ];
      index2_index++;
    } else
#endif
#if NUM_DICTIONARY_INDICES
    if ( dictionary_index < NUM_DICTIONARY_INDICES && string_index == dictionary_indices[ dictionary_index ] )
    {
      uint32_t w = word_indices[ dictionary_index ];
      uint32_t o = word_offsets[ w ];
      uint32_t l = word_lengths[ w ];
      for ( uint16_t j = 0; j < l; j++ )
        new_str[ write_index++ ] = dictionary_words[ o + j ];
      dictionary_index++;
    } else
#endif
#if NUM_DICTIONARY_INDICES_MIRRORED
    if ( dictionary_index_mirrored < NUM_DICTIONARY_INDICES_MIRRORED && string_index == dictionary_indices_mirrored[ dictionary_index_mirrored ] )
    {
      uint32_t w = word_indices[ dictionary_index_mirrored ];
      uint32_t o = word_offsets[ w ];
      uint32_t l = word_lengths[ w ];
      for ( uint16_t j = 0; j < l; j++ )
        new_str[ write_index++ ] = dictionary_words[ o + j ];
      dictionary_index_mirrored++;
    } else
#endif
    {
#if NUM_DICTIONARY_INDICES
      new_str[ write_index++ ] = base_str[ string_index ];
#else
      write_index++;
#endif
    }
    string_index++;
  }
  uint16_t state_index = write_index - MIN_LENGTH;
  while ( write_index % 12 != 0 )
    new_str[ write_index++ ] = 0;

  // hash the string and check for matches
  uint64_t hash = hashlittle2( new_str, write_index - 12, state_index );
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
