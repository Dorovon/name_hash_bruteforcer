#include "util.h"

#include <format>
#include <fstream>
#include <iostream>

char util::to_upper( char c )
{
  if ( c >= 'a' && c <= 'z' )
    return c & 0xdf;

  if ( c == '/' )
    return '\\';

  return c;
}

char util::to_lower( char c )
{
  if ( c >= 'A' && c <= 'Z' )
    return c | 0x20;

  if ( c == '\\' )
    return '/';

  return c;
}

void util::to_upper( std::string& str )
{
  for ( size_t i = 0; i < str.size(); i++ )
    str[ i ] = to_upper( str[ i ] );
}

// comparison function for case-insensitive strings
bool util::str_lt_ci( std::string_view first, std::string_view second )
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

std::vector<std::string_view> util::string_split( std::string_view str, std::string_view delim )
{
  std::vector<std::string_view> splits;
  size_t start = 0;

  for ( size_t i = 0; i < str.size() - delim.size(); i++ )
  {
    if ( str.substr( i, delim.size() ) == delim )
    {
      if ( i > start )
      {
        splits.emplace_back( str.substr( start, i - start ) );
      }
      i += delim.size();
      start = i;
    }
  }

  if ( start < str.size() )
  {
    splits.emplace_back( str.substr( start, str.size() - start ) );
  }

  return splits;
}

std::string util::read_text_file( std::string& path )
{
  std::ifstream file( path, std::ios::binary );
  if ( !file.good() )
  {
    std::cerr << std::format( "Error opening file: {}", path ) << std::endl;
    std::exit( 1 );
  }

  // get file size
  file.unsetf( std::ios::skipws );
  file.seekg( 0, std::ios::end );
  size_t size = file.tellg();
  file.seekg( 0, std::ios::beg );

  // load the entire file into a single string block
  std::string file_data;
  file_data.resize( size );
  file.read( file_data.data(), size );
  return file_data;
}

