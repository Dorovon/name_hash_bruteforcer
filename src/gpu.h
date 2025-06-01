#pragma once

#include "util.h"

#include <CL/cl.h>
#include <functional>
#include <vector>

struct gpu_context_t
{
  std::vector<cl_context> contexts;
  std::vector<std::vector<cl_command_queue>> queues;
  std::vector<std::vector<cl_device_id>> device_ids;

  gpu_context_t();
  ~gpu_context_t();
};

struct gpu_t
{
  cl_context context;
  cl_command_queue queue;
  cl_device_id device_id;
  cl_program program;
  cl_kernel kernel;
  size_t num_buffers;
  size_t current_buffer_index;
  std::vector<cl_mem> buffers;
  std::vector<std::vector<cl_mem>> indexed_buffers;
  std::vector<std::vector<std::vector<unsigned char>>> host_buffers;
  std::vector<size_t> buffer_sizes;
  std::vector<size_t> work_sizes;
  cl_event kernel_event;
  std::vector<cl_event> write_events;
  std::vector<std::vector<cl_event>> read_events;

  gpu_t( cl_context context, cl_command_queue queue, cl_device_id device_id );
  ~gpu_t();
  void reset();
  void init( std::vector<const char*>& source, const char* entry_point, size_t num_gpu_buffers = 1 );
  cl_mem add_buffer( size_t size, cl_mem_flags flags = CL_MEM_READ_WRITE );
  void write_buffer( cl_mem buffer, const void* data, size_t size );
  cl_event read_buffer( cl_mem buffer, void* data, size_t size );
  void read_buffer( size_t index );
  void write_indexed_buffer( size_t index, const void* data );
  void set_arg( cl_uint arg_index, cl_mem buffer );
  void set_arg( cl_uint arg_index, size_t buffer_index );
  size_t execute();

  void set_next_work_size( size_t work_size )
  {
    work_sizes[ current_buffer_index ] = work_size;
  }

  size_t get_last_work_size()
  {
    return work_sizes[ current_buffer_index ];
  }

  template<typename T>
  void add_indexed_buffer( size_t index, size_t size, cl_mem_flags flags = CL_MEM_READ_WRITE )
  {
    for ( size_t i = 0; i < num_buffers; i++ )
    {
      while ( indexed_buffers[ i ].size() <= index )
        indexed_buffers[ i ].emplace_back();
      while ( host_buffers[ i ].size() <= index )
        host_buffers[ i ].emplace_back();
      while ( buffer_sizes.size() <= index )
        buffer_sizes.push_back( 0 );

      indexed_buffers[ i ][ index ] = add_buffer( size * sizeof( T ), flags );
      host_buffers[ i ][ index ].resize( size * sizeof( T ) );
      buffer_sizes[ index ] = size * sizeof( T );
    }
  }

  template<typename T>
  const T* get_host_buffer( size_t index )
  {
    return reinterpret_cast<const T*>( host_buffers[ current_buffer_index ][ index ].data() );
  }
};

struct gpu_pool_t
{
  // the number of buffers to prepare at once for each device
  size_t num_buffers_per_device;
  gpu_context_t gpu_context;
  std::vector<gpu_t> gpus;
  std::function<void( gpu_t& )> init_shared_buffers;
  std::function<void( gpu_t& )> init_device_buffers;
  std::function<bool( gpu_t& )> prepare_batch;
  std::function<void( gpu_t& )> execute_batch;
  std::function<void( gpu_t& )> batch_results;

  gpu_pool_t( size_t num_buffers_per_device = 2 );

  // initialize the GPUs with a kernel
  void init_gpus( std::vector<const char*>& source, const char* entry_point );

  // initialize constant read-only buffers that will be used by every device
  void set_fn_init_shared_buffers( std::function<void( gpu_t& )> f );

  // initialize device buffers that will be used by each batch on each device
  void set_fn_init_device_buffers( std::function<void( gpu_t& )> f );

  // prepare the buffers of a device with a batch of data
  void set_fn_prepare_batch( std::function<bool( gpu_t& )> f );

  // queue the execution of the prepared batch of data
  void set_fn_execute_batch( std::function<void (gpu_t& )> f );

  // process the results from a batch of data
  void set_fn_batch_results( std::function<void( gpu_t& )> f );

  // execute total_work units of work with the given work_size
  void execute();
};
