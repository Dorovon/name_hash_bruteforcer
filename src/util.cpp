#include "util.h"

#include <fstream>
#include <iostream>

std::vector<std::string_view> util::string_split( std::string_view str, std::string_view delim )
{
  std::vector<std::string_view> splits;
  size_t start = 0;

  for ( size_t i = 0; i < str.size() - delim.size(); i++ )
  {
    if ( str.substr( i, delim.size() ) == delim )
    {
      if ( i > start )
        splits.emplace_back( str.substr( start, i - start ) );
      i += delim.size();
      start = i;
    }
  }

  if ( start < str.size() )
    splits.emplace_back( str.substr( start, str.size() - start ) );

  return splits;
}

std::string util::read_text_file( std::string path )
{
  std::ifstream file( path, std::ios::binary );
  if ( !file.good() )
  {
    error( "Error opening file: {}", path );
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
