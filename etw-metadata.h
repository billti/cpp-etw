#pragma once

#include <utility>

namespace v8 {
namespace etw {

using uint8_t = unsigned char;

// Structure to treat a string literal, or char[], as a constexpr byte sequence
template <size_t N>
struct str_bytes {
  template <std::size_t... I>
  constexpr str_bytes(char const (&s)[N], std::index_sequence<I...>)
      : bytes{s[I]...}, size(N){};

  // Concatenate two str_bytes
  template <std::size_t s1, std::size_t s2, std::size_t... I1,
            std::size_t... I2>
  constexpr str_bytes(const str_bytes<s1>& b1, std::index_sequence<I1...>,
                      const str_bytes<s2>& b2, std::index_sequence<I2...>)
      : bytes{b1.bytes[I1]..., b2.bytes[I2]...}, size(N) {}

  char bytes[N];
  size_t size;
};

// Specialization for 0 (base case when joining fields)
template <>
struct str_bytes<0> {
  constexpr str_bytes() : bytes{}, size(0) {}
  char bytes[1];  // MSVC doesn't like an array of 0 bytes
  size_t size;
};

template <size_t N, typename idx = std::make_index_sequence<N>>
constexpr auto MakeStrBytes(char const (&s)[N]) {
  return str_bytes<N>{s, idx{}};
}

template <std::size_t s1, std::size_t s2>
constexpr auto JoinBytes(const str_bytes<s1>& b1, const str_bytes<s2>& b2) {
  auto idx1 = std::make_index_sequence<s1>();
  auto idx2 = std::make_index_sequence<s2>();
  return str_bytes<s1 + s2>{b1, idx1, b2, idx2};
}

template <size_t N>
constexpr auto Field(char const (&s)[N], uint8_t type) {
  auto field_name = MakeStrBytes(s);
  const char type_arr[1] = {char(type)};
  return JoinBytes(field_name, MakeStrBytes(type_arr));
}

// The metadata starts with a uint16 of the total size, and a 0x00 value tag
constexpr auto Header(size_t size) {
  const char header_bytes[3] = {char(size & 0xFF), char(size >> 8 & 0xFF),
                                char(0x00)};
  return MakeStrBytes(header_bytes);
}

// Empty case needed for events with no fields.
constexpr auto JoinFields() { return str_bytes<0>{}; }

// Only one field, or base case when multiple fields.
template <typename F1>
constexpr auto JoinFields(F1 field1) {
  return field1;
}

template <typename F1, typename F2, typename... Ts>
constexpr auto JoinFields(F1 field1, F2 field2, Ts... args) {
  auto bytes = JoinBytes(field1, field2);
  return JoinFields(bytes, args...);
}

template <std::size_t N, typename... Ts>
constexpr auto EventMetadata(char const (&event_name)[N], Ts... fieldArgs) {
  auto name = MakeStrBytes(event_name);
  auto fields = JoinFields(fieldArgs...);
  auto data = JoinBytes(name, fields);

  auto header = Header(data.size + 3);  // Size includes the 2 byte size + tag
  return JoinBytes(header, data);
}

}  // namespace etw
}  // namespace v8
