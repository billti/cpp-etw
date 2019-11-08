// Copyright 2019 Bill Ticehurst. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>

#include "./etw-provider.h"

namespace chakra {

using etw::EtwProvider;
using etw::EventDescriptor;
using etw::EventMetadata;
using etw::Field;

/*
Note: Below should be run from an admin prompt.

For simple testing, use "logman" to create a trace for this provider via:

  logman create trace -n chakra -o chakra.etl -p {57277741-3638-4A4B-BDBA-0AC6E45DA56C}

After the provider GUID, you can optionally specificy keywords and level, e.g.

  -p {57277741-3638-4A4B-BDBA-0AC6E45DA56C} 0xBEEF 0x05

To capture events, start/stop the trace via:

  logman start chakra
  logman stop chakra

When finished recording, remove the configured trace via:

  logman delete chakra

Alternatively, use a tool such as PerfView or WPR to configure and record
traces.
*/

// {57277741-3638-4A4B-BDBA-0AC6E45DA56C}
constexpr GUID chakra_provider_guid = {0x57277741, 0x3638, 0x4A4B,
  {0xBD, 0xBA, 0x0A, 0xC6, 0xE4, 0x5D, 0xA5, 0x6C}};
constexpr char chakra_provider_name[] = "Microsoft-JScript";

class ChakraEtwProvider : public EtwProvider {
 public:
  static ChakraEtwProvider& GetProvider();

  void SourceLoad(
    uint64_t source_id,
    void *script_context_id,
    uint32_t source_flags,
    const std::wstring& url);

  void MethodLoad(
    void *script_context_id,
    void *method_start_address,
    uint64_t method_size,
    uint32_t method_id,
    uint16_t method_flags,
    uint16_t method_address_range_id,
    uint64_t source_id,
    uint32_t line,
    uint32_t column,
    const std::wstring& method_name);

  // TODO(billti) SourceUnload & MethodUnload

 private:
  ChakraEtwProvider();
};

inline void ChakraEtwProvider::SourceLoad(
    uint64_t source_id,
    void *script_context_id,
    uint32_t source_flags,
    const std::wstring& url) {
  constexpr static auto event_desc = EventDescriptor(
    41,  // EventId
    etw::kLevelInfo,
    1,   // JScriptRuntimeKeyword
    12,  // SourceLoadOpcode
    2);  // ScriptContextRuntimeTask
  constexpr static auto event_meta = EventMetadata("SourceLoad",
      Field("SourceID", etw::kTypeUInt64),
      Field("ScriptContextID", etw::kTypePointer),
      Field("SourceFlags", etw::kTypeUInt32),
      Field("Url", etw::kTypeUnicodeStr));

  LogEventData(&event_desc, &event_meta,
      source_id, script_context_id, source_flags, url);
}

inline void ChakraEtwProvider::MethodLoad(
    void *script_context_id,
    void *method_start_address,
    uint64_t method_size,
    uint32_t method_id,
    uint16_t method_flags,
    uint16_t method_address_range_id,
    uint64_t source_id,
    uint32_t line,
    uint32_t column,
    const std::wstring& method_name) {
  constexpr static auto event_desc = EventDescriptor(
    9,   // EventId
    etw::kLevelInfo,
    1,   // JScriptRuntimeKeyword
    10,  // MethodLoadOpcode
    1);  // MethodRuntimeTask
  constexpr static auto event_meta = EventMetadata("MethodLoad",
      Field("ScriptContextID", etw::kTypePointer),
      Field("MethodStartAddress", etw::kTypePointer),
      Field("MethodSize", etw::kTypeUInt64),
      Field("MethodID", etw::kTypeUInt32),
      Field("MethodFlags", etw::kTypeUInt16),
      Field("MethodAddressRangeID", etw::kTypeUInt16),
      Field("SourceID", etw::kTypeUInt64),
      Field("Line", etw::kTypeUInt32),
      Field("Column", etw::kTypeUInt32),
      Field("MethodName", etw::kTypeUnicodeStr));

  LogEventData(&event_desc, &event_meta,
      script_context_id, method_start_address, method_size, method_id,
      method_flags, method_address_range_id, source_id,
      line, column, method_name);
}

}  // namespace chakra
