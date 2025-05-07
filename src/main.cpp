#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static std::mutex cout_mutex;
static std::string LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ";
static size_t LETTERS_SIZE = LETTERS.size();

char to_upper( char c )
{
  if ( c >= 'a' && c <= 'z' )
    return c & 0xdf;

  if ( c == '/' )
    return '\\';

  return c;
}

char to_lower( char c )
{
  if ( c >= 'A' && c <= 'Z' )
    return c | 0x20;

  if ( c == '\\' )
    return '/';

  return c;
}

void to_upper( std::string& str )
{
  for ( size_t i = 0; i < str.size(); i++ )
    str[ i ] = to_upper( str[ i ] );
}

// comparison function for case-insensitive strings
bool str_lt_ci( std::string_view first, std::string_view second )
{
  size_t min_size = first.size() < second.size() ? first.size() : second.size();

  for ( size_t i = 0; i < min_size; i++ )
  {
    if ( to_lower( first [ i ] ) < to_lower( second [ i ] ) )
      return true;
    if ( to_lower( first [ i ] ) > to_lower( second [ i ] ) )
      return false;
  }

  return first.size() < second.size();
}

struct hash_string_t
{
  // length of the string
  size_t size;

  // string data padded to mod 12 for simplified hashing
  std::shared_ptr<unsigned char[]> _data;

  // state for partially completed hash
  size_t offset;
  uint32_t a;
  uint32_t b;
  uint32_t c;

  hash_string_t() :
    size(), _data(), offset(), a(), b(), c()
  {}

  hash_string_t( std::string_view str ) :
    size( str.size() ), offset(), a(), b(), c()
  {
    size_t data_size = str.size() + 12 - str.size() % 12;
    _data = std::make_shared<unsigned char[]>( data_size );
    for ( size_t i = 0; i < str.size(); i++ )
      _data.get()[ i ] = to_upper( str[ i ] );
    for ( size_t i = str.size(); i < data_size; i++ )
      _data.get()[ i ] = 0;
    initialize();
  }

  hash_string_t& operator=( std::string_view str )
  {
    *this = hash_string_t{ str };
    return *this;
  }

  unsigned char& operator[]( const size_t index )
  {
    return _data.get()[ index ];
  }

  const unsigned char& operator[]( const size_t index ) const
  {
    return _data.get()[ index ];
  }

  const char* as_string() const
  {
    return reinterpret_cast<const char*>( _data.get() );
  }

  const unsigned char* data() const
  {
    return _data.get();
  }

  void initialize();
};

uint32_t rotate_left( uint32_t value, size_t distance )
{
  return ( value << distance ) | ( value >> ( 32 - distance ) );
}

uint64_t hashlittle2( hash_string_t& str, size_t length = 0, bool save_state = false )
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

void hash_string_t::initialize()
{
  size_t i;
  for ( i = 0; i < size - 1; i++ )
  {
    if ( ( *this )[ i + 1 ] == '*' || ( *this )[ i + 1 ] == '%' )
      break;
  }
  size_t this_size = i - i % 12;
  hashlittle2( *this, this_size, true );
  offset = this_size;
}

std::vector<std::string_view> string_split( std::string_view str, std::string_view delim )
{
  std::vector<std::string_view> splits;
  size_t start = 0;

  for ( size_t i = 0; i < str.size() - delim.size(); i++ )
  {
    if ( str.substr( i, delim.size() ) == delim )
    {
      if ( i > start )
      {
        splits.emplace_back( str.substr( start, i - start ) );
      }
      i += delim.size();
      start = i;
    }
  }

  if ( start < str.size() )
  {
    splits.emplace_back( str.substr( start, str.size() - start ) );
  }

  return splits;
}

void print_match( std::string_view original_pattern, std::string_view match, uint32_t file_data_id = 0 )
{
  std::lock_guard<std::mutex> guard( cout_mutex );
  if ( file_data_id > 0 )
    std::cout << file_data_id << ';';
  for ( size_t i = 0; i < match.size(); i++ )
  {
    if ( i < original_pattern.size() && original_pattern[ i ] != '*' && original_pattern[ i ] != '%' )
      std::cout << original_pattern[ i ];
    else
      std::cout << to_lower( match[ i ] );
  }
  std::cout << std::endl;
}

void get_combination( hash_string_t& str, std::vector<size_t>& counts, const std::vector<size_t>& indices, const std::vector<size_t>& indices2 )
{
  for ( size_t i = 0; i < counts.size(); i++ )
  {
    str[ indices[ i ] ] = LETTERS[ counts[ i ] ];
    if ( i < indices2.size() )
      str[ indices2[ i ] ] = LETTERS[ counts[ i ] ];
  }
}

bool next_combination( std::vector<size_t>& counts, size_t increment = 1 )
{
  counts[ 0 ] += increment;
  for ( size_t i = 0; i < counts.size(); i++ )
  {
    if ( counts[ i ] >= LETTERS_SIZE )
    {
      if ( i + 1 >= counts.size() )
        return false;
      counts[ i + 1 ] += counts[ i ] / LETTERS_SIZE;
      counts[ i ] = counts[ i ] % LETTERS_SIZE;
    }
  }

  return true;
}

std::string read_text_file( std::string& path )
{
  std::ifstream file( path, std::ios::binary );
  if ( !file.good() )
  {
    std::cerr << std::format( "Error opening file: {}", path ) << std::endl;
    std::exit( 1 );
  }

  // get file size
  file.unsetf( std::ios::skipws );
  file.seekg( 0, std::ios::end );
  size_t size = file.tellg();
  file.seekg( 0, std::ios::beg );

  // load the entire file into a single string block
  std::string file_data;
  file_data.resize( size );
  file.read( file_data.data(), size );
  return file_data;
}

void set_alphabet( std::string_view str )
{
  LETTERS = str;
  LETTERS_SIZE = LETTERS.size();
  to_upper( LETTERS );
}

std::string usage_message( const char* name )
{
  return std::format( "Usage: {} <-n name_hash|name_hash_file> [-a alphabet] [-c cpu_threads] [-l listfile] [-p pattern] [-f pattern_file] [-?]", name );
}

void print_help( const char* name )
{
  std::cout << usage_message( name )
            << "\n\nOPTIONS\n"
            << "  -a  use the given alphabet instead of the default\n"
            << "  -c  limit the number of threads used to the given number\n"
            << "  -l  use the given listfile for modes that need one\n"
            << "      this will also filter the given name hash file to ignore names that are already known\n"
            << "  -n  compare against the given name hash or\n"
            << "      compare against the name hashes in the given file\n"
            << "  -p  test the given pattern against all provided name hashes\n"
            << "  -f  test the patterns in the given file against all provided name hashes\n"
            << "  -?  display this message and exit\n"
            << std::endl;
  std::exit( 0 );
}

int main( int argc, char** argv )
{
  auto exit_usage = [ argv ]( std::string_view str = "" )
  {
    if ( !str.empty() )
      std::cerr << str << std::endl;
    std::cerr << usage_message( argv[ 0 ] ) << std::endl;
    std::exit( 1 );
  };

  int arg_index = 1;
  auto get_next_arg = [ & ]()
  {
    if ( arg_index >= argc )
      exit_usage();

    return argv[ arg_index++ ];
  };

  std::string listfile_path = "";
  std::string pattern_path = "";
  std::string name_hash_str = "";
  std::vector<std::string> patterns;
  std::vector<std::string> alphabets;
  int NUM_THREADS = std::thread::hardware_concurrency();
  const char* current_arg;
  while ( arg_index < argc )
  {
    current_arg = get_next_arg();
    if ( !current_arg || current_arg[ 0 ] != '-' )
      exit_usage();

    if ( !current_arg[ 1 ] || current_arg[ 2 ] != 0 )
      exit_usage( std::format( "Unsupported argument: {}", current_arg ) );

    switch ( to_lower( current_arg[ 1 ] ) )
    {
      case 'a':
        set_alphabet( get_next_arg() );
        break;
      case 'c':
      {
        std::string thread_count_str = get_next_arg();
        int thread_count = std::stoul( thread_count_str );
        if ( thread_count > NUM_THREADS )
          exit_usage( std::format( "provided number of threads ({}) was greater than system recommended limit of {}", thread_count, NUM_THREADS ) );
        if ( thread_count <= 0 )
          exit_usage( std::format( "provided number of threads ({}) must be greater than zero", thread_count ) );
        NUM_THREADS = thread_count;
        break;
      }
      case 'l':
        listfile_path = get_next_arg();
        break;
      case 'n':
        name_hash_str = get_next_arg();
        break;
      case 'p':
        patterns.push_back( get_next_arg() );
        break;
      case 'f':
        pattern_path = get_next_arg();
        break;
      case '?':
        print_help( argv[ 0 ] );
        break;
      default:
        exit_usage( std::format( "Unsupported argument: {}", current_arg ) );
    }
  }

  while ( alphabets.size() < patterns.size() )
    alphabets.push_back( LETTERS );

  if ( name_hash_str.empty() )
    exit_usage();

  // read the listfile if given
  std::string listfile_data;
  std::unordered_map<uint32_t, std::string_view> listfile;
  if ( !listfile_path.empty() )
  {
    listfile_data = read_text_file( listfile_path );
    size_t line_start = 0;
    std::string_view line;
    for ( size_t i = 0; i < listfile_data.size(); i++ )
    {
      if ( listfile_data[ i ] == '\n' || listfile_data[ i ] == '\r' )
      {
        if ( line_start < i )
        {
          line = std::string_view( listfile_data ).substr( line_start, i - line_start );
          auto splits = string_split( line, ";" );
          if ( splits.size() >= 2 )
            listfile[ std::stoul( std::string( splits[ 0 ] ) ) ] = splits[ 1 ];
        }
        line_start = i + 1;
      }
    }
  }

  // read patterns if given
  if ( !pattern_path.empty() )
  {
    std::string pattern_data = read_text_file( pattern_path );
    size_t line_start = 0;
    std::string_view line;
    for ( size_t i = 0; i < pattern_data.size(); i++ )
    {
      if ( pattern_data[ i ] == '\n' || pattern_data[ i ] == '\r' )
      {
        if ( line_start < i )
        {
          line = std::string_view( pattern_data ).substr( line_start, i - line_start );
          if ( !line.empty() && line[ 0 ] != '#' )
          {
            auto splits = string_split( line, ";" );
            patterns.push_back( std::string( splits[ 0 ] ) );
            if ( splits.size() == 2 )
              alphabets.push_back( std::string( splits[ 1 ] ) );
            else
              alphabets.push_back( LETTERS );
          }
        }
        line_start = i + 1;
      }
    }
  }

  // read the name hashes
  std::unordered_map<uint64_t, uint32_t> name_hashes;
  try
  {
    name_hashes[ std::stoull( name_hash_str, nullptr, 16 ) ] = 0;
  }
  catch ( std::exception& )
  {
    std::ifstream file( name_hash_str );
    if ( !file.good() )
    {
      std::cerr << std::format( "Error opening file: {}", name_hash_str ) << std::endl;
      return 1;
    }

    std::string line;
    while ( std::getline( file, line ) )
    {
      auto splits = string_split( line, ";" );
      if ( splits.size() >= 2 )
      {
        uint64_t name_hash = std::stoull( std::string( splits[ 1 ] ), nullptr, 16 );
        uint32_t file_data_id = std::stoul( std::string( splits[ 0 ] ) );
        auto it = listfile.find( file_data_id );
        bool known = false;
        if ( it != listfile.end() )
        {
          hash_string_t filename{ it->second };
          if ( name_hash == hashlittle2( filename ) )
            known = true;
        }

        if ( !known )
          name_hashes[ name_hash ] = file_data_id;
      }
    }
  }

  // name hashes are required, so exit if none were provided
  if ( name_hashes.empty() )
    exit_usage( "at least one name hash must be provided" );

  if ( patterns.empty() )
  {
    // Look for matches using names from the listfile
    if ( listfile.empty() )
      exit_usage( "either a listfile or pattern must be provided" );

    std::set<std::string_view, decltype( str_lt_ci )*> path_names( str_lt_ci );
    std::set<std::string_view, decltype( str_lt_ci )*> base_names( str_lt_ci );
    for ( auto& [ file_data_id, name ] : listfile )
    {
      // check if preprending the hash with some specific directories finds anything
      for ( auto prefix : { "Data/", "Alternate/", "Test/" } )
      {
        hash_string_t data_name{ prefix + std::string( name ) };
        auto match = name_hashes.find( hashlittle2( data_name ) );
        if ( match != name_hashes.end() )
          print_match( prefix + std::string( name ), data_name.as_string(), match->second );
      }

      for ( size_t i = name.size() - 1; i > 0; i-- )
      {
        if ( name[ i ] == '/' )
        {
          path_names.insert( name.substr( 0, i ) );
          base_names.insert( name.substr( i + 1, name.size() - i - 1 ) );
          break;
        }
      }
    }

    std::vector<std::string_view> base_names_vec{ base_names.begin(), base_names.end() };
    std::vector<std::thread> threads;
    for ( int i = 0; i < NUM_THREADS; i++ )
    {
      threads.emplace_back( [&]( int thread_index )
      {
        for ( size_t b = thread_index; b < base_names_vec.size(); b += NUM_THREADS )
        {
          std::string current_name;
          hash_string_t hash_name;
          for ( auto path : path_names )
          {
            current_name = path;
            current_name += "/";
            current_name += base_names_vec[ b ];
            hash_name = current_name;
            auto match = name_hashes.find( hashlittle2( hash_name ) );
            if ( match != name_hashes.end() )
              print_match( current_name, hash_name.as_string(), match->second );
          }
        }
      }, i );
    }
    for ( auto& thread : threads )
      thread.join();
  }
  else
  {
    // look for matches using the provided pattern(s)
    for ( size_t pattern_index = 0; pattern_index < patterns.size(); pattern_index++ )
    {
      const auto& original_pattern = patterns[ pattern_index ];
      set_alphabet( alphabets[ pattern_index ] );
      hash_string_t current_string{ original_pattern };
      std::vector<size_t> indices;
      std::vector<size_t> indices2;
      std::vector<std::thread> threads;
      for ( size_t i = 0; i < current_string.size; i++ )
      {
        if ( current_string[ i ] == '*' )
          indices.push_back( i );
        if ( current_string[ i ] == '%' )
          indices2.push_back( i );
      }
      if ( indices2.size() > indices.size() )
        std::swap( indices, indices2 );

      std::vector<size_t> counts( indices.size(), 0 );
      if ( counts.size() == 0 )
      {
        auto match = name_hashes.find( hashlittle2( current_string ) );
        if ( match != name_hashes.end() )
          print_match( original_pattern, current_string.as_string(), match->second );
      }
      else
      {
        for ( int i = 0; i < NUM_THREADS; i++ )
        {
          threads.emplace_back( [&]( int thread_index )
          {
            hash_string_t thread_string{ original_pattern };
            std::vector<size_t> thread_counts = counts;
            if ( thread_index > 0 )
            {
              if ( !next_combination( thread_counts, thread_index ) )
                return;
            }
            do
            {
              get_combination( thread_string, thread_counts, indices, indices2 );
              auto match = name_hashes.find( hashlittle2( thread_string ) );
              if ( match != name_hashes.end() )
                print_match( original_pattern, thread_string.as_string(), match->second );
            } while ( next_combination( thread_counts, NUM_THREADS ) );
          }, i );
        }

        for ( auto& thread : threads )
          thread.join();
      }
    }
  }

  return 0;
}
