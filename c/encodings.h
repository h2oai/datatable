//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_ENCODINGS_H
#define dt_ENCODINGS_H
#include <cstddef>
#include <cstdint>
using std::size_t;


int is_valid_utf8(const uint8_t* src, size_t len);

int check_escaped_string(const uint8_t* src, size_t len, uint8_t escape_char);

int decode_escaped_csv_string(const uint8_t* src, int len, uint8_t* dest, uint8_t quote);

int decode_sbcs(const uint8_t* src, int len, uint8_t* dest, uint32_t *map);

int64_t utf32_to_utf8(uint32_t* buf, size_t maxchars, char* ch);

int read_codepoint_from_utf8(const uint8_t** src);


// Defined in utils/wcwidth.cc
int mk_wcwidth(int ucp);

#endif
