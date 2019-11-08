// Copyright 2019 Bill Ticehurst. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides constants and a base class to use in the implementation of an ETW
// provider. To use, derive a class from EtwProvider which constructs the base
// class with the GUID and name for the provider, e.g.
//
//    ExampleEtwProvider::ExampleEtwProvider()
//        : EtwProvider(example_provider_guid, example_provider_name) {}
//
// To log events, implement member functions that define the necessary event
// metadata, and then call the LogEventData member function with the metadata
// and field values, e.g.
//
//    void ExampleEtwProvider::Log3Fields(int val, const std::string& msg, void* addr)
//    {
//      constexpr static auto event_desc = EventDescriptor(100);
//      constexpr static auto event_meta = EventMetadata("my1stEvent",
//          Field("MyIntVal", etw::kTypeInt32),
//          Field("MyMsg", etw::kTypeAnsiStr),
//          Field("Address", etw::kTypePointer));
//
//      LogEventData(&event_desc, &event_meta, val, msg, addr);
//    }

#pragma once

#include <Windows.h>
#include <evntprov.h>

#include <string>
#include <vector>

#include "./etw-metadata.h"

namespace etw {

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
const UCHAR kTypeUnicodeStr = 1;  // WCHARs (wchar_t on Windows)
const UCHAR kTypeAnsiStr    = 2;  // CHARs  (char on Windows)
const UCHAR kTypeInt8       = 3;
const UCHAR kTypeUInt8      = 4;
const UCHAR kTypeInt16      = 5;
const UCHAR kTypeUInt16     = 6;
const UCHAR kTypeInt32      = 7;
const UCHAR kTypeUInt32     = 8;
const UCHAR kTypeInt64      = 9;
const UCHAR kTypeUInt64     = 10;
const UCHAR kTypeFloat      = 11;
const UCHAR kTypeDouble     = 12;
const UCHAR kTypeBool32     = 13;

const UCHAR kTypeHexInt32 = 20;
const UCHAR kTypeHexInt64 = 21;
const UCHAR kTypePointer = (sizeof(void*) == 8) ? kTypeHexInt64 : kTypeHexInt32;

// All "manifest-free" events should go to channel 11 by default
const UCHAR kManifestFreeChannel = 11;

// Creates a constexpr EVENT_DESCRIPTOR structure for use with ETW calls
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

class EtwProvider {
 public:
  // An event provider should be a singleton in a process. Disable copy/move.
  EtwProvider(const EtwProvider&) = delete;
  EtwProvider& operator=(const EtwProvider&) = delete;

// GCC/Clang supported builtin for branch hints
#if defined(__GNUC__)
#define LIKELY(condition) (__builtin_expect(!!(condition), 1))
#else
#define LIKELY(condition) (condition)
#endif

  // For use by this class before calling EventWrite
  bool IsEventEnabled(const EVENT_DESCRIPTOR* event_desc) {
    if (LIKELY(this->enabled_ == false)) return false;
    return (event_desc->Level <= this->level_) &&
           (event_desc->Keyword == 0 ||
            ((event_desc->Keyword & this->keywords_) != 0));
  }

  // For use by user-code before constructing event data
  bool IsEventEnabled(UCHAR level, ULONGLONG keywords = 0) {
    if (LIKELY(this->enabled_ == false)) return false;
    return (level <= this->level_) &&
           (keywords == 0 || ((keywords & this->keywords_) != 0));
  }

#undef LIKELY

  void SetMetaDescriptors(EVENT_DATA_DESCRIPTOR* data_descriptor,
                          const void* metadata,
                          size_t size) {
    // Note: May be able to just set the name on the provider if only Win10 or
    // later can be supported. See the docs for EventSetInformation and
    // https://docs.microsoft.com/en-us/windows/win32/etw/provider-traits
    EventDataDescCreate(data_descriptor, traits_.data(), traits_.size());
    data_descriptor->Type = EVENT_DATA_DESCRIPTOR_TYPE_PROVIDER_METADATA;
    ++data_descriptor;
    EventDataDescCreate(data_descriptor, metadata, static_cast<ULONG>(size));
    data_descriptor->Type = EVENT_DATA_DESCRIPTOR_TYPE_EVENT_METADATA;
  }

  ULONG LogEvent(const EVENT_DESCRIPTOR* event_descriptor,
                 EVENT_DATA_DESCRIPTOR* data_descriptor, ULONG desc_count) {
    if (reg_handle_ == 0) return ERROR_SUCCESS;
    return EventWriteTransfer(reg_handle_, event_descriptor,
                              NULL /* ActivityId */,
                              NULL /* RelatedActivityId */,
                              desc_count,
                              data_descriptor);
  }

  // One or more fields to set
  template <typename T, typename... Ts>
  void SetFieldDescriptors(EVENT_DATA_DESCRIPTOR *data_descriptors,
                           const T& value, const Ts&... rest) {
    EventDataDescCreate(data_descriptors, &value, sizeof(value));
    SetFieldDescriptors(++data_descriptors, rest...);
  }

  // Specialize for strings
  template <typename... Ts>
  void SetFieldDescriptors(EVENT_DATA_DESCRIPTOR *data_descriptors,
                           const std::string& value, const Ts&... rest) {
    EventDataDescCreate(data_descriptors, value.data(),
                        static_cast<ULONG>(value.size() + 1));
    SetFieldDescriptors(++data_descriptors, rest...);
  }

  template <typename... Ts>
  void SetFieldDescriptors(EVENT_DATA_DESCRIPTOR *data_descriptors,
                           const std::wstring& value, const Ts&... rest) {
    EventDataDescCreate(data_descriptors, value.data(),
                        static_cast<ULONG>(value.size() * 2 + 2));
    SetFieldDescriptors(++data_descriptors, rest...);
  }

  // Base case, no fields left to set
  void SetFieldDescriptors(EVENT_DATA_DESCRIPTOR *data_descriptors) {}

  // Template LogEvent used to simplify call
  template <typename T, typename... Fs>
  void LogEventData(const EVENT_DESCRIPTOR* event_descriptor, T* meta,
                    const Fs&... fields) {
    if (!IsEventEnabled(event_descriptor)) return;

    const size_t descriptor_count = sizeof...(fields) + 2;
    EVENT_DATA_DESCRIPTOR descriptors[sizeof...(fields) + 2];

    SetMetaDescriptors(descriptors, meta->bytes, meta->size);

    EVENT_DATA_DESCRIPTOR *data_descriptors = descriptors + 2;
    SetFieldDescriptors(data_descriptors, fields...);

    LogEvent(event_descriptor, descriptors, descriptor_count);
  }

  // Called whenever the the state of providers listening changes.
  // Also called immediately on registering if there is already a listener.
  static void NTAPI EnableCallback(
      const GUID *source_id,
      ULONG is_enabled,
      UCHAR level,  // Is 0xFF if not specified by the session
      ULONGLONG match_any_keyword,  // 0xFF...FF if not specified by the session
      ULONGLONG match_all_keyword,
      EVENT_FILTER_DESCRIPTOR *filter_data,
      VOID *callback_context) {
    if (callback_context == nullptr) return;
    EtwProvider* the_provider = static_cast<EtwProvider*>(callback_context);
    switch (is_enabled) {
      case 0:  // EVENT_CONTROL_CODE_DISABLE_PROVIDER
        the_provider->enabled_ = false;
        break;
      case 1:  // EVENT_CONTROL_CODE_ENABLE_PROVIDER
        the_provider->enabled_ = true;
        the_provider->level_ = level;
        the_provider->keywords_ = match_any_keyword;
        break;
    }
  }

  bool enabled() { return enabled_; }
  void set_enabled(bool value) { enabled_ = value; }  // For testing only

 protected:
  // All creation/deletion should be via derived classes, hence protected.
  EtwProvider(const GUID& provider_guid, const std::string& provider_name)
      : enabled_(false),
        provider_(provider_guid),
        name_(provider_name),
        reg_handle_(0),
        level_(0),
        keywords_(0) {
    ULONG result =
        EventRegister(&provider_,
                      EtwProvider::EnableCallback, this, &reg_handle_);
    if (result != ERROR_SUCCESS) {
      // Note: Fail silenty here, rather than throw. Tracing is typically not
      // critical, and this means no exception support is needed.
      reg_handle_ = 0;
      return;
    }

    // Copy the provider name, prefixed by a UINT16 length, to a buffer.
    // The string in the buffer should be null terminated.
    // See https://docs.microsoft.com/en-us/windows/win32/etw/provider-traits
    size_t traits_bytes = sizeof(UINT16) + name_.size() + 1;
    traits_.resize(traits_bytes, '\0');  // Trailing byte will already be null
    *reinterpret_cast<UINT16*>(traits_.data()) = traits_bytes;
    name_.copy(traits_.data() + sizeof(UINT16), name_.size(), 0);
  }

  ~EtwProvider() {
    if (reg_handle_ != 0) EventUnregister(reg_handle_);
  }

 private:
  bool enabled_;
  const GUID provider_;
  const std::string name_;
  REGHANDLE reg_handle_;
  std::vector<char> traits_;
  UCHAR level_;
  ULONGLONG keywords_;
};

}  // namespace etw
