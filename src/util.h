#pragma once

#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#ifdef _WIN32
#include <Windows.h>
#endif

static size_t last_printr_size = 0;
static std::mutex cout_mutex;

namespace util
{
std::vector<std::string_view> string_split( std::string_view str, std::string_view delim );
std::string read_text_file( std::string path );

inline char to_upper( char c )
{
  if ( c >= 'a' && c <= 'z' )
    return c & 0xdf;

  if ( c == '/' )
    return '\\';

  return c;
}

inline char to_lower( char c )
{
  if ( c >= 'A' && c <= 'Z' )
    return c | 0x20;

  if ( c == '\\' )
    return '/';

  return c;
}

inline void to_upper( std::string& str )
{
  for ( size_t i = 0; i < str.size(); i++ )
    str[ i ] = to_upper( str[ i ] );
}

// comparison function for case-insensitive strings
inline bool str_lt_ci( std::string_view first, std::string_view second )
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

template<typename T>
void read_lines( std::string_view file_data, T line_func )
{
  size_t line_start = 0;
  std::string_view line;
  for ( size_t i = 0; i < file_data.size(); i++ )
  {
    if ( file_data[ i ] == '\n' || file_data[ i ] == '\r' )
    {
      if ( line_start < i )
        line_func( file_data.substr( line_start, i - line_start ) );
      line_start = i + 1;
    }
  }
  if ( line_start < file_data.size() )
    line_func( file_data.substr( line_start, file_data.size() - line_start ) );
}

inline bool stdout_has_color()
{
#ifdef _WIN32
  std::lock_guard<std::mutex> guard( cout_mutex );
  HANDLE stdout_handle = GetStdHandle( STD_OUTPUT_HANDLE );
  DWORD stdout_flags = 0;
  if ( !stdout_handle )
    return false;
  GetConsoleMode( stdout_handle, &stdout_flags );
  if ( ( stdout_flags & ENABLE_VIRTUAL_TERMINAL_PROCESSING ) == 0 )
    SetConsoleMode( stdout_handle, stdout_flags | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
  GetConsoleMode( stdout_handle, &stdout_flags );
  if ( ( stdout_flags & ENABLE_VIRTUAL_TERMINAL_PROCESSING ) == 0 )
    return false;
#endif
  return true;
}

inline void print( std::string_view s )
{
  std::lock_guard<std::mutex> guard( cout_mutex );
  std::cout << s;
  for ( size_t i = s.size(); i < last_printr_size; i++ )
    std::cout << ' ';
  std::cout << std::endl;
  last_printr_size = 0;
}

inline void printr( std::string_view s )
{
  std::lock_guard<std::mutex> guard( cout_mutex );
  std::cout << s;
  for ( size_t i = s.size(); i < last_printr_size; i++ )
    std::cout << ' ';
  std::cout <<  '\r' << std::flush;
  last_printr_size = s.size();
}

inline void error( std::string_view s )
{
  bool has_color = stdout_has_color();
  std::lock_guard<std::mutex> guard( cout_mutex );
  if ( has_color )
    std::cerr << "\033[1;31m";
  std::cerr << s;
  if ( has_color )
    std::cerr << "\033[0m";
  for ( size_t i = s.size(); i < last_printr_size; i++ )
    std::cerr << ' ';
  std::cerr << std::endl;
  last_printr_size = 0;
}

template<class... Args>
inline void print( std::format_string<Args...> fmt, Args&&... args )
{
  print( std::format( fmt, std::forward<Args>( args )... ) );
}

template<class... Args>
inline void printr( std::format_string<Args...> fmt, Args&&... args )
{
  printr( std::format( fmt, std::forward<Args>( args )... ) );
}

template<class... Args>
inline void error( std::format_string<Args...> fmt, Args&&... args )
{
  error( std::format( fmt, std::forward<Args>( args )... ) );
}

inline void print_green( std::string_view s )
{
  if ( stdout_has_color() )
  {
    std::lock_guard<std::mutex> guard( cout_mutex );
    std::cout << "\033[1;32m";
    std::cout << s;
    std::cout << "\033[0m";
    for ( size_t i = s.size(); i < last_printr_size; i++ )
      std::cerr << ' ';
    std::cout << std::endl;
    last_printr_size = 0;
  }
  else
  {
    print( s );
  }
}

template<class... Args>
inline void print_green( std::format_string<Args...> fmt, Args&&... args )
{
  print_green( std::format( fmt, std::forward<Args>( args )... ) );
}
}
