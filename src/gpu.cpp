#include "gpu.h"

#include "cl/errors.h"
#include "util.h"

#include <cstring>
#include <thread>

void check_error( cl_int error, std::string_view str )
{
  if ( error == CL_SUCCESS )
    return;

  util::error( "Error in {}: {} ({})", str, cl_error_string( error ), error );
  std::exit( 1 );
}

std::vector<cl_platform_id> get_platforms()
{
  cl_int error;
  cl_uint num_platforms;
  error = clGetPlatformIDs( 0, NULL, &num_platforms );
  check_error( error, "clGetPlatformIDs" );
  if ( num_platforms == 0 )
  {
    util::error( "Error: No OpenCL platforms found" );
    std::exit( 1 );
  }

  std::vector<cl_platform_id> platform_ids( num_platforms );
  error = clGetPlatformIDs( num_platforms, platform_ids.data(), NULL );
  check_error( error, "clGetPlatformIDs" );

  return platform_ids;
}

std::vector<cl_device_id> get_gpus( cl_platform_id platform_id )
{
  cl_int error;
  cl_uint num_devices;
  error = clGetDeviceIDs( platform_id, CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices );
  if ( error != CL_DEVICE_NOT_FOUND )
    check_error( error, "clGetDeviceIDs" );
  if ( error == CL_DEVICE_NOT_FOUND || num_devices == 0 )
    return {}; // no GPUs found

  std::vector<cl_device_id> device_ids( num_devices );
  error = clGetDeviceIDs( platform_id, CL_DEVICE_TYPE_GPU, num_devices, device_ids.data(), NULL );
  check_error( error, "clGetDeviceIDs" );

  return device_ids;
}

gpu_context_t::gpu_context_t() :
  contexts(), queues(), device_ids()
{
  cl_int error;
  for ( auto platform_id : get_platforms() )
  {
    auto gpus = get_gpus( platform_id );
    if ( gpus.size() > 0 )
    {
      contexts.push_back( clCreateContext( NULL, static_cast<cl_uint>( gpus.size() ), gpus.data(), NULL, NULL, &error ) );
      check_error( error, "clCreateContext" );
      queues.emplace_back();
      for ( auto d : gpus )
      {
        queues.back().push_back( clCreateCommandQueueWithProperties( contexts.back(), d, 0, &error ) );
        check_error( error, "clCreateCommandQueueWithProperties" );
      }
      device_ids.push_back( gpus );
    }
  }
}

gpu_context_t::~gpu_context_t()
{
  for ( auto& v : queues )
  {
    for ( auto& q : v )
      clReleaseCommandQueue( q );
  }
  for ( auto& c : contexts )
    clReleaseContext( c );
}

gpu_t::gpu_t( cl_context context, cl_command_queue queue, cl_device_id device_id ) :
  context( context ),
  queue( queue ),
  device_id( device_id ),
  program(),
  kernel(),
  num_buffers(),
  current_buffer_index(),
  buffers(),
  indexed_buffers(),
  host_buffers(),
  buffer_sizes(),
  work_sizes(),
  kernel_event(),
  write_events(),
  read_events()
{}

gpu_t::~gpu_t()
{
  reset();
}

void gpu_t::reset()
{
  if ( kernel_event )
    clReleaseEvent( kernel_event );
  for ( auto& e : write_events )
  {
    if ( e )
      clReleaseEvent( e );
  }
  for ( auto& b : buffers )
  {
    if ( b )
      clReleaseMemObject( b );
  }
  if ( kernel )
    clReleaseKernel( kernel );
  if ( program )
    clReleaseProgram( program );

  buffers.clear();
  indexed_buffers.clear();
  host_buffers.clear();
  buffer_sizes.clear();
  work_sizes.clear();
  write_events.clear();
  read_events.clear();
  kernel_event = nullptr;
}

void gpu_t::init( std::vector<const char*>& source, const char* entry_point, size_t num_gpu_buffers )
{
  reset();
  num_buffers = num_gpu_buffers;
  cl_int error;
  program = clCreateProgramWithSource( context, static_cast<cl_uint>( source.size() ), source.data(), NULL, &error );
  check_error( error, "clCreateProgramWithSource" );

  error = clBuildProgram( program, 1, &device_id, NULL, NULL, NULL );
  if ( error != CL_SUCCESS )
  {
    if ( error == CL_BUILD_PROGRAM_FAILURE )
    {
      util::error( "Error in clBuildProgram: {} ({})", cl_error_string( error ), error );
      size_t log_size;
      clGetProgramBuildInfo( program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size );
      std::string log( log_size, '\0' );
      clGetProgramBuildInfo( program, device_id, CL_PROGRAM_BUILD_LOG, log_size, log.data(), NULL );
      util::print( log );
      std::exit( 1 );
    }
    check_error( error, "clBuildProgram" );
  }

  kernel = clCreateKernel( program, entry_point, &error );
  check_error( error, "clCreateKernel" );

  for ( size_t i = 0; i < num_buffers; i++ )
  {
    indexed_buffers.emplace_back();
    host_buffers.emplace_back();
    work_sizes.emplace_back();
    read_events.emplace_back();
  }
}

cl_mem gpu_t::add_buffer( size_t size, cl_mem_flags flags )
{
  cl_int error;
  buffers.push_back( clCreateBuffer( context, flags, size, NULL, &error ) );
  check_error( error, "clCreateBuffer" );
  return buffers.back();
}

void gpu_t::write_buffer( cl_mem buffer, const void* data, size_t size )
{
  cl_event write_event;
  cl_int error = clEnqueueWriteBuffer( queue, buffer, CL_FALSE, 0, size, data, 0, NULL, &write_event );
  write_events.push_back( write_event );
  check_error( error, "clEnqueueWriteBuffer" );
}

cl_event gpu_t::read_buffer( cl_mem buffer, void* data, size_t size )
{
  cl_event read_event;
  cl_int error = clEnqueueReadBuffer( queue, buffer, CL_FALSE, 0, size, data, 1, &kernel_event, &read_event );
  check_error( error, "clEnqueueReadBuffer" );
  return read_event;
}

void gpu_t::read_buffer( size_t index )
{
  if ( index >= indexed_buffers[ current_buffer_index ].size() )
  {
    util::error( "Error in read_buffer: index={} indexed_buffers.size()={}", index, indexed_buffers.size() );
    std::exit( 1 );
  }

  cl_mem buffer = indexed_buffers[ current_buffer_index ][ index ];
  cl_event read_event = read_buffer( buffer, host_buffers[ current_buffer_index ][ index ].data(), buffer_sizes[ index ] );
  read_events[ current_buffer_index ].push_back( read_event );
}

void gpu_t::write_indexed_buffer( size_t index, const void* data )
{
  if ( index >= indexed_buffers[ current_buffer_index ].size() )
  {
    util::error( "Error in write_indexed_buffer: index={} indexed_buffers.size()={}", index, indexed_buffers.size() );
    std::exit( 1 );
  }

  std::memcpy( host_buffers[ current_buffer_index ][ index ].data(), data, buffer_sizes[ index ] );
  write_buffer( indexed_buffers[ current_buffer_index ][ index ],
                host_buffers[ current_buffer_index ][ index ].data(),
                buffer_sizes[ index ] );
}

void gpu_t::set_arg( cl_uint arg_index, cl_mem buffer )
{
  cl_int error = clSetKernelArg( kernel, arg_index, sizeof( buffer ), ( void* ) &buffer );
  check_error( error, "clSetKernelArg" );
}

void gpu_t::set_arg( cl_uint arg_index, size_t buffer_index )
{
  if ( buffer_index >= indexed_buffers[ current_buffer_index ].size() )
  {
    util::error( "Error in set_arg: buffer_index={} indexed_buffers.size()={}", buffer_index, indexed_buffers.size() );
    std::exit( 1 );
  }

  cl_mem buffer = indexed_buffers[ current_buffer_index ][ buffer_index ];
  set_arg( arg_index, buffer );
}

size_t gpu_t::execute()
{
  cl_int error = clEnqueueNDRangeKernel( queue, kernel, 1, NULL, &work_sizes[ current_buffer_index ], NULL,
                                         static_cast<cl_uint>( write_events.size() ), write_events.data(), &kernel_event );
  check_error( error, "clEnqueueNDRangeKernel" );
  for ( auto& e : write_events )
  {
    if ( e )
      clReleaseEvent( e );
  }
  write_events.clear();
  return work_sizes[ current_buffer_index ];
}

gpu_pool_t::gpu_pool_t( size_t num_buffers_per_device ) :
  num_buffers_per_device( num_buffers_per_device ),
  gpu_context(),
  gpus(),
  init_shared_buffers(),
  init_device_buffers(),
  prepare_batch(),
  execute_batch(),
  batch_results()
{
  for ( size_t i = 0; i < gpu_context.contexts.size(); i++ )
  {
    cl_context context = gpu_context.contexts[ i ];
    for ( size_t j = 0; j < gpu_context.device_ids[ i ].size(); j++ )
    {
      cl_command_queue queue = gpu_context.queues[ i ][ j ];
      cl_device_id device_id = gpu_context.device_ids[ i ][ j ];
      gpus.emplace_back( context, queue, device_id );
    }
  }

  if ( gpus.size() == 0 )
  {
    util::error( "No OpenCL devices found." );
    std::exit( 1 );
  }
}

void gpu_pool_t::init_gpus( std::vector<const char*>& source, const char* entry_point )
{
  for ( auto& gpu : gpus )
    gpu.init( source, entry_point, num_buffers_per_device );
}

void gpu_pool_t::set_fn_init_shared_buffers( std::function<void( gpu_t& )> f )
{
  init_shared_buffers = f;
}

void gpu_pool_t::set_fn_init_device_buffers( std::function<void( gpu_t& )> f )
{
  init_device_buffers = f;
}

void gpu_pool_t::set_fn_prepare_batch( std::function<bool( gpu_t& )> f )
{
  prepare_batch = f;
}

void gpu_pool_t::set_fn_execute_batch( std::function<void (gpu_t& )> f )
{
  execute_batch = f;
}

void gpu_pool_t::set_fn_batch_results( std::function<void( gpu_t& )> f )
{
  batch_results = f;
}

void gpu_pool_t::execute()
{
  for ( auto& gpu : gpus )
  {
    init_shared_buffers( gpu );
    init_device_buffers( gpu );
  }

  std::vector<std::thread> threads;
  for ( int t = 0; t < static_cast<int>( gpus.size() ); t++ )
  {
    threads.emplace_back( [ & ]( int thread_index )
    {
      auto& gpu = gpus[ thread_index ];

      auto check_results = [ & ]
      {
        auto& read_events = gpu.read_events[ gpu.current_buffer_index ];
        if ( !read_events.empty() )
        {
          clWaitForEvents( static_cast<cl_uint>( read_events.size() ), read_events.data() );
          batch_results( gpu );
          for ( size_t i = 0; i < read_events.size(); i++ )
            clReleaseEvent( read_events[ i ] );
          read_events.clear();
        }
      };

      gpu.current_buffer_index = 0;
      bool not_done = true;
      while ( not_done )
      {
        // If the current buffers already have results waiting, check those results before queuing more work.
        check_results();

        // prepare the next batch of work
        not_done = prepare_batch( gpu );

        // Queue commands for the GPU to start executing the next batch of work.
        execute_batch( gpu );

        // Switch to the next set of buffers.
        gpu.current_buffer_index = ( gpu.current_buffer_index + 1 ) % gpu.num_buffers;
      }

      // Check any remaining results.
      for ( gpu.current_buffer_index = 0; gpu.current_buffer_index < gpu.num_buffers; gpu.current_buffer_index++ )
        check_results();
    }, t );
  }

  for ( auto& thread : threads )
    thread.join();
}
