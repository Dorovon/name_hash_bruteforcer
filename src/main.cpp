#include "gpu_kernel.h"
#include "hash_string.h"
#include "hashlittle2.h"
#include "kernels.h"
#include "progress_bar.h"
#include "util.h"

#include <chrono>
#include <format>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

static std::string LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ";
static size_t LETTERS_SIZE = LETTERS.size();
static const char* PROGRAM_NAME = "bruteforcer";
static size_t GPU_BATCH_MAX_RESULTS = 1024;
static size_t GPU_MAX_WORK_SIZE = 2ull << 30;
constexpr size_t NUM_GPU_BUFFERS = 2;

void print_match( std::string_view original_pattern, std::string_view match, uint32_t file_data_id = 0 )
{
  std::string s;
  if ( file_data_id > 0 )
    s += std::format( "{};", file_data_id );
  s.reserve( s.size() + match.size() );
  for ( size_t i = 0; i < match.size(); i++ )
  {
    if ( i < original_pattern.size() && original_pattern[ i ] != '*' && original_pattern[ i ] != '%' )
      s += original_pattern[ i ];
    else
      s += util::to_lower( match[ i ] );
  }
  util::print_green( s );
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

void set_alphabet( std::string_view str )
{
  LETTERS = str;
  LETTERS_SIZE = LETTERS.size();
  util::to_upper( LETTERS );
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
                      "[-?]", PROGRAM_NAME );
}

void print_help()
{
  util::print( usage_message() );
  util::print( "\nOPTIONS\n"
               "  -a  use the given alphabet instead of the default\n"
               "  -c  limit the number of threads used to the given number\n"
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

int main( int argc, char** argv )
{
  int arg_index = 0;
  auto get_next_arg = [ & ]()
  {
    if ( arg_index >= argc )
      exit_usage( "Error while parsing options" );

    return argv[ arg_index++ ];
  };

  std::string listfile_path = "";
  std::string pattern_path = "";
  std::string name_hash_str = "";
  std::vector<std::string> patterns;
  std::vector<std::string> alphabets;
  int NUM_THREADS = std::thread::hardware_concurrency();
  bool use_gpu = false;
  bool quiet = false;
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
        print_help();
        break;
      case 'g':
      {
        use_gpu = true;
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
        quiet = true;
        break;
      default:
        exit_usage( std::format( "Error: Unsupported argument: {}", current_arg ) );
    }
  }

  while ( alphabets.size() < patterns.size() )
    alphabets.push_back( LETTERS );

  if ( name_hash_str.empty() )
    exit_usage();

  // read listfile if given
  std::string listfile_data;
  std::unordered_map<uint32_t, std::string_view> listfile;
  if ( !listfile_path.empty() )
  {
    listfile_data = util::read_text_file( listfile_path );
    util::read_lines( listfile_data, [&] ( std::string_view line )
    {
      auto splits = util::string_split( line, ";" );
      if ( splits.size() >= 2 )
        listfile[ std::stoul( std::string( splits[ 0 ] ) ) ] = splits[ 1 ];
    } );
  }

  // read patterns from file if given
  if ( !pattern_path.empty() )
  {
    std::string pattern_data = util::read_text_file( pattern_path );
    util::read_lines( pattern_data, [&] ( std::string_view line )
    {
      if ( !line.empty() && line[ 0 ] != '#' )
      {
        auto splits = util::string_split( line, ";" );
        patterns.push_back( std::string( splits[ 0 ] ) );
        if ( splits.size() == 2 )
          alphabets.push_back( std::string( splits[ 1 ] ) );
        else
          alphabets.push_back( LETTERS );
      }
    } );
  }

  // read name hashes
  std::unordered_map<uint64_t, uint32_t> name_hashes;
  try
  {
    name_hashes[ std::stoull( name_hash_str, nullptr, 16 ) ] = 0;
  }
  catch ( std::exception& )
  {
    std::string name_hash_data = util::read_text_file( name_hash_str );
    util::read_lines( name_hash_data, [&] ( std::string_view line )
    {
      auto splits = util::string_split( line, ";" );
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
    } );
  }

  // name hashes are required, so exit if none were provided
  if ( name_hashes.empty() )
    exit_usage( "Error: at least one name hash must be provided" );

  if ( patterns.empty() )
  {
    // Look for matches using names from the listfile
    if ( listfile.empty() )
      exit_usage( "Error: either a listfile or pattern must be provided" );

    std::set<std::string_view, decltype( util::str_lt_ci )*> path_names( util::str_lt_ci );
    std::set<std::string_view, decltype( util::str_lt_ci )*> base_names( util::str_lt_ci );
    size_t num_paths = 0;
    size_t num_base = 0;
    for ( auto& [ file_data_id, name ] : listfile )
    {
      // check if prepending the hash with some specific directories finds anything
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
          if ( path_names.insert( name.substr( 0, i ) ).second )
            num_paths++;
          if ( base_names.insert( name.substr( i + 1, name.size() - i - 1 ) ).second )
            num_base++;
          break;
        }
      }
    }

    progress_bar_t progress_bar( static_cast<double>( num_paths ) * num_base );
    std::vector<std::string_view> base_names_vec{ base_names.begin(), base_names.end() };
    std::vector<std::thread> threads;
    progress_bar.reset_threads();
    for ( int i = 0; i < NUM_THREADS; i++ )
    {
      threads.emplace_back( [&]( int thread_index )
      {
        size_t update_count = 0;
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
            if ( !quiet )
            {
              update_count++;
              if ( update_count > 10000 )
              {
                progress_bar.increment( update_count );
                update_count = 0;
              }
            }
          }
        }
        if ( !quiet )
          progress_bar.increment( update_count );
        progress_bar.finish_thread();
      }, i );
    }
    if ( !quiet )
    {
      threads.emplace_back( [&] ( int )
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
    progress_bar.finish();
  }
  else
  {
    // look for matches using the provided pattern(s)
    constexpr uint64_t bucket_mask = 0xffff;
    size_t bucket_size = 0;
    std::vector<uint64_t> bucket_hashes;
    std::unique_ptr<gpu_context_t> gpu_context;
    if ( use_gpu )
    {
      gpu_context = std::make_unique<gpu_context_t>();
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

    double total_combinations = 0;
    for ( size_t i = 0; i < patterns.size(); i++ )
    {
      double pattern_combinations = 1;
      size_t size_1 = 0;
      size_t size_2 = 0;
      for ( size_t j = 0; j < patterns[ i ].size(); j++ )
      {
        if ( patterns[ i ][ j ] == '*' )
          size_1++;
        if ( patterns[ i ][ j ] == '%' )
          size_2++;
      }
      for ( size_t j = 0; j < size_1 || j < size_2; j++ )
        pattern_combinations *= alphabets[ i ].size();
      total_combinations += pattern_combinations;
    }
    progress_bar_t progress_bar( total_combinations );

    for ( size_t pattern_index = 0; pattern_index < patterns.size(); pattern_index++ )
    {
      const auto& original_pattern = patterns[ pattern_index ];
      set_alphabet( alphabets[ pattern_index ] );
      hash_string_t current_string{ original_pattern };
      std::vector<size_t> indices;
      std::vector<size_t> indices2;

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
        progress_bar.increment( 1 );
        continue;
      }

      if ( use_gpu )
      {
        // Prepare the source code for the kernel.
        std::string defines;
        defines += std::format( "#define NUM_LETTERS {}\n", LETTERS_SIZE );
        defines += std::format( "#define LETTERS \"{}\"\n", LETTERS );
        defines += "#define STR ";
        for ( size_t i = current_string.offset; i < current_string.data_size; i++ )
        {
          if ( i != current_string.offset )
            defines += ",";
          defines += std::to_string( static_cast<unsigned int>( current_string[ i ] ) );
        }
        defines += "\n";
        defines += std::format( "#define LEN {}\n", current_string.data_size - current_string.offset - 12 );
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
        defines += std::format( "#define A {}\n", current_string.a );
        defines += std::format( "#define B {}\n", current_string.b );
        defines += std::format( "#define C {}\n", current_string.c );
        defines += std::format( "#define BUCKET_MASK {}\n", bucket_mask );
        defines += std::format( "#define NUM_HASHES {}\n", bucket_hashes.size() );
        defines += std::format( "#define BUCKET_SIZE {}\n", bucket_size );
        defines += std::format( "#define MAX_RESULTS {}\n", GPU_BATCH_MAX_RESULTS );
        std::vector<const char*> source;
        source.push_back( defines.c_str() );
        source.push_back( kernels::hashlittle2 );

        size_t total_work = 1;
        for ( size_t i = 0; i < indices.size(); i++ )
          total_work *= LETTERS_SIZE;
        gpu_kernel_t gpu{ *gpu_context, source, "bruteforce", total_work, GPU_MAX_WORK_SIZE };

        // device memory buffers
        cl_mem hash_buffer;
        cl_mem initial_counts_buffer[ NUM_GPU_BUFFERS ];
        cl_mem num_results_buffer[ NUM_GPU_BUFFERS ];
        cl_mem results_buffer[ NUM_GPU_BUFFERS ];
        hash_buffer = gpu.add_buffer( bucket_hashes.size() * sizeof( uint64_t ) );
        gpu.write_buffer( hash_buffer, bucket_hashes.data(), bucket_hashes.size() * sizeof( uint64_t ) );
        gpu.set_arg( 3, hash_buffer );
        for ( size_t i = 0; i < NUM_GPU_BUFFERS; i++ )
        {
          initial_counts_buffer[ i ] = gpu.add_buffer( counts.size() * sizeof( size_t ) );
          num_results_buffer[ i ] = gpu.add_buffer( sizeof( cl_uint ) );
          results_buffer[ i ] = gpu.add_buffer( GPU_BATCH_MAX_RESULTS * sizeof( size_t ) );
        }

        // host buffers and events
        size_t batch_index = 0;
        constexpr size_t num_read_events = 2;
        cl_event read_events[ num_read_events * NUM_GPU_BUFFERS ];
        std::vector<size_t> batch_counts[ NUM_GPU_BUFFERS ];
        cl_uint num_results[ NUM_GPU_BUFFERS ];
        std::vector<size_t> results[ NUM_GPU_BUFFERS ];
        for ( size_t i = 0; i < NUM_GPU_BUFFERS; i++ )
        {
          for ( size_t j = 0; j < num_read_events; j++ )
            read_events[ num_read_events * i + j ] = nullptr;
          num_results[ i ] = 0;
          results[ i ].resize( GPU_BATCH_MAX_RESULTS );
        }

        // check any positive results from the GPU
        auto process_results = [ & ] ( size_t buffer_index )
        {
          for ( size_t i = 0; i < num_read_events; i++ )
          {
            if ( !read_events[ num_read_events * buffer_index + i ] )
              return;
          }

          clWaitForEvents( num_read_events, &read_events[ num_read_events * buffer_index ] );
          if ( num_results[ buffer_index ] >= GPU_BATCH_MAX_RESULTS )
            util::error( "Warning: GPU batch may have exceeded the maximum number of allowed results ({}). Consider using the \"-m\" option to increase this limit.", GPU_BATCH_MAX_RESULTS );
          for ( size_t i = 0; i < num_results[ buffer_index ]; i++ )
          {
            std::vector<size_t> temp_counts = batch_counts[ buffer_index ];
            if ( next_combination( temp_counts, results[ buffer_index ][ i ] ) )
            {
              get_combination( current_string, temp_counts, indices, indices2 );
              uint64_t hash = hashlittle2( current_string );
              auto match = name_hashes.find( hash );
              if ( match != name_hashes.end() )
                print_match( original_pattern, current_string.as_string(), match->second );
              else if ( hash != 0 ) // zeros are used to fill in the buckets, so this isn't a false positive worth warning about
                util::error( "Error: GPU result did not match on CPU ({}): {}", results[ buffer_index ][ i ], current_string.as_string() );
            }
            else
            {
              util::error( "Error: GPU result is out of range ({})", results[ buffer_index ][ i ] );
            }
          }
          for ( size_t i = 0; i < num_read_events; i++ )
          {
            clReleaseEvent( read_events[ num_read_events * buffer_index + i ] );
            read_events[ num_read_events * buffer_index + i ] = nullptr;
          }
        };

        // check all combinations on the GPU
        do
        {
          constexpr cl_uint zero = 0;
          size_t buffer_index = batch_index % NUM_GPU_BUFFERS;
          process_results( buffer_index );
          batch_counts[ buffer_index ] = counts;
          gpu.write_buffer( initial_counts_buffer[ buffer_index ], batch_counts[ buffer_index ].data(), batch_counts[ buffer_index ].size() * sizeof( size_t ) );
          gpu.write_buffer( num_results_buffer[ buffer_index ], &zero, sizeof( cl_uint ) );
          gpu.set_arg( 0, initial_counts_buffer[ buffer_index ] );
          gpu.set_arg( 1, num_results_buffer[ buffer_index ] );
          gpu.set_arg( 2, results_buffer[ buffer_index ] );
          size_t this_work_size = gpu.execute();
          read_events[ num_read_events * buffer_index ] = gpu.read_buffer( num_results_buffer[ buffer_index ], &num_results[ buffer_index ], sizeof( cl_uint ) );
          read_events[ num_read_events * buffer_index + 1 ] = gpu.read_buffer( results_buffer[ buffer_index ], results[ buffer_index ].data(), GPU_BATCH_MAX_RESULTS * sizeof( size_t ) );
          batch_index++;
          if ( !quiet )
          {
            progress_bar.increment( this_work_size );
            progress_bar.out();
          }
        } while ( next_combination( counts, GPU_MAX_WORK_SIZE ) );

        for ( size_t i = 0; i < NUM_GPU_BUFFERS; i++ )
          process_results( i );
      }
      else
      {
        // check pattern using CPU
        std::vector<std::thread> threads;
        progress_bar.reset_threads();
        for ( int i = 0; i < NUM_THREADS; i++ )
        {
          threads.emplace_back( [&]( int thread_index )
          {
            hash_string_t thread_string{ original_pattern };
            std::vector<size_t> thread_counts = counts;
            if ( thread_index > 0 )
            {
              if ( !next_combination( thread_counts, thread_index ) )
              {
                progress_bar.finish_thread();
                return;
              }
            }
            size_t update_count = 0;
            do
            {
              get_combination( thread_string, thread_counts, indices, indices2 );
              auto match = name_hashes.find( hashlittle2( thread_string ) );
              if ( match != name_hashes.end() )
                print_match( original_pattern, thread_string.as_string(), match->second );
              if ( !quiet )
              {
                update_count++;
                if ( update_count > 10000 )
                {
                  progress_bar.increment( update_count );
                  update_count = 0;
                }
              }
            } while ( next_combination( thread_counts, NUM_THREADS ) );
            if ( !quiet )
              progress_bar.increment( update_count );
            progress_bar.finish_thread();
          }, i );
        }
        if ( !quiet )
        {
          threads.emplace_back( [&] ( int )
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
    }
    progress_bar.finish();
  }

  return 0;
}
