#pragma once

#include <memory>
#include <string_view>

struct hash_string_t
{
  // length of the string
  size_t size;

  // string data padded to mod 12 + 1 for simplified hashing and cstring handling
  std::shared_ptr<unsigned char[]> _data;
  size_t data_size;

  // state for partially completed hash
  size_t offset;
  uint32_t a;
  uint32_t b;
  uint32_t c;

  hash_string_t();
  hash_string_t( std::string_view str );
  void compute_partial_hash();
  hash_string_t& operator=( std::string_view str );
  unsigned char& operator[]( const size_t index );
  const unsigned char& operator[]( const size_t index ) const;
  const char* as_string() const;
  const unsigned char* data() const;
};

#include "hashlittle2.h"
#include "util.h"

hash_string_t::hash_string_t() :
  size(), _data(), offset(), a(), b(), c()
{}

hash_string_t::hash_string_t( std::string_view str ) :
  size( str.size() ), offset(), a(), b(), c()
{
  data_size = str.size();
  if ( ( data_size % 12 ) != 0 )
    data_size += 12 - str.size() % 12;
  _data = std::make_shared<unsigned char[]>( data_size + 1 );
  for ( size_t i = 0; i < str.size(); i++ )
    _data.get()[ i ] = util::to_upper( str[ i ] );
  for ( size_t i = str.size(); i < data_size; i++ )
    _data.get()[ i ] = 0;
  _data.get()[ data_size ] = 0; // null byte
  compute_partial_hash();
}

hash_string_t& hash_string_t::operator=( std::string_view str )
{
  *this = hash_string_t{ str };
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

const char* hash_string_t::as_string() const
{
  return reinterpret_cast<const char*>( _data.get() );
}

const unsigned char* hash_string_t::data() const
{
  return _data.get();
}

void hash_string_t::compute_partial_hash()
{
  if ( ( *this )[ 0 ] == '*' || ( *this )[ 0 ] == '%' )
  {
    hashlittle2_precompute( *this, 0 );
    return;
  }

  size_t i;
  for ( i = 0; i < size - 1; i++ )
  {
    if ( ( *this )[ i + 1 ] == '*' || ( *this )[ i + 1 ] == '%' )
      break;
  }
  size_t this_size = i - i % 12;
  hashlittle2_precompute( *this, this_size );
  offset = this_size;
}
