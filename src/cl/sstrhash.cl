typedef uchar uint8_t;
typedef ushort uint16_t;
typedef uint uint32_t;
typedef ulong uint64_t;

constant uchar letters[ NUM_LETTERS ] = LETTERS;
constant uchar base_str[ LEN ] = { STR };
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

constant uint32_t const s_hashtable[ 16 ] = {
  0x486e26ee, 0xdcaa16b3, 0xe1918eef, 0x202dafdb,
  0x341c7dc7, 0x1c365303, 0x40ef2d37, 0x65fd5e49,
  0xd6057177, 0x904ece93, 0x1c38024f, 0x98fd323b,
  0xe3061ae7, 0xa39b0fa1, 0x9797f25f, 0xe4444563,
};

inline uint32_t s_str_hash( unsigned char* str, uint16_t length )
{
  uint32_t seed = A;
  uint32_t shift = B;

  for ( uint16_t i = 0; i < length; i++ )
  {
    seed = ( s_hashtable[ str[ i ] >> 4 ] - s_hashtable[ str[ i ] & 0xf ] ) ^ ( shift + seed );
    shift = str[ i ] + seed + 33 * shift + 3;
  }

  return seed ? seed : 1;
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
  uchar new_str[ MAX_LENGTH ];
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
      new_str[ write_index++ ] = base_str[ string_index ];
    }
    string_index++;
  }

  // hash the string and check for matches
  uint64_t hash = s_str_hash( new_str, write_index );
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
