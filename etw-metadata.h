// Copyright 2019 Bill Ticehurst. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>
#include <utility>

namespace etw {

// Structure to treat a string literal, or char[], as a constexpr byte sequence
template <size_t count>
struct str_bytes {
  template <std::size_t... idx>
  constexpr str_bytes(char const (&str)[count], std::index_sequence<idx...>)
      : bytes{str[idx]...}, size(count) {}

  // Concatenate two str_bytes
  template <std::size_t count1, std::size_t count2,
            std::size_t... idx1, std::size_t... idx2>
  constexpr str_bytes(const str_bytes<count1>& s1, std::index_sequence<idx1...>,
                      const str_bytes<count2>& s2, std::index_sequence<idx2...>)
      : bytes{s1.bytes[idx1]..., s2.bytes[idx2]...}, size(count) {}

  char bytes[count];  // NOLINT
  size_t size;
};

// Specialization for 0 (base case when joining fields)
template <>
struct str_bytes<0> {
  constexpr str_bytes() : bytes{}, size(0) {}
  char bytes[1];  // MSVC doesn't like an array of 0 bytes
  size_t size;
};

template <size_t count, typename idx = std::make_index_sequence<count>>
constexpr auto MakeStrBytes(char const (&s)[count]) {
  return str_bytes<count>{s, idx{}};
}

template <std::size_t size1, std::size_t size2>
constexpr auto JoinBytes(const str_bytes<size1>& str1,
                         const str_bytes<size2>& str2) {
  auto idx1 = std::make_index_sequence<size1>();
  auto idx2 = std::make_index_sequence<size2>();
  return str_bytes<size1 + size2>{str1, idx1, str2, idx2};
}

template <size_t count>
constexpr auto Field(char const (&s)[count], uint8_t type) {
  auto field_name = MakeStrBytes(s);
  const char type_arr[1] = {static_cast<char>(type)};
  return JoinBytes(field_name, MakeStrBytes(type_arr));
}

// The metadata starts with a uint16 of the total size, and a 0x00 value tag
constexpr auto Header(size_t size) {
  const char header_bytes[3] = {static_cast<char>(size & 0xFF),
                                static_cast<char>(size >> 8 & 0xFF),
                                '\0'};
  return MakeStrBytes(header_bytes);
}

// Empty case needed for events with no fields.
constexpr auto JoinFields() { return str_bytes<0>{}; }

// Only one field, or base case when multiple fields.
template <typename T1>
constexpr auto JoinFields(T1 field) {
  return field;
}

// Join two or more fields together.
template <typename T1, typename T2, typename... Ts>
constexpr auto JoinFields(T1 field1, T2 field2, Ts... args) {
  auto bytes = JoinBytes(field1, field2);
  return JoinFields(bytes, args...);
}

template <std::size_t count, typename... Ts>
constexpr auto EventMetadata(char const (&name)[count], Ts... field_args) {
  auto name_bytes = MakeStrBytes(name);
  auto fields = JoinFields(field_args...);
  auto data = JoinBytes(name_bytes, fields);

  auto header = Header(data.size + 3);  // Size includes the 2 byte size + tag
  return JoinBytes(header, data);
}

}  // namespace etw
