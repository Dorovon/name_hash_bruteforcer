#pragma once

#include "util.h"

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

struct dictionary_t
{
  std::vector<std::string> _words;
  size_t _min_length;
  size_t _max_length;

  dictionary_t( const std::string& path ) :
    _words(), _min_length( 0 ), _max_length( 0 )
  {
    std::string file_data = util::read_text_file( path );
    util::read_lines( file_data, [ & ]( std::string_view line )
    {
      if ( _min_length == 0 || line.size() < _min_length )
        _min_length = line.size();
      if ( line.size() > _max_length )
        _max_length = line.size();
      _words.emplace_back( line );
      util::to_upper( _words.back() );
    } );

    std::sort( _words.begin(), _words.end(), []( const std::string& a, const std::string& b ) {
      if ( a.size() != b.size() )
        return a.size() < b.size();
      return a < b;
    });
  }

  dictionary_t( const std::vector<std::string_view>& word_list ) :
    _words(), _min_length( 0 ), _max_length( 0 )
  {
    for ( auto line : word_list )
    {
      if ( _min_length == 0 || line.size() < _min_length )
        _min_length = line.size();
      if ( line.size() > _max_length )
        _max_length = line.size();
      _words.emplace_back( line );
      util::to_upper( _words.back() );
    }

    std::sort( _words.begin(), _words.end(), []( const std::string& a, const std::string& b ) {
      if ( a.size() != b.size() )
        return a.size() < b.size();
      return a < b;
    });
  }

  size_t size() const
  {
    return _words.size();
  }

  size_t min_length() const
  {
    return _min_length;
  }

  size_t max_length() const
  {
    return _max_length;
  }

  std::string_view operator[]( const size_t index )
  {
    return _words[ index ];
  }

  const std::string_view operator[]( const size_t index ) const
  {
    return _words[ index ];
  }
};
