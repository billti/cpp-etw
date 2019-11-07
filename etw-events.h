// Copyright 2019 Bill Ticehurst. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <Windows.h>
#include <evntprov.h>

#include <string>
#include <vector>

#include "etw-metadata.h"

namespace etw {

// All "manifest-free" events should go to channel 11 by default
const UCHAR kManifestFreeChannel = 11;

// Taken from the TRACE_LEVEL_* macros in <evntrace.h>
const UCHAR kLevelNone = 0;
const UCHAR kLevelFatal = 1;
const UCHAR kLevelError = 2;
const UCHAR kLevelWarning = 3;
const UCHAR kLevelInfo = 4;
const UCHAR kLevelVerbose = 5;

// Taken from the EVENT_TRACE_TYPE_* macros in <evntrace.h>
const UCHAR kOpCodeInfo = 0;
const UCHAR kOpCodeStart = 1;
const UCHAR kOpCodeStop = 2;

// See "enum TlgIn_t" in <TraceLoggingProvider.h>
const UCHAR kTypeAnsiStr = 2;
const UCHAR kTypeInt8 = 3;
const UCHAR kTypeInt32 = 7;
const UCHAR kTypeDouble = 12;
const UCHAR kTypePointer = 21;

class EtwEvents {
 public:
  // An event provider should be a singleton in a process. Disable copy/move.
  EtwEvents(const EtwEvents&) = delete;
  EtwEvents& operator=(const EtwEvents&) = delete;

// GCC/Clang supported builtin for branch hints
#if defined(__GNUC__)
#define LIKELY(condition) (__builtin_expect(!!(condition), 1))
#else
#define LIKELY(condition) (condition)
#endif

  // For use by this class before calling EventWrite
  bool IsEventEnabled(const EVENT_DESCRIPTOR* pEventDesc) {
    if (LIKELY(this->is_enabled == false)) return false;
    return (pEventDesc->Level <= this->current_level) &&
           (pEventDesc->Keyword == 0 ||
            ((pEventDesc->Keyword & this->current_keywords) != 0));
  }

  // For use by user-code before constructing event data
  bool IsEventEnabled(UCHAR level, ULONGLONG keywords) {
    if (LIKELY(this->is_enabled == false)) return false;
    return (level <= this->current_level) &&
           (keywords == 0 || ((keywords & this->current_keywords) != 0));
  }

#undef LIKELY

  void SetMetaDescriptors(EVENT_DATA_DESCRIPTOR* pDesc, const void* pMetadata,
                          size_t size) {
    EventDataDescCreate(pDesc, traits.data(), traits.size());
    pDesc->Type = EVENT_DATA_DESCRIPTOR_TYPE_PROVIDER_METADATA;
    ++pDesc;
    EventDataDescCreate(pDesc, pMetadata, static_cast<ULONG>(size));
    pDesc->Type = EVENT_DATA_DESCRIPTOR_TYPE_EVENT_METADATA;
  }

  ULONG LogEvent(const EVENT_DESCRIPTOR* pEventDesc,
                 EVENT_DATA_DESCRIPTOR* pDataDesc, ULONG desc_count) {
    if (reg_handle == 0) return ERROR_SUCCESS;
    return EventWriteTransfer(reg_handle, pEventDesc, NULL /* ActivityId */,
                              NULL /* RelatedActivityId */, desc_count,
                              pDataDesc);
  }

  // One or more fields to set
  template <typename T, typename... Ts>
  void SetFieldDescriptors(PEVENT_DATA_DESCRIPTOR pFields, const T& val,
                           const Ts&... rest) {
    EventDataDescCreate(pFields, &val, sizeof(val));
    SetFieldDescriptors(++pFields, rest...);
  }

  // Specialize for strings
  template <typename... Ts>
  void SetFieldDescriptors(PEVENT_DATA_DESCRIPTOR pFields,
                           const std::string& val, const Ts&... rest) {
    EventDataDescCreate(pFields, val.data(),
                        static_cast<ULONG>(val.size() + 1));
    SetFieldDescriptors(++pFields, rest...);
  }

  // Base case, no fields left to set
  void SetFieldDescriptors(PEVENT_DATA_DESCRIPTOR pFields) {}

  // Template LogEvent used to simplify call
  template <typename T, typename... Fs>
  void LogEventData(const EVENT_DESCRIPTOR* p_event_desc, T* meta,
                    const Fs&... fields) {
    if (!IsEventEnabled(p_event_desc)) return;

    const size_t desc_count = sizeof...(fields) + 2;
    EVENT_DATA_DESCRIPTOR descriptors[sizeof...(fields) + 2];

    SetMetaDescriptors(descriptors, meta->bytes, meta->size);

    PEVENT_DATA_DESCRIPTOR pFields = descriptors + 2;
    SetFieldDescriptors(pFields, fields...);

    LogEvent(p_event_desc, descriptors, desc_count);
  }

  // Called whenever the the state of providers listening changes.
  // Also called immediately on registering if there is already a listener.
  static void NTAPI EnableCallback(
      LPCGUID SourceId, ULONG IsEnabled,
      UCHAR Level,  // Is 0xFF if not specified by the session
      ULONGLONG
          MatchAnyKeyword,  // Is 0xFF...FF if not specified by the session
      ULONGLONG MatchAllKeyword, PEVENT_FILTER_DESCRIPTOR FilterData,
      PVOID CallbackContext) {
    if (CallbackContext == nullptr) return;
    EtwEvents* p_this = static_cast<EtwEvents*>(CallbackContext);
    switch (IsEnabled) {
      case 0:  // EVENT_CONTROL_CODE_DISABLE_PROVIDER
        p_this->is_enabled = false;
        break;
      case 1:  // EVENT_CONTROL_CODE_ENABLE_PROVIDER
        p_this->is_enabled = true;
        p_this->current_level = Level;
        p_this->current_keywords = MatchAnyKeyword;
        break;
    }
  }

  // TODO: Here temporarily to manipulate for testing. Make private after.
  bool is_enabled;

protected:
  // All creation/deletion should be via derived classes
  EtwEvents(const GUID& provider_guid, const std::string& provider_name)
      : is_enabled(false),
        provider(provider_guid),
        name(provider_name),
        reg_handle(0),
        current_level(0),
        current_keywords(0) {
    ULONG result =
        EventRegister(&provider, EtwEvents::EnableCallback, this, &reg_handle);
    if (result != ERROR_SUCCESS) {
      // Note: Fail silenty here, rather than throw. Tracing is typically not
      // critical, and this means no exception support is needed.
      reg_handle = 0;
      return;
    }

    // Copy the provider name, prefixed by a UINT16 length, to a buffer.
    // The string in the buffer should be null terminated.
    // See https://docs.microsoft.com/en-us/windows/win32/etw/provider-traits
    size_t traits_bytes = sizeof(UINT16) + name.size() + 1;
    traits.resize(traits_bytes, '\0');  // Trailing byte will already be null
    *reinterpret_cast<UINT16*>(traits.data()) = traits_bytes;
    name.copy(traits.data() + sizeof(UINT16), name.size(), 0);
  }

  ~EtwEvents() {
    if (reg_handle != 0) EventUnregister(reg_handle);
  }

 private:
  const GUID provider;
  const std::string name;
  REGHANDLE reg_handle;
  std::vector<char> traits;
  UCHAR current_level;
  ULONGLONG current_keywords;
};

constexpr auto EventDescriptor(USHORT id, UCHAR level = 0,
                               ULONGLONG keyword = 0, UCHAR opcode = 0,
                               USHORT task = 0) {
  return EVENT_DESCRIPTOR{id,
                          0,  // Version
                          kManifestFreeChannel,
                          level,
                          opcode,
                          task,
                          keyword};
}

}  // namespace etw
