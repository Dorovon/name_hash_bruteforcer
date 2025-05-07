#pragma once

#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace util
{
std::vector<std::string_view> string_split( std::string_view str, std::string_view delim );
std::string read_text_file( std::string& path );

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
}
