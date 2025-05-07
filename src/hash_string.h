#pragma once

#include <memory>
#include <string_view>

struct hash_string_t
{
  // length of the string
  size_t size;

  // string data padded to mod 12 for simplified hashing
  std::shared_ptr<unsigned char[]> _data;

  // state for partially completed hash
  size_t offset;
  uint32_t a;
  uint32_t b;
  uint32_t c;

  hash_string_t();
  hash_string_t( std::string_view str );
  void initialize();
  hash_string_t& operator=( std::string_view str );
  unsigned char& operator[]( const size_t index );
  const unsigned char& operator[]( const size_t index ) const;
  const char* as_string() const;
  const unsigned char* data() const;
};
