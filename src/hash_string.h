#pragma once

#include "dictionary.h"
#include "util.h"

#include <memory>
#include <string_view>
#include <vector>

enum hash_type : size_t
{
  H_HASHLITTLE2 = 0,
  H_SSTRHASH = 1,
};

// state for partially completed hash
struct hash_state_t
{
  size_t length;
  uint32_t a;
  uint32_t b;
  uint32_t c;
};

struct hash_string_t
{
  std::string original_str;
  size_t hash_type;
  size_t size; // length of the pattern
  size_t min_size; // minimum length accounting for pattern replacements
  size_t max_size; // maximum length accounting for pattern replacements
  size_t data_size; // maximum length padded to provide extra space for simplified handling
  std::shared_ptr<unsigned char[]> _data; // data_size bytes
  std::vector<hash_state_t> hash_states; // partially completed hash states
  size_t offset; // offset up to where the hash state is computed
  size_t current_size; // needs to be set before computing the hash if it changes

  hash_string_t();
  hash_string_t( std::string_view str, size_t hash_type, const std::vector<dictionary_t>& dictionaries );
  void compute_partial_hash();
  hash_string_t& operator=( std::string_view str );
  unsigned char& operator[]( const size_t index );
  const unsigned char& operator[]( const size_t index ) const;
  std::string as_string( std::string_view original_pattern = {} ) const;
  const unsigned char* data() const;
  const hash_state_t& state() const;
};

#include "hashlittle2.h"
#include "sstrhash.h"
#include "util.h"

hash_string_t::hash_string_t() :
  original_str(), hash_type(), size(), max_size(), data_size(), _data(), hash_states()
{}

hash_string_t::hash_string_t( std::string_view str, size_t hash_type, const std::vector<dictionary_t>& dictionaries = {} ) :
  original_str( str ), hash_type( hash_type ), size( str.size() ), hash_states(), current_size( str.size() )
{
  util::to_upper( original_str );
  size_t num_scratch_bytes = 0;
  min_size = str.size();
  max_size = str.size();
  if ( !dictionaries.empty() )
  {
    size_t dictionary_index = 0;
    for ( auto c : str )
    {
      if ( c == '@' )
      {
        if ( dictionary_index < dictionaries.size() )
        {
          min_size += dictionaries[ dictionary_index ].min_length() - 1;
          max_size += dictionaries[ dictionary_index ].max_length() - 1;
        }
        else
        {
          min_size += dictionaries[ 0 ].min_length() - 1;
          max_size += dictionaries[ 0 ].max_length() - 1;
        }
        dictionary_index++;
      }
      if ( c == '*' || c == '%' )
        num_scratch_bytes++; // extra space to keep track of these replacements
    }
  }
  data_size = max_size;
  if ( ( data_size % 12 ) != 0 )
    data_size += 12 - data_size % 12;
  data_size++; // null byte
  data_size += num_scratch_bytes;
  _data = std::make_shared<unsigned char[]>( data_size );
  for ( size_t i = 0; i < size; i++ )
  {
    if ( str[ i ] == '@' && dictionaries.empty() )
    {
      util::error( "No dictionaries for hash_string_t" );
      std::exit( 1 );
    }
    _data.get()[ i ] = util::to_upper( str[ i ] );
  }
  for ( size_t i = size; i < data_size; i++ )
    _data.get()[ i ] = 0;
  compute_partial_hash();
}

hash_string_t& hash_string_t::operator=( std::string_view str )
{
  *this = hash_string_t{ str, hash_type };
  return *this;
}

unsigned char& hash_string_t::operator[]( const size_t index )
{
  return _data.get()[ index ];
}

const unsigned char& hash_string_t::operator[]( const size_t index ) const
{
  return _data.get()[ index ];
}

std::string hash_string_t::as_string( std::string_view original_pattern ) const
{
  std::string s = "";
  bool try_to_match_pattern = true;
  for ( size_t i = 0; i < current_size; i++ )
  {
    unsigned char ch;
    if ( i < original_pattern.size() && original_pattern[ i ] == '@' )
      try_to_match_pattern = false;
    if ( try_to_match_pattern && i < original_pattern.size() && original_pattern[ i ] != '*' && original_pattern[ i ] != '%' )
      ch = original_pattern[ i ];
    else
      ch = util::to_lower( _data.get()[ i ] );
    if ( ch >= 32 && ch < 127 )
      s += static_cast<char>( ch );
    else
      s += std::format( "<0x{:02x}>", static_cast<unsigned int>( ch ) );
  }
  return s;
}

const unsigned char* hash_string_t::data() const
{
  return _data.get();
}

void hash_string_t::compute_partial_hash()
{
  if ( hash_type == H_HASHLITTLE2 )
  {
    if ( ( *this )[ 0 ] == '*' || ( *this )[ 0 ] == '%' || ( *this )[ 0 ] == '@' )
    {
      offset = 0;
      for ( size_t length = min_size; length <= max_size; length++ )
        hash_states.emplace_back( hashlittle2_precompute( *this, 0, length ) );
      return;
    }

    size_t i;
    for ( i = 0; i < size - 1; i++ )
    {
      if ( ( *this )[ i + 1 ] == '*' || ( *this )[ i + 1 ] == '%' || ( *this )[ i + 1 ] == '@' )
        break;
    }
    offset = i - i % 12;
    for ( size_t length = min_size; length <= max_size; length++ )
      hash_states.emplace_back( hashlittle2_precompute( *this, offset, length ) );
  }
  else if ( hash_type == H_SSTRHASH )
  {
    size_t i;
    for ( i = 0; i < size; i++ )
    {
      if ( ( *this )[ i ] == '*' || ( *this )[ i ] == '%' || ( *this )[ i ] == '@' )
        break;
    }
    hash_states.emplace_back( s_str_hash_precompute( *this, i ) );
  }
}

const hash_state_t& hash_string_t::state() const
{
  if ( hash_states.empty() )
  {
    util::error( "hash_string_t has no hash state" );
    std::exit( 1 );
  }

  if ( hash_type == H_HASHLITTLE2 )
  {
    // TODO: optimize
    for ( const auto& h : hash_states )
    {
      if ( h.length == current_size )
        return h;
    }
    util::error( "hash_string_t has no hash state of length {}", current_size );
    std::exit( 1 );
  }

  return hash_states[ 0 ];
}
