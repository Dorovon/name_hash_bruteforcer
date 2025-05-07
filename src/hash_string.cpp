#include "hash_string.h"

#include "hashlittle2.h"
#include "util.h"

hash_string_t::hash_string_t() :
  size(), _data(), offset(), a(), b(), c()
{}

hash_string_t::hash_string_t( std::string_view str ) :
  size( str.size() ), offset(), a(), b(), c()
{
  size_t data_size = str.size() + 12 - str.size() % 12;
  _data = std::make_shared<unsigned char[]>( data_size );
  for ( size_t i = 0; i < str.size(); i++ )
    _data.get()[ i ] = util::to_upper( str[ i ] );
  for ( size_t i = str.size(); i < data_size; i++ )
    _data.get()[ i ] = 0;
  initialize();
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

void hash_string_t::initialize()
{
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
