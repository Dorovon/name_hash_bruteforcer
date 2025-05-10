#include "hash_string.h"
#include "hashlittle2.h"
#include "util.h"

#include <CL/cl.h>
#include <format>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

static std::mutex cout_mutex;
static std::string LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ";
static size_t LETTERS_SIZE = LETTERS.size();
static const char* program_name = "bruteforcer";
static size_t GPU_BATCH_MAX_RESULTS = 1024;
static size_t GPU_MAX_WORK_SIZE = 2u << 30;

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
      std::cout << util::to_lower( match[ i ] );
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

void set_alphabet( std::string_view str )
{
  LETTERS = str;
  LETTERS_SIZE = LETTERS.size();
  util::to_upper( LETTERS );
}

std::string usage_message()
{
  return std::format( "Usage: {} <-n name_hash|name_hash_file> [-a alphabet] [-c cpu_threads] [-l listfile] [-p pattern] [-f pattern_file] [-g] [-?]", program_name );
}

void exit_usage( std::string_view str = "" )
{
  if ( !str.empty() )
    std::cerr << str << std::endl;
  std::cerr << usage_message() << std::endl;
  std::exit( 1 );
}

cl_device_id find_gpu()
{
  cl_int error;
  cl_platform_id platform_id;
  cl_uint num_platforms;
  error = clGetPlatformIDs( 1, &platform_id, &num_platforms );
  if ( error != CL_SUCCESS )
  {
      std::cerr << "Error: failed to find OpenCL platform: " << error << std::endl;
      std::exit( 1 );
  }
  if ( num_platforms == 0 )
  {
      std::cerr << "Error: No OpenCL platforms found" << std::endl;
      std::exit( 1 );
  }

  cl_device_id device_id;
  cl_uint num_devices;
  error = clGetDeviceIDs( platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_devices );
  if ( error != CL_SUCCESS )
  {
      std::cerr << "Error: failed to find GPU: " << error << std::endl;
      std::exit( 1 );
  }
  if ( num_platforms == 0 )
  {
      std::cerr << "Error: No GPUs found" << std::endl;
      std::exit( 1 );
  }

  return device_id;
}

void print_help()
{
  std::cout << usage_message()
            << "\n\nOPTIONS\n"
            << "  -a  use the given alphabet instead of the default\n"
            << "  -c  limit the number of threads used to the given number\n"
            << "  -l  use the given listfile for modes that need one\n"
            << "      this will also filter the given name hash file to ignore names that are already known\n"
            << "  -n  compare against the given name hash or\n"
            << "      compare against the name hashes in the given file\n"
            << "  -p  test the given pattern against all provided name hashes\n"
            << "  -f  test the patterns in the given file against all provided name hashes\n"
            << "  -g  use the GPU where supported\n"
            << "  -b  set the batch size for the GPU\n"
            << "  -m  set the maximum results for a single GPU batch\n"
            << "  -?  display this message and exit\n"
            << std::endl;
  std::exit( 0 );
}

int main( int argc, char** argv )
{
  int arg_index = 0;
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
  bool use_gpu = false;
  cl_device_id gpu_device_id = 0;
  program_name = get_next_arg();
  const char* current_arg;
  while ( arg_index < argc )
  {
    current_arg = get_next_arg();
    if ( !current_arg || current_arg[ 0 ] != '-' )
      exit_usage();

    if ( !current_arg[ 1 ] || current_arg[ 2 ] != 0 )
      exit_usage( std::format( "Unsupported argument: {}", current_arg ) );

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
        print_help();
        break;
      case 'g':
      {
        use_gpu = true;
        gpu_device_id = find_gpu();
        break;
      }
      case 'b':
      {
        std::string batch_size_str = get_next_arg();
        GPU_MAX_WORK_SIZE = std::stoull( batch_size_str );
        if ( GPU_MAX_WORK_SIZE <= 0 )
          exit_usage( std::format( "GPU batch size ({}) must be greater than zero", GPU_MAX_WORK_SIZE ) );
        break;
      }
      case 'm':
      {
        std::string max_results_str = get_next_arg();
        GPU_BATCH_MAX_RESULTS = std::stoull( max_results_str );
        if ( GPU_BATCH_MAX_RESULTS <= 0 )
          exit_usage( std::format( "GPU max results ({}) must be greater than zero", GPU_BATCH_MAX_RESULTS ) );
        break;
      }
      default:
        exit_usage( std::format( "Unsupported argument: {}", current_arg ) );
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
    exit_usage( "at least one name hash must be provided" );

  if ( patterns.empty() )
  {
    // Look for matches using names from the listfile
    if ( listfile.empty() )
      exit_usage( "either a listfile or pattern must be provided" );

    std::set<std::string_view, decltype( util::str_lt_ci )*> path_names( util::str_lt_ci );
    std::set<std::string_view, decltype( util::str_lt_ci )*> base_names( util::str_lt_ci );
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
    std::string kernel_source;
    constexpr uint64_t bucket_mask = 0xffff;
    size_t bucket_size = 0;
    std::vector<uint64_t> bucket_hashes;
    if ( use_gpu )
    {
      kernel_source = util::read_text_file( "src/cl/hashlittle2.cl" );
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
        continue;
      }

      if ( use_gpu )
      {
        // Prepare the source code for the kernel.
        std::string defines;
        defines += std::format( "#define NUM_LETTERS {}\n", LETTERS_SIZE );
        defines += std::format( "#define LETTERS \"{}\"\n", LETTERS );
        defines += "#define STR ";
        for ( size_t i = current_string.offset; i < current_string.size; i++ )
        {
          if ( i != current_string.offset )
            defines += ",";
          defines += std::to_string( static_cast<unsigned int>( current_string[ i ] ) );
        }
        for ( size_t i = 0; i < 12 - current_string.size % 12; i++ )
          defines += ",0";
        defines += "\n";
        defines += std::format( "#define LEN {}\n", current_string.size - ( current_string.size % 12 ) - current_string.offset );
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
        source.push_back( kernel_source.c_str() );

        cl_int error;
        cl_context context = clCreateContext( NULL, 1, &gpu_device_id, NULL, NULL, &error );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while creating OpenCL context: " << error << std::endl;
          std::exit( 1 );
        }

        cl_command_queue queue = clCreateCommandQueueWithProperties( context, gpu_device_id, 0, &error );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while creating OpenCL command queue: " << error << std::endl;
          std::exit( 1 );
        }

        cl_program program = clCreateProgramWithSource( context, 2, source.data(), NULL, &error );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while creating OpenCL program from source: " << error << std::endl;
          std::exit( 1 );
        }

        error = clBuildProgram( program, 1, &gpu_device_id, NULL, NULL, NULL );
        if ( error != CL_SUCCESS )
        {
            size_t log_size;
            clGetProgramBuildInfo( program, gpu_device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size );
            std::string log( log_size, '\0' );
            clGetProgramBuildInfo( program, gpu_device_id, CL_PROGRAM_BUILD_LOG, log_size, log.data(), NULL );
            std::cerr << std::format( "OpenCL kernel build failed:\n{}", log ) << std::endl;
            std::exit( 1 );
        }

        cl_kernel kernel = clCreateKernel( program, "bruteforce", &error );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while creating kernel: " << error << std::endl;
          std::exit( 1 );
        }

        cl_mem initial_counts_buffer = clCreateBuffer( context, CL_MEM_READ_ONLY, counts.size() * sizeof( size_t ), NULL, &error );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while creating buffer: " << error << std::endl;
          std::exit( 1 );
        }

        cl_mem num_results_buffer = clCreateBuffer( context, CL_MEM_READ_WRITE, sizeof( cl_uint ), NULL, &error );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while creating buffer: " << error << std::endl;
          std::exit( 1 );
        }

        cl_mem results_buffer = clCreateBuffer( context, CL_MEM_WRITE_ONLY, GPU_BATCH_MAX_RESULTS * sizeof( size_t ), NULL, &error );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while creating buffer: " << error << std::endl;
          std::exit( 1 );
        }

        cl_mem hash_buffer = clCreateBuffer( context, CL_MEM_READ_ONLY, bucket_hashes.size() * sizeof( uint64_t ), NULL, &error );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while creating buffer: " << error << std::endl;
          std::exit( 1 );
        }

        error = clSetKernelArg( kernel, 0, sizeof( initial_counts_buffer ), ( void* ) &initial_counts_buffer );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while setting kernel argument 0: " << error << std::endl;
          std::exit( 1 );
        }

        error = clSetKernelArg( kernel, 1, sizeof( num_results_buffer ), ( void* ) &num_results_buffer );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while setting kernel argument 1: " << error << std::endl;
          std::exit( 1 );
        }

        error = clSetKernelArg( kernel, 2, sizeof( results_buffer ), ( void* ) &results_buffer );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while setting kernel argument 2: " << error << std::endl;
          std::exit( 1 );
        }

        error = clSetKernelArg( kernel, 3, sizeof( hash_buffer ), ( void* ) &hash_buffer );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while setting kernel argument 3: " << error << std::endl;
          std::exit( 1 );
        }

        error = clEnqueueWriteBuffer( queue, hash_buffer, CL_TRUE, 0, bucket_hashes.size() * sizeof( uint64_t ), bucket_hashes.data(), 0, NULL, NULL );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error while writing to hash_buffer: " << error << std::endl;
          std::exit( 1 );
        }

        size_t total_work = 1;
        for ( size_t i = 0; i < indices.size(); i++ )
          total_work *= LETTERS_SIZE;
        size_t work_done = 0;
        size_t zero = 0;
        do
        {
          size_t work_size = ( work_done + GPU_MAX_WORK_SIZE > total_work ) ? total_work - work_done : GPU_MAX_WORK_SIZE;
          work_done += work_size;
          error = clEnqueueWriteBuffer( queue, initial_counts_buffer, CL_TRUE, 0, counts.size() * sizeof( size_t ), counts.data(), 0, NULL, NULL );
          if ( error != CL_SUCCESS )
          {
            std::cerr << "Error while writing to initial_counts_buffer: " << error << std::endl;
            std::exit( 1 );
          }

          error = clEnqueueWriteBuffer( queue, num_results_buffer, CL_TRUE, 0, sizeof( cl_uint ), &zero, 0, NULL, NULL );
          if ( error != CL_SUCCESS )
          {
            std::cerr << "Error while writing to num_results_buffer: " << error << std::endl;
            std::exit( 1 );
          }

          error = clEnqueueNDRangeKernel( queue, kernel, 1, NULL, &work_size, NULL, 0, NULL, NULL );
          if ( error != CL_SUCCESS )
          {
            std::cerr << "Error while enqueuing kernel: " << error << std::endl;
            std::exit( 1 );
          }

          error = clFinish( queue );
          if ( error != CL_SUCCESS )
          {
            std::cerr << "Error in OpenCL finish: " << error << std::endl;
            std::exit( 1 );
          }

          cl_uint num_results = *( ( cl_uint* ) clEnqueueMapBuffer( queue, num_results_buffer, CL_TRUE, CL_MAP_READ, 0, sizeof( cl_uint ), 0, NULL, NULL, &error ) );
          if ( error != CL_SUCCESS )
          {
            std::cerr << "Error retrieving OpenCL num results: " << error << std::endl;
            std::exit( 1 );
          }

          if ( num_results >= GPU_BATCH_MAX_RESULTS )
            std::cerr << std::format( "Warning: GPU batch may have exceeded the maximum number of allowed results ({}). Consider using the \"-m\" option to increase this limit.", GPU_BATCH_MAX_RESULTS );

          size_t* results = ( size_t* ) clEnqueueMapBuffer( queue, results_buffer, CL_TRUE, CL_MAP_READ, 0, GPU_BATCH_MAX_RESULTS * sizeof( size_t ), 0, NULL, NULL, &error );
          if ( error != CL_SUCCESS )
          {
            std::cerr << "Error retrieving OpenCL results: " << error << std::endl;
            std::exit( 1 );
          }

          // error = clEnqueueUnmapMemObject( queue, num_results_buffer, &num_results, NULL, NULL, NULL );
          // if ( error != CL_SUCCESS )
          // {
          //   std::cerr << "Error unmapping num results: " << error << std::endl;
          //   std::exit( 1 );
          // }
          error = clEnqueueUnmapMemObject( queue, results_buffer, results, NULL, NULL, NULL );
          if ( error != CL_SUCCESS )
          {
            std::cerr << "Error unmapping results: " << error << std::endl;
            std::exit( 1 );
          }

          for ( size_t i = 0; i < num_results; i++ )
          {
            std::vector<size_t> temp_counts = counts;
            if ( next_combination( temp_counts, results[ i ] ) )
            {
              get_combination( current_string, temp_counts, indices, indices2 );
              auto match = name_hashes.find( hashlittle2( current_string ) );
              if ( match != name_hashes.end() )
                print_match( original_pattern, current_string.as_string(), match->second );
              else
                std::cerr << std::format( "Error: Invalid GPU result did not match on CPU ({})", results[ i ] ) << std::endl;
            }
            else
            {
              std::cerr << std::format( "Error: Invalid GPU result is out of range ({})", results[ i ] ) << std::endl;
            }
          }
        } while ( next_combination( counts, GPU_MAX_WORK_SIZE ) );

        clReleaseMemObject( initial_counts_buffer );
        clReleaseMemObject( num_results_buffer );
        clReleaseMemObject( results_buffer );
        clReleaseMemObject( hash_buffer );
        clReleaseKernel( kernel );
        clReleaseProgram( program );
        clReleaseCommandQueue( queue );
        clReleaseContext( context );
        if ( error != CL_SUCCESS )
        {
          std::cerr << "Error in OpenCL finish: " << error << std::endl;
          std::exit( 1 );
        }
      }
      else
      {
        // check pattern using CPU
        std::vector<std::thread> threads;
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
