// Necessary Defines
// NUM_LETTERS: length of the alphabet to use
// LETTERS: the alphabet to use
// STR: the initial bytes of the string fragment that will be hashed
//      each global id will permute this to a different value
// LEN: length of the string to hash
// NUM_INDICES: the number of wildcards that need to be permuted over in the string
// NUM_INDICES2: the number of wildcards that mirror the first set in the string
// INDICES: the string offset for each wildcard
// INDICES2: the string offset for each wildcard that mirrors the first set
// NUM_HASHES: the size of the bucketed array of hashes
// BUCKET_SIZE: the number of hashes in each bucket
// MAX_RESULTS: the maximum number of results that are allowed

typedef uint uint32_t;
typedef ulong uint64_t;

constant unsigned char letters[ NUM_LETTERS ] = LETTERS;

#if NUM_INDICES > 0
constant size_t indices[ NUM_INDICES ] = { INDICES };
#else
constant size_t indices[ 1 ] = { 0 };
#endif

#if NUM_INDICES2 > 0
constant size_t indices2[ NUM_INDICES2 ] = { INDICES2 };
#else
constant size_t indices2[ 1 ] = { 0 };
#endif

constant uint32_t const s_hashtable[ 16 ] = {
  0x486e26ee, 0xdcaa16b3, 0xe1918eef, 0x202dafdb,
  0x341c7dc7, 0x1c365303, 0x40ef2d37, 0x65fd5e49,
  0xd6057177, 0x904ece93, 0x1c38024f, 0x98fd323b,
  0xe3061ae7, 0xa39b0fa1, 0x9797f25f, 0xe4444563,
};

inline uint32_t s_str_hash( unsigned char* str )
{
  uint32_t seed = A;
  uint32_t shift = B;

  for ( short i = 0; i < LEN; i++ )
  {
    seed = ( s_hashtable[ str[ i ] >> 4 ] - s_hashtable[ str[ i ] & 0xf ] ) ^ ( shift + seed );
    shift = str[ i ] + seed + 33 * shift + 3;
  }

  return seed ? seed : 1;
}


kernel void bruteforce( global size_t* initial_counts, global uint* num_results, global size_t* result_id, global uint64_t* hashes )
{
  size_t id = get_global_id( 0 );

  // initialize the string
  unsigned char str[ LEN ] = { STR };
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
  uint64_t hash = s_str_hash( str );
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
