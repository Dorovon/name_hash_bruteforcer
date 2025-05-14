#pragma once

#include <CL/cl.h>
#include <vector>

struct gpu_context_t
{
  cl_context context;
  cl_command_queue queue;
  cl_device_id device_id;

  gpu_context_t();
  ~gpu_context_t();
};

struct gpu_kernel_t
{
  cl_context context;
  cl_command_queue queue;
  cl_program program;
  cl_kernel kernel;
  std::vector<cl_mem> buffers;
  size_t remaining_work;
  size_t work_size;
  cl_event kernel_event;
  std::vector<cl_event> write_events;

  gpu_kernel_t( const gpu_context_t& gpu_context, std::vector<const char*>& source, const char* entry_point, size_t total_work, size_t work_size );
  ~gpu_kernel_t();
  cl_mem add_buffer( size_t size, cl_mem_flags flags = CL_MEM_READ_WRITE );
  void write_buffer( cl_mem buffer, const void* data, size_t size );
  cl_event read_buffer( cl_mem buffer, void* data, size_t size );
  void set_arg( cl_uint arg_index, cl_mem buffer );
  size_t execute();
};
