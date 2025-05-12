#include "gpu_kernel.h"

#include "util.h"

void check_error( cl_int error, std::string_view str )
{
  if ( error == CL_SUCCESS )
    return;

  util::error( "Error in {}: {}", str, error );
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
  if ( num_platforms == 0 )
  {
      util::error( "Error: No GPUs found" );
      std::exit( 1 );
  }

  return device_id;
}

gpu_kernel_t::gpu_kernel_t( std::vector<const char*>& source, const char* entry_point, size_t total_work, size_t work_size ) :
  context(), queue(), program(), kernel(), buffers(), remaining_work( total_work ), work_size( work_size )
{
  cl_int error;
  cl_device_id gpu_device_id = find_gpu();

  context = clCreateContext( NULL, 1, &gpu_device_id, NULL, NULL, &error );
  check_error( error, "clCreateContext" );
  queue = clCreateCommandQueueWithProperties( context, gpu_device_id, 0, &error );
  check_error( error, "clCreateCommandQueueWithProperties" );
  program = clCreateProgramWithSource( context, static_cast<cl_uint>( source.size() ), source.data(), NULL, &error );
  check_error( error, "clCreateProgramWithSource" );

  error = clBuildProgram( program, 1, &gpu_device_id, NULL, NULL, NULL );
  if ( error != CL_SUCCESS )
  {
      size_t log_size;
      clGetProgramBuildInfo( program, gpu_device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size );
      std::string log( log_size, '\0' );
      clGetProgramBuildInfo( program, gpu_device_id, CL_PROGRAM_BUILD_LOG, log_size, log.data(), NULL );
      util::error( "OpenCL kernel build failed: {}", error );
      util::print( log );
      std::exit( 1 );
  }

  kernel = clCreateKernel( program, entry_point, &error );
  check_error( error, "clCreateKernel" );
}

gpu_kernel_t::~gpu_kernel_t()
{
  clReleaseEvent( enqueue_event );
  for ( auto& b : buffers )
    clReleaseMemObject( b );
  clReleaseKernel( kernel );
  clReleaseProgram( program );
  clReleaseCommandQueue( queue );
  clReleaseContext( context );
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
  cl_int error = clEnqueueWriteBuffer( queue, buffer, CL_FALSE, 0, size, data, 0, NULL, NULL );
  check_error( error, "clEnqueueWriteBuffer" );
}

cl_event gpu_kernel_t::read_buffer( cl_mem buffer, void* data, size_t size )
{
  cl_event read_event;
  cl_int error = clEnqueueReadBuffer( queue, buffer, CL_FALSE, 0, size, data, 1, &enqueue_event, &read_event );
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
  cl_int error = clEnqueueNDRangeKernel( queue, kernel, 1, NULL, &this_work_size, NULL, 0, NULL, &enqueue_event );
  check_error( error, "clEnqueueNDRangeKernel" );
  return this_work_size;
}
