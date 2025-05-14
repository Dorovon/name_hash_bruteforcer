#include "gpu_kernel.h"

#include "cl/errors.h"
#include "util.h"

void check_error( cl_int error, std::string_view str )
{
  if ( error == CL_SUCCESS )
    return;

  util::error( "Error in {}: {} ({})", str, cl_error_string( error ), error );
  std::exit( 1 );
}

cl_device_id find_gpu()
{
  cl_int error;
  cl_platform_id platform_id;
  cl_uint num_platforms;
  error = clGetPlatformIDs( 1, &platform_id, &num_platforms );
  check_error( error, "clGetPlatformIDs" );
  if ( num_platforms == 0 )
  {
      util::error( "Error: No OpenCL platforms found" );
      std::exit( 1 );
  }

  cl_device_id device_id;
  cl_uint num_devices;
  error = clGetDeviceIDs( platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_devices );
  check_error( error, "clGetDeviceIDs" );
  if ( num_devices == 0 )
  {
      util::error( "Error: No GPUs found" );
      std::exit( 1 );
  }

  return device_id;
}

gpu_context_t::gpu_context_t() :
  context(), queue(), device_id()
{
  cl_int error;
  device_id = find_gpu();
  context = clCreateContext( NULL, 1, &device_id, NULL, NULL, &error );
  check_error( error, "clCreateContext" );
  queue = clCreateCommandQueueWithProperties( context, device_id, 0, &error );
  check_error( error, "clCreateCommandQueueWithProperties" );
}

gpu_context_t::~gpu_context_t()
{
  if ( queue )
    clReleaseCommandQueue( queue );
  if ( context )
    clReleaseContext( context );
}

gpu_kernel_t::gpu_kernel_t( const gpu_context_t& gpu_context, std::vector<const char*>& source, const char* entry_point, size_t total_work, size_t work_size ) :
  context( gpu_context.context ),
  queue( gpu_context.queue ),
  program(),
  kernel(),
  buffers(),
  remaining_work( total_work ),
  work_size( work_size ),
  kernel_event(),
  write_events()
{
  cl_int error;
  program = clCreateProgramWithSource( context, static_cast<cl_uint>( source.size() ), source.data(), NULL, &error );
  check_error( error, "clCreateProgramWithSource" );

  error = clBuildProgram( program, 1, &gpu_context.device_id, NULL, NULL, NULL );
  if ( error != CL_SUCCESS )
  {
    if ( error == CL_BUILD_PROGRAM_FAILURE )
    {
      util::error( "Error in clBuildProgram: {} ({})", cl_error_string( error ), error );
      size_t log_size;
      clGetProgramBuildInfo( program, gpu_context.device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size );
      std::string log( log_size, '\0' );
      clGetProgramBuildInfo( program, gpu_context.device_id, CL_PROGRAM_BUILD_LOG, log_size, log.data(), NULL );
      util::print( log );
      std::exit( 1 );
    }
    check_error( error, "clBuildProgram" );
  }

  kernel = clCreateKernel( program, entry_point, &error );
  check_error( error, "clCreateKernel" );
}

gpu_kernel_t::~gpu_kernel_t()
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
}

cl_mem gpu_kernel_t::add_buffer( size_t size, cl_mem_flags flags )
{
  cl_int error;
  buffers.push_back( clCreateBuffer( context, flags, size, NULL, &error ) );
  check_error( error, "clCreateBuffer" );
  return buffers.back();
}

void gpu_kernel_t::write_buffer( cl_mem buffer, const void* data, size_t size )
{
  cl_event write_event;
  cl_int error = clEnqueueWriteBuffer( queue, buffer, CL_FALSE, 0, size, data, 0, NULL, &write_event );
  write_events.push_back( write_event );
  check_error( error, "clEnqueueWriteBuffer" );
}

cl_event gpu_kernel_t::read_buffer( cl_mem buffer, void* data, size_t size )
{
  cl_event read_event;
  cl_int error = clEnqueueReadBuffer( queue, buffer, CL_FALSE, 0, size, data, 1, &kernel_event, &read_event );
  check_error( error, "clEnqueueReadBuffer" );
  return read_event;
}

void gpu_kernel_t::set_arg( cl_uint arg_index, cl_mem buffer )
{
  cl_int error = clSetKernelArg( kernel, arg_index, sizeof( buffer ), ( void* ) &buffer );
  check_error( error, "clSetKernelArg" );
}

size_t gpu_kernel_t::execute()
{
  size_t this_work_size = ( remaining_work < work_size ) ? remaining_work : work_size;
  remaining_work -= this_work_size;
  cl_int error = clEnqueueNDRangeKernel( queue, kernel, 1, NULL, &this_work_size, NULL, static_cast<cl_uint>( write_events.size() ), write_events.data(), &kernel_event );
  check_error( error, "clEnqueueNDRangeKernel" );
  for ( auto& e : write_events )
  {
    if ( e )
      clReleaseEvent( e );
  }
  write_events.clear();
  return this_work_size;
}
