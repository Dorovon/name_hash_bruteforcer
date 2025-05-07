#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace util
{
char to_upper( char c );
char to_lower( char c );
void to_upper( std::string& str );
bool str_lt_ci( std::string_view first, std::string_view second );
std::vector<std::string_view> string_split( std::string_view str, std::string_view delim );
std::string read_text_file( std::string& path );

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
