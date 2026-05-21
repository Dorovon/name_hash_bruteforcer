#include "gpu.h"
#include "hash_string.h"
#include "hashlittle2.h"
#include "sstrhash.h"
#include "kernels.h"
#include "progress_bar.h"
#include "util.h"

#include <array>
#include <chrono>
#include <cstring>
#include <format>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

static unsigned char LETTERS[ 256 ] = { 0 };
static size_t LETTERS_SIZE = 0;
static std::string LISTFILE_DATA = "";
static std::string LISTFILE_PATH = "";
static const char* PROGRAM_NAME = "bruteforcer";
static size_t GPU_BATCH_MAX_RESULTS = 1024;
static size_t GPU_MAX_WORK_SIZE = 2ull << 30;
static std::string PATTERN_PATH = "";
static std::string NAME_HASH_STR = "";
static std::vector<std::string> PATTERNS;
static std::vector<std::vector<unsigned char>> ALPHABETS;
static int NUM_THREADS = 1;
static bool USE_GPU = false;
static bool QUIET = false;
static size_t HASH_TYPE = H_HASHLITTLE2;
static std::vector<std::string> DICTIONARY_FILES;
static std::vector<dictionary_t> DICTIONARIES;

void print_match( std::string_view match, uint32_t file_data_id = 0 )
{
  std::string s;
  if ( file_data_id > 0 )
    s += std::format( "{};", file_data_id );
  s.reserve( s.size() + match.size() );
  for ( size_t i = 0; i < match.size(); i++ )
    s += match[ i ];
  util::print_green( s );
}

void get_combination( hash_string_t& str, std::vector<size_t>& counts, const std::vector<size_t>& indices,
                      const std::vector<size_t>& indices2, const std::vector<size_t>& dictionary_indices,
                      const std::vector<size_t>& dictionary_indices_mirrored )
{
  unsigned char* indices_str = &str[ str.data_size - indices.size() ];
  for ( size_t i = 0; i < indices.size(); i++ )
    indices_str[ i ] = LETTERS[ counts[ i ] ];
  std::vector<std::string_view> words_str; // TODO: If this function ever needs to be faster, don't reallocate this every time this runs
  for ( size_t i = 0; i < dictionary_indices.size(); i++ )
  {
    if ( i < DICTIONARIES.size() )
      words_str.emplace_back( DICTIONARIES[ i ][ counts[ indices.size() + i ] ] );
    else
      words_str.emplace_back( DICTIONARIES[ 0 ][ counts[ indices.size() + i ] ] );
  }
  size_t string_index = 0;
  size_t index_index = 0;
  size_t index2_index = 0;
  size_t dictionary_index = 0;
  size_t dictionary_index_mirrored = 0;
  size_t write_index = 0;
  while ( string_index < str.size )
  {
    if ( index_index < indices.size() && string_index == indices[ index_index ] )
    {
      str[ write_index++ ] = indices_str[ index_index ];
      index_index++;
    }
    else if ( index2_index < indices2.size() && string_index == indices2[ index2_index ] )
    {
      str[ write_index++ ] = indices_str[ index2_index ];
      index2_index++;
    }
    else if ( dictionary_index < dictionary_indices.size() && string_index == dictionary_indices[ dictionary_index ] )
    {
      std::string_view word = words_str[ dictionary_index ];
      for ( size_t j = 0; j < word.size(); j++ )
        str[ write_index++ ] = word[ j ];
      dictionary_index++;
    }
    else if ( dictionary_index_mirrored < dictionary_indices_mirrored.size() && string_index == dictionary_indices_mirrored[ dictionary_index_mirrored ] )
    {
      std::string_view word = words_str[ dictionary_index_mirrored ];
      for ( size_t j = 0; j < word.size(); j++ )
        str[ write_index++ ] = word[ j ];
      dictionary_index_mirrored++;
    }
    else
    {
      str[ write_index++ ] = str.original_str[ string_index ];
    }
    string_index++;
  }
  str.current_size = write_index;
  while ( write_index < str.data_size )
  {
    str[ write_index ] = 0;
    write_index++;
  }
}

bool next_combination( std::vector<size_t>& counts, size_t num_indices, size_t increment = 1 )
{
  counts[ 0 ] += increment;
  for ( size_t i = 0; i < counts.size(); i++ )
  {
    if ( i < num_indices )
    {
      if ( counts[ i ] >= LETTERS_SIZE )
      {
        if ( i + 1 >= counts.size() )
          return false;
        counts[ i + 1 ] += counts[ i ] / LETTERS_SIZE;
        counts[ i ] = counts[ i ] % LETTERS_SIZE;
      }
    }
    else
    {
      size_t d = i - num_indices;
      size_t dictionary_size;
      if ( d < DICTIONARIES.size() )
        dictionary_size = DICTIONARIES[ d ].size();
      else
        dictionary_size = DICTIONARIES[ 0 ].size();
      if ( counts[ i ] >= dictionary_size )
      {
        if ( i + 1 >= counts.size() )
          return false;
        counts[ i + 1 ] += counts[ i ] / dictionary_size;
        counts[ i ] = counts[ i ] % dictionary_size;
      }
    }
  }

  return true;
}

inline uint64_t compute_hash( hash_string_t& str )
{
  switch ( HASH_TYPE )
  {
    case H_HASHLITTLE2:
      return hashlittle2( str );
    case H_SSTRHASH:
      return s_str_hash( str );
    default:
      util::error( "Invalid hash type {}", HASH_TYPE );
      std::exit( 1 );
  }
}

constexpr const unsigned char LETTERS_DEFAULT[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ";
constexpr const unsigned char LETTERS_DIGITS[] = "0123456789";
constexpr const unsigned char LETTERS_LETTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr const unsigned char LETTERS_HEX[] = "0123456789ABCDEF";
constexpr const unsigned char LETTERS_BYTES[] = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
                                                "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
                                                "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
                                                "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F"
                                                "\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F"
                                                "\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F"
                                                "\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F"
                                                "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F"
                                                "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F"
                                                "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F"
                                                "\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF"
                                                "\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF"
                                                "\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF"
                                                "\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF"
                                                "\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF"
                                                "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF";

void set_alphabet( std::vector<unsigned char>& a )
{
  std::memcpy( LETTERS, a.data(), a.size() );
  LETTERS_SIZE = a.size();
}

void set_alphabet( std::string_view str )
{
  if ( str == "default" )
  {
    LETTERS_SIZE = 39;
    std::memcpy( LETTERS, LETTERS_DEFAULT, LETTERS_SIZE );
  }
  else if ( str == "digits" || str == "numbers" )
  {
    LETTERS_SIZE = 10;
    std::memcpy( LETTERS, LETTERS_DIGITS, LETTERS_SIZE );
  }
  else if ( str == "letters" )
  {
    LETTERS_SIZE = 26;
    std::memcpy( LETTERS, LETTERS_LETTERS, LETTERS_SIZE );
  }
  else if ( str == "hex" )
  {
    LETTERS_SIZE = 16;
    std::memcpy( LETTERS, LETTERS_HEX, LETTERS_SIZE );
  }
  else if ( str == "bytes" )
  {
    LETTERS_SIZE = 256;
    std::memcpy( LETTERS, LETTERS_BYTES, LETTERS_SIZE );
  }
  else
  {
    std::memcpy( LETTERS, str.data(), str.size() );
    LETTERS_SIZE = str.size();
    for ( size_t i = 0; i < LETTERS_SIZE; i++ )
      LETTERS[ i ] = util::to_upper( LETTERS[ i ] );
  }
}

std::string usage_message()
{
  return std::format( "Usage: {} <-n name_hash|name_hash_file> "
                      "[-a alphabet] "
                      "[-c cpu_threads] "
                      "[-l listfile] "
                      "[-p pattern] "
                      "[-f pattern_file] "
                      "[-g] "
                      "[-w gpu_batch_work_size] "
                      "[-m gpu_match_buffer] "
                      "[-q] "
                      "[-t] "
                      "[-d dictionary_file] "
                      "[-?]", PROGRAM_NAME );
}

void print_help()
{
  util::print( usage_message() );
  util::print( "\nOPTIONS\n"
               "  -a  use the given alphabet instead of the default\n"
               "      the following names are shortcuts for predefined alphabets\n"
               "        default: \"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- \"\n"
               "        digits:  \"0123456789\"\n"
               "        numbers: \"0123456789\"\n"
               "        letters: \"ABCDEFGHIJKLMNOPQRSTUVWXYZ\"\n"
               "        hex:     \"0123456789ABCDEF\"\n"
               "        bytes:   all 256 bytes\n"
               "  -c  limit the number of computational threads used to the given number when using the CPU\n"
               "      this limit does not include background threads used to manage the workload and results\n"
               "  -l  use the given listfile for modes that need one\n"
               "      this will also filter the given name hash file to ignore names that are already known\n"
               "  -n  compare against the given name hash or\n"
               "      compare against the name hashes in the given file\n"
               "  -p  test the given pattern against all provided name hashes\n"
               "  -f  test the patterns in the given file against all provided name hashes\n"
               "  -g  use the GPU where supported\n"
               "  -w  set the batch work size for the GPU\n"
               "  -m  set the maximum number of matches for a single GPU batch\n"
               "  -q  suppress unnecessary output\n"
               "  -t  calculate a table hash instead\n"
               "  -d  use the given file as a dictionary to replace @ or # characters in the pattern\n"
               "      using this multiple times allows prividing a different files for each wildcard\n"
               "  -?  display this message and exit\n" );
  std::exit( 0 );
}

void exit_usage( std::string_view str = "" )
{
  if ( !str.empty() )
    util::error( str );
  util::print( usage_message() );
  std::exit( 1 );
}

void read_args( int argc, char** argv )
{
  int arg_index = 0;
  auto get_next_arg = [ & ]()
  {
    if ( arg_index >= argc )
      exit_usage( "Error while parsing options" );

    return argv[ arg_index++ ];
  };

  PROGRAM_NAME = get_next_arg();
  const char* current_arg;
  while ( arg_index < argc )
  {
    current_arg = get_next_arg();
    if ( !current_arg || current_arg[ 0 ] != '-' )
      exit_usage( "Error while parsing options");

    if ( !current_arg[ 1 ] || current_arg[ 2 ] != 0 )
      exit_usage( std::format( "Error: Unsupported argument: {}", current_arg ) );

    switch ( util::to_lower( current_arg[ 1 ] ) )
    {
      case 'a':
        set_alphabet( get_next_arg() );
        break;
      case 'c':
      {
        std::string thread_count_str = get_next_arg();
        int thread_count = std::stoul( thread_count_str );
        if ( thread_count > NUM_THREADS )
          exit_usage( std::format( "Error: provided number of threads ({}) was greater than system recommended limit of {}", thread_count, NUM_THREADS ) );
        if ( thread_count <= 0 )
          exit_usage( std::format( "Error: provided number of threads ({}) must be greater than zero", thread_count ) );
        NUM_THREADS = thread_count;
        break;
      }
      case 'l':
        LISTFILE_PATH = get_next_arg();
        break;
      case 'n':
        NAME_HASH_STR = get_next_arg();
        break;
      case 'p':
        PATTERNS.push_back( get_next_arg() );
        break;
      case 'f':
        PATTERN_PATH = get_next_arg();
        break;
      case '?':
        print_help();
        break;
      case 'g':
      {
        USE_GPU = true;
        break;
      }
      case 'w':
      {
        std::string batch_size_str = get_next_arg();
        GPU_MAX_WORK_SIZE = std::stoull( batch_size_str );
        if ( GPU_MAX_WORK_SIZE <= 0 )
          exit_usage( std::format( "Error: GPU batch size ({}) must be greater than zero", GPU_MAX_WORK_SIZE ) );
        break;
      }
      case 'm':
      {
        std::string max_results_str = get_next_arg();
        GPU_BATCH_MAX_RESULTS = std::stoull( max_results_str );
        if ( GPU_BATCH_MAX_RESULTS <= 0 )
          exit_usage( std::format( "Error: GPU max results ({}) must be greater than zero", GPU_BATCH_MAX_RESULTS ) );
        break;
      }
      case 'q':
        QUIET = true;
        break;
      case 't':
        HASH_TYPE = H_SSTRHASH;
        break;
      case 'd':
        DICTIONARY_FILES.push_back( get_next_arg() );
        break;
      default:
        exit_usage( std::format( "Error: Unsupported argument: {}", current_arg ) );
    }
  }

  if ( LETTERS_SIZE == 0 )
    set_alphabet( "default" );

  while ( ALPHABETS.size() < PATTERNS.size() )
    ALPHABETS.emplace_back( std::begin( LETTERS ), std::begin( LETTERS ) + LETTERS_SIZE );

  if ( NAME_HASH_STR.empty() )
    exit_usage();
}

std::unordered_map<uint32_t, std::string_view> read_listfile( const std::string& path )
{
  if ( path.empty() )
    return {};

  std::unordered_map<uint32_t, std::string_view> listfile;
  LISTFILE_DATA = util::read_text_file( path );
  util::read_lines( LISTFILE_DATA, [ & ]( std::string_view line )
  {
    auto splits = util::string_split( line, ";" );
    if ( splits.size() >= 2 )
      listfile[ std::stoul( std::string( splits[ 0 ] ) ) ] = splits[ 1 ];
  } );

  return listfile;
}

void read_pattern_file()
{
  if ( PATTERN_PATH.empty() )
    return;

  std::string pattern_data = util::read_text_file( PATTERN_PATH );
  util::read_lines( pattern_data, [ & ]( std::string_view line )
  {
    if ( !line.empty() && line[ 0 ] != '#' )
    {
      auto splits = util::string_split( line, ";" );
      PATTERNS.push_back( std::string( splits[ 0 ] ) );
      if ( splits.size() == 2 )
        set_alphabet( splits[ 1 ] );
      else
        set_alphabet( "default" );
      ALPHABETS.emplace_back( std::begin( LETTERS ), std::begin( LETTERS ) + LETTERS_SIZE );
    }
  } );
}

std::unordered_map<uint64_t, uint32_t> read_name_hashes( const std::string& hash_str,
                                                         const std::unordered_map<uint32_t, std::string_view>& listfile )
{
  std::unordered_map<uint64_t, uint32_t> name_hashes;

  bool is_hex = std::all_of( hash_str.begin(), hash_str.end(), []( char c ) {
    return ( c >= '0' && c <= '9' ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' );
  });

  if ( is_hex )
  {
    if ( hash_str.size() > 16 )
    {
      util::error( "name hashes cannot exceed 16 characters" );
      std::exit( 1 );
    }

    name_hashes[ std::stoull( hash_str, nullptr, 16 ) ] = 0;
  }
  else
  {
    std::string name_hash_data = util::read_text_file( hash_str );
    util::read_lines( name_hash_data, [ & ]( std::string_view line )
    {
      auto splits = util::string_split( line, ";" );
      if ( splits.size() >= 2 )
      {
        if ( splits[ 1 ].size() > 16 )
        {
          util::error( "name hashes cannot exceed 16 characters" );
          std::exit( 1 );
        }
        uint64_t name_hash = std::stoull( std::string( splits[ 1 ] ), nullptr, 16 );
        uint32_t file_data_id = std::stoul( std::string( splits[ 0 ] ) );
        auto it = listfile.find( file_data_id );
        bool known = false;
        if ( it != listfile.end() )
        {
          hash_string_t filename{ it->second, HASH_TYPE };
          if ( name_hash == compute_hash( filename ) )
            known = true;
        }
        if ( !known )
          name_hashes[ name_hash ] = file_data_id;
      }
    } );
  }

  // name hashes are required, so exit if none were provided
  if ( name_hashes.empty() )
    exit_usage( "Error: at least one name hash must be provided" );

  return name_hashes;
}

void generate_patterns_from_listfile( const std::unordered_map<uint32_t, std::string_view>& listfile )
{
  if ( listfile.empty() )
    exit_usage( "Error: either a listfile or pattern must be provided" );

  std::set<std::string_view, decltype( util::str_lt_ci )*> path_names( util::str_lt_ci );
  std::set<std::string_view, decltype( util::str_lt_ci )*> base_names( util::str_lt_ci );
  size_t num_paths = 0;
  size_t num_base = 0;
  for ( auto& [ file_data_id, name ] : listfile )
  {
    for ( size_t i = name.size() - 1; i > 0; i-- )
    {
      if ( name[ i ] == '/' )
      {
        if ( path_names.insert( name.substr( 0, i ) ).second )
          num_paths++;
        if ( base_names.insert( name.substr( i + 1, name.size() - i - 1 ) ).second )
          num_base++;
        break;
      }
    }
  }

  std::vector<std::string_view> path_names_vec{ path_names.begin(), path_names.end() };
  std::vector<std::string_view> base_names_vec{ base_names.begin(), base_names.end() };

  DICTIONARIES.clear();
  DICTIONARIES.emplace_back( path_names_vec );
  DICTIONARIES.emplace_back( base_names_vec );
  PATTERNS.clear();
  PATTERNS.push_back( "@/@" );
  while ( ALPHABETS.size() < PATTERNS.size() )
    ALPHABETS.emplace_back( std::begin( LETTERS ), std::begin( LETTERS ) + LETTERS_SIZE );
}

struct pattern_bruteforcer_t
{
  const std::unordered_map<uint64_t, uint32_t>& name_hashes;
  progress_bar_t progress_bar;
  static constexpr uint64_t bucket_mask = 0xffff;
  static constexpr size_t num_gpu_buffers = 2;
  size_t bucket_size = 0;
  std::vector<uint64_t> bucket_hashes;
  std::unique_ptr<gpu_pool_t> gpu_pool;
  std::mutex gpu_mutex;

  pattern_bruteforcer_t( const std::unordered_map<uint64_t, uint32_t>& name_hashes ) :
    name_hashes( name_hashes ), progress_bar( 0 ), bucket_hashes(), gpu_pool(), gpu_mutex()
  {
    init_progress_bar();
    init_gpus();
  }

  ~pattern_bruteforcer_t()
  {
    progress_bar.finish();
  }

  void init_progress_bar()
  {
    double total_combinations = 0;
    for ( size_t i = 0; i < PATTERNS.size(); i++ )
    {
      double pattern_combinations = 1;
      size_t size_1 = 0;
      size_t size_2 = 0;
      size_t d_size_1 = 0;
      size_t d_size_2 = 0;
      for ( size_t j = 0; j < PATTERNS[ i ].size(); j++ )
      {
        if ( PATTERNS[ i ][ j ] == '*' )
          size_1++;
        if ( PATTERNS[ i ][ j ] == '%' )
          size_2++;
        if ( PATTERNS[ i ][ j ] == '@' )
          d_size_1++;
        if ( PATTERNS[ i ][ j ] == '#' )
          d_size_2++;
      }
      for ( size_t j = 0; j < size_1 || j < size_2; j++ )
        pattern_combinations *= ALPHABETS[ i ].size();
      for ( size_t j = 0; j < d_size_1 || j < d_size_2; j++ )
      {
        if ( j < DICTIONARIES.size() )
          pattern_combinations *= DICTIONARIES[ j ].size();
        else
          pattern_combinations *= DICTIONARIES[ 0 ].size();
      }
      total_combinations += pattern_combinations;
    }

    // TODO: Handle this in a better way.
    std::destroy_at( &progress_bar );
    std::construct_at( &progress_bar, total_combinations );
  }

  void init_gpus()
  {
    if ( !USE_GPU )
      return;

    gpu_pool = std::make_unique<gpu_pool_t>( num_gpu_buffers );
    size_t bucket_counts[ bucket_mask + 1 ];
    for ( size_t i = 0; i < bucket_mask + 1; i++ )
      bucket_counts[ i ] = 0;
    for ( auto [ hash, _ ] : name_hashes )
      bucket_counts[ hash & bucket_mask ]++;
    for ( auto b : bucket_counts )
    {
      if ( b > bucket_size )
        bucket_size = b;
    }
    bucket_hashes.resize( bucket_size * bucket_mask + 1, 0 );
    for ( auto [ hash, _ ] : name_hashes )
    {
      size_t bucket_index = hash & bucket_mask;
      bucket_hashes[ bucket_size * bucket_index + bucket_counts[ bucket_index ] - 1 ] = hash;
      bucket_counts[ bucket_index ]--;
    }
  }

  void match_pattern( size_t pattern_index )
  {
    const auto& original_pattern = PATTERNS[ pattern_index ];
    set_alphabet( ALPHABETS[ pattern_index ] );
    hash_string_t current_string{ original_pattern, HASH_TYPE, DICTIONARIES };
    std::vector<size_t> indices;
    std::vector<size_t> indices2;
    std::vector<size_t> dictionary_indices;
    std::vector<size_t> dictionary_indices_mirrored;

    for ( size_t i = 0; i < current_string.size; i++ )
    {
      if ( current_string[ i ] == '*' )
        indices.push_back( i );
      if ( current_string[ i ] == '%' )
        indices2.push_back( i );
      if ( current_string[ i ] == '@' )
        dictionary_indices.push_back( i );
      if ( current_string[ i ] == '#' )
        dictionary_indices_mirrored.push_back( i );
    }
    if ( indices2.size() > indices.size() )
      std::swap( indices, indices2 );
    if ( dictionary_indices_mirrored.size() > dictionary_indices.size() )
      std::swap( dictionary_indices, dictionary_indices_mirrored );
    std::vector<size_t> counts( indices.size() + dictionary_indices.size(), 0 );

    if ( counts.size() == 0 )
    {
      auto match = name_hashes.find( compute_hash( current_string ) );
      if ( match != name_hashes.end() )
        print_match( current_string.as_string( original_pattern ), match->second );
      progress_bar.increment( 1 );
      return;
    }

    if ( USE_GPU )
      match_pattern_gpu( original_pattern, current_string, indices, indices2, dictionary_indices, dictionary_indices_mirrored, counts );
    else
      match_pattern_cpu( original_pattern, indices, indices2, dictionary_indices, dictionary_indices_mirrored, counts );
  }

  void match_pattern_cpu( const auto& original_pattern,
                          std::vector<size_t>& indices,
                          std::vector<size_t>& indices2,
                          std::vector<size_t>& dictionary_indices,
                          std::vector<size_t>& dictionary_indices_mirrored,
                          std::vector<size_t>& counts )
  {
    std::vector<std::thread> threads;
    progress_bar.reset_threads();
    for ( int i = 0; i < NUM_THREADS; i++ )
    {
      threads.emplace_back( [ & ]( int thread_index )
      {
        hash_string_t thread_string{ original_pattern, HASH_TYPE, DICTIONARIES };
        std::vector<size_t> thread_counts = counts;
        if ( thread_index > 0 )
        {
          if ( !next_combination( thread_counts, indices.size(), thread_index ) )
          {
            progress_bar.finish_thread();
            return;
          }
        }
        size_t update_count = 0;
        do
        {
          get_combination( thread_string, thread_counts, indices, indices2, dictionary_indices, dictionary_indices_mirrored );
          auto match = name_hashes.find( compute_hash( thread_string ) );
          if ( match != name_hashes.end() )
            print_match( thread_string.as_string( original_pattern ), match->second );
          if ( !QUIET )
          {
            update_count++;
            if ( update_count > 10000 )
            {
              progress_bar.increment( update_count );
              update_count = 0;
            }
          }
        } while ( next_combination( thread_counts, indices.size(), NUM_THREADS ) );
        if ( !QUIET )
          progress_bar.increment( update_count );
        progress_bar.finish_thread();
      }, i );
    }
    if ( !QUIET )
    {
      threads.emplace_back( [ & ]( int )
      {
        using namespace std::chrono_literals;
        while ( !progress_bar.is_finished( NUM_THREADS ) )
        {
          progress_bar.out();
          std::this_thread::sleep_for( 100ms );
        }
      }, NUM_THREADS );
    }
    for ( auto& thread : threads )
      thread.join();
  }

  void match_pattern_gpu( const auto& original_pattern,
                          hash_string_t& current_string,
                          std::vector<size_t>& indices,
                          std::vector<size_t>& indices2,
                          std::vector<size_t>& dictionary_indices,
                          std::vector<size_t>& dictionary_indices_mirrored,
                          std::vector<size_t>& counts )
  {
    enum gpu_pattern_buffers : size_t
    {
      B_INITIAL_COUNTS = 0,
      B_NUM_RESULTS = 1,
      B_RESULTS = 2,
      B_HASHES = 3,
      B_DICTIONARY_WORDS = 4,
      B_WORD_OFFSETS = 5,
      B_WORD_LENGTHS = 6,
      B_DICTIONARY_LENGTHS = 7,
      B_WORD_INDICES = 8,
    };

    // Prepare data
    std::vector<unsigned char> dictionary_words;
    std::vector<uint32_t> word_offsets;
    std::vector<uint16_t> word_lengths;
    if ( DICTIONARIES.empty() )
    {
      dictionary_words.emplace_back();
      word_offsets.emplace_back();
      word_lengths.emplace_back();
    }
    else
    {
      size_t total_chars = 0;
      size_t total_words = 0;
      for ( const auto& d : DICTIONARIES )
      {
        for ( const auto& w : d._words )
        {
          total_chars += w.size();
          total_words++;
        }
      }
      dictionary_words.reserve( total_chars );
      word_offsets.reserve( total_words );
      word_lengths.reserve( total_words );
      for ( auto d : DICTIONARIES )
      {
        for ( const auto& w : d._words )
        {
          word_offsets.push_back( static_cast<uint32_t>( dictionary_words.size() ) );
          word_lengths.push_back( static_cast<uint16_t>( w.size() ) );
          for ( auto c : w )
            dictionary_words.push_back( c );
        }
      }
    }

    // Prepare the source code for the kernel.
    std::string defines;
    defines += std::format( "#define NUM_LETTERS {}\n", LETTERS_SIZE );
    defines += "#define LETTERS {";
    for ( size_t i = 0; i < LETTERS_SIZE; i++ )
    {
      if ( i != 0 )
        defines += ",";
      defines += std::to_string( static_cast<unsigned int>( LETTERS[ i ] ) );
    }
    defines += "}\n";
    defines += "#define STR ";
    for ( size_t i = current_string.offset; i < current_string.size; i++ )
    {
      if ( i != current_string.offset )
        defines += ",";
      defines += std::to_string( static_cast<unsigned int>( current_string[ i ] ) );
    }
    defines += "\n";
    defines += std::format( "#define LEN {}\n", current_string.size - current_string.offset );
    defines += std::format( "#define NUM_INDICES {}\n", indices.size() );
    defines += std::format( "#define NUM_INDICES2 {}\n", indices2.size() );
    defines += "#define INDICES ";
    for ( size_t i = 0; i < indices.size(); i++ )
    {
      if ( i != 0 )
        defines += ",";
      defines += std::to_string( indices[ i ] - current_string.offset );
    }
    defines += "\n";
    defines += "#define INDICES2 ";
    for ( size_t i = 0; i < indices2.size(); i++ )
    {
      if ( i != 0 )
        defines += ",";
      defines += std::to_string( indices2[ i ] - current_string.offset );
    }
    defines += "\n";
    defines += std::format( "#define MAX_LENGTH {}\n", current_string.max_size - current_string.offset );
    if ( HASH_TYPE == H_HASHLITTLE2 )
    {
      std::string str_a;
      std::string str_b;
      std::string str_c;
      str_a += "#define A {";
      str_b += "#define B {";
      str_c += "#define C {";
      for ( size_t i = current_string.offset; i <= current_string.max_size; i++ )
      {
        if ( i != current_string.offset )
        {
          str_a += ",";
          str_b += ",";
          str_c += ",";
        }
        if ( i >= current_string.min_size )
        {
          str_a += std::to_string( current_string.hash_states[ i - current_string.min_size ].a );
          str_b += std::to_string( current_string.hash_states[ i - current_string.min_size ].b );
          str_c += std::to_string( current_string.hash_states[ i - current_string.min_size ].c );
        }
        else
        {
          str_a += "0";
          str_b += "0";
          str_c += "0";
        }
      }
      str_a += "}\n";
      str_b += "}\n";
      str_c += "}\n";
      defines += str_a + str_b + str_c;
    }
    else if ( HASH_TYPE == H_SSTRHASH )
    {
      defines += std::format( "#define A {}\n", current_string.hash_states[ 0 ].a );
      defines += std::format( "#define B {}\n", current_string.hash_states[ 0 ].b );
    }
    defines += std::format( "#define BUCKET_MASK {}\n", bucket_mask );
    defines += std::format( "#define NUM_HASHES {}\n", bucket_hashes.size() );
    defines += std::format( "#define BUCKET_SIZE {}\n", bucket_size );
    defines += std::format( "#define MAX_RESULTS {}\n", GPU_BATCH_MAX_RESULTS );

    defines += std::format( "#define NUM_DICTIONARY_INDICES {}\n", dictionary_indices.size() );
    defines += std::format( "#define DICTIONARY_INDICES " );
    for ( size_t i = 0; i < dictionary_indices.size(); i++ )
    {
      if ( i != 0 )
        defines += ",";
      defines += std::to_string( dictionary_indices[ i ] - current_string.offset );
    }
    defines += "\n";
    defines += std::format( "#define NUM_DICTIONARY_INDICES_MIRRORED {}\n", dictionary_indices_mirrored.size() );
    defines += std::format( "#define DICTIONARY_INDICES_MIRRORED " );
    for ( size_t i = 0; i < dictionary_indices_mirrored.size(); i++ )
    {
      if ( i != 0 )
        defines += ",";
      defines += std::to_string( dictionary_indices_mirrored[ i ] - current_string.offset );
    }
    defines += "\n";
    defines += std::format( "#define NUM_DICTIONARY_SELECTORS {}\n", dictionary_indices.size() );
    defines += std::format( "#define DICTIONARY_SELECTORS " );
    for ( size_t i = 0; i < dictionary_indices.size(); i++ )
    {
      if ( i != 0 )
        defines += ",";
      if ( i < DICTIONARIES.size() )
        defines += std::to_string( i );
      else
        defines += "0";
    }
    defines += "\n";
    defines += std::format( "#define NUM_DICTIONARIES {}\n", DICTIONARIES.size() );
    defines += std::format( "#define DICTIONARY_LENGTHS " );
    for ( size_t i = 0; i < DICTIONARIES.size(); i++ )
    {
      if ( i != 0 )
        defines += ",";
      defines += std::to_string( DICTIONARIES[ i ].size() );
    }
    defines += "\n";
    defines += std::format( "#define DICTIONARY_OFFSETS " );
    size_t current_offset = 0;
    for ( size_t i = 0; i < DICTIONARIES.size(); i++ )
    {
      if ( i != 0 )
        defines += ",";
      defines += std::to_string( current_offset );
      current_offset += DICTIONARIES[ i ].size();
    }
    defines += "\n";

    std::vector<const char*> source;
    source.push_back( defines.c_str() );
    switch ( HASH_TYPE )
    {
      case H_HASHLITTLE2:
        source.push_back( kernels::hashlittle2 );
        break;
      case H_SSTRHASH:
        source.push_back( kernels::sstrhash );
        break;
      default:
        break;
    }

    size_t total_work = 1;
    for ( size_t i = 0; i < indices.size(); i++ )
      total_work *= LETTERS_SIZE;
    for ( size_t i = 0; i < dictionary_indices.size(); i++ )
    {
      if ( i < DICTIONARIES.size() )
        total_work *= DICTIONARIES[ i ].size();
      else
        total_work *= DICTIONARIES[ 0 ].size();
    }
    size_t remaining_work = total_work;

    gpu_pool->init_gpus( source, "bruteforce" );

    // constant buffers used by every device
    gpu_pool->set_fn_init_shared_buffers( [ & ]( gpu_t& gpu )
    {
      gpu.add_constant_arg<uint64_t>( B_HASHES, bucket_hashes.data(), bucket_hashes.size() ); // global const uint64_t* hashes
      gpu.add_constant_arg<unsigned char>( B_DICTIONARY_WORDS, dictionary_words.data(), dictionary_words.size() ); // global const uchar* dictionary_words
      gpu.add_constant_arg<uint32_t>( B_WORD_OFFSETS, word_offsets.data(), word_offsets.size() ); // global const uint32_t* word_offsets
      gpu.add_constant_arg<uint16_t>( B_WORD_LENGTHS, word_lengths.data(), word_lengths.size() ); // global const uint16_t* word_lengths
    });

    // read/write buffers used by each batch
    gpu_pool->set_fn_init_device_buffers( [ & ]( gpu_t& gpu )
    {
      gpu.add_indexed_buffer<size_t>( B_INITIAL_COUNTS, counts.size() ); // global const size_t* initial_counts
      gpu.add_indexed_buffer<cl_uint>( B_NUM_RESULTS, 1 ); // global uint* num_results
      gpu.add_indexed_buffer<size_t>( B_RESULTS, GPU_BATCH_MAX_RESULTS ); // global size_t* result_id
    });

    constexpr cl_uint zero = 0;
    gpu_pool->set_fn_prepare_batch( [ & ]( gpu_t& gpu )
    {
      std::lock_guard<std::mutex> guard( gpu_mutex );
      gpu.write_indexed_buffer( B_INITIAL_COUNTS, counts.data() );
      gpu.write_indexed_buffer( B_NUM_RESULTS, &zero );
      gpu.set_arg( B_INITIAL_COUNTS );
      gpu.set_arg( B_NUM_RESULTS );
      gpu.set_arg( B_RESULTS );
      size_t this_work_size = ( remaining_work < GPU_MAX_WORK_SIZE ) ? remaining_work : GPU_MAX_WORK_SIZE;
      remaining_work -= this_work_size;
      gpu.set_next_work_size( this_work_size );
      return next_combination( counts, indices.size(), GPU_MAX_WORK_SIZE );
    });

    gpu_pool->set_fn_execute_batch( [ & ]( gpu_t& gpu )
    {
      gpu.execute();
      gpu.read_buffer( B_NUM_RESULTS );
      gpu.read_buffer( B_RESULTS );
    });

    gpu_pool->set_fn_batch_results( [ & ]( gpu_t& gpu )
    {
      std::lock_guard<std::mutex> guard( gpu_mutex );
      std::vector<size_t> temp_counts = counts;
      cl_uint num_results = gpu.get_host_buffer<cl_uint>( B_NUM_RESULTS )[ 0 ];
      const size_t* results = gpu.get_host_buffer<size_t>( B_RESULTS );
      const size_t* batch_counts = gpu.get_host_buffer<size_t>( B_INITIAL_COUNTS );
      if ( num_results > GPU_BATCH_MAX_RESULTS )
        util::error( "Warning: GPU batch exceeded the maximum number of allowed results ({} > {}). Consider using the \"-m\" option to see them all.", num_results, GPU_BATCH_MAX_RESULTS );
      for ( size_t i = 0; i < num_results && i < GPU_BATCH_MAX_RESULTS; i++ )
      {
        for ( size_t c = 0; c < counts.size(); c++ )
          temp_counts[ c ] = batch_counts[ c ];
        if ( next_combination( temp_counts, indices.size(), results[ i ] ) )
        {
          get_combination( current_string, temp_counts, indices, indices2, dictionary_indices, dictionary_indices_mirrored );
          uint64_t hash = compute_hash( current_string );
          auto match = name_hashes.find( hash );
          if ( match != name_hashes.end() )
            print_match( current_string.as_string( original_pattern ), match->second );
          else if ( hash != 0 ) // zeros are used to fill in the buckets, so this isn't a false positive worth warning about
            util::error( "Error: GPU result did not match on CPU ({}): {}", results[ i ], current_string.as_string() );
        }
        else
        {
          util::error( "Error: GPU result is out of range ({})", results[ i ] );
        }
      }
      if ( !QUIET )
      {
        progress_bar.increment( gpu.get_last_work_size() );
        progress_bar.out();
      }
    });

    gpu_pool->execute();
  }
};

void match_patterns( const std::unordered_map<uint64_t, uint32_t>& name_hashes )
{
  pattern_bruteforcer_t pattern_bruteforcer( name_hashes );
  for ( size_t pattern_index = 0; pattern_index < PATTERNS.size(); pattern_index++ )
    pattern_bruteforcer.match_pattern( pattern_index );
}

int main( int argc, char** argv )
{
  try
  {
    NUM_THREADS = std::thread::hardware_concurrency();
    read_args( argc, argv );
    std::unordered_map<uint32_t, std::string_view> listfile = read_listfile( LISTFILE_PATH );
    read_pattern_file();
    std::unordered_map<uint64_t, uint32_t> name_hashes = read_name_hashes( NAME_HASH_STR, listfile );
    for ( const auto& p : DICTIONARY_FILES )
      DICTIONARIES.emplace_back( p );

    if ( PATTERNS.empty() )
      generate_patterns_from_listfile( listfile );

    if ( !PATTERNS.empty() && DICTIONARIES.empty() )
    {
      for ( const auto& p : PATTERNS )
      {
        for ( auto c : p )
        {
          if ( c == '@' || c == '#' )
          {
            exit_usage( "At least one dictionary must be provided for patterns that contain @ or #" );
          }
        }
      }
    }

    match_patterns( name_hashes );
  }
  catch ( std::exception& e )
  {
    util::error( e.what() );
    std::exit( 1 );
  }

  return 0;
}
