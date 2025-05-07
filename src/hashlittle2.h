#pragma once

#include "hash_string.h"

uint64_t hashlittle2( hash_string_t& str );
void hashlittle2_precompute( hash_string_t& str, size_t length );
