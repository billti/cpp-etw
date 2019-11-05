#pragma once

#include "etw-events.h"

namespace example {

/*
Note: Below should be run from an admin prompt.

For simple testing, use "logman" to create a trace for this provider via:

  logman create trace -n example -o example.etl -p {f0c59bc0-7da6-58c1-b1b0-e97dd10ac324}

To capture events, start/stop the trace via:

  logman start example
  logman stop example

When finished recording, remove the configured trace via:

  logman delete example

Alternatively, use a tool such as PerfView or WPR to configure and record
traces.
*/

// {f0c59bc0-7da6-58c1-b1b0-e97dd10ac324}
constexpr GUID example_provider_guid = {
    0xf0c59bc0,
    0x7da6,
    0x58c1,
    {0xb1,0xb0,0xe9,0x7d,0xd1,0x0a,0xc3,0x24}};
constexpr char example_provider_name[] = "example";

using namespace ::etw;

class ExampleEtwProvider : public EtwEvents {
 public:
  static ExampleEtwProvider* GetProvider();
  void Initialized();
  void StartSort(INT32 element_count);
  void StopSort();
  void Finished(INT32 total_elements);
  void Log3Fields(INT32 val, const std::string& msg, void* addr);

 private:
  ExampleEtwProvider();
};

// For minimal overhead in instrumented code, make the functions inline to avoid
// a call.
inline void ExampleEtwProvider::Initialized() {
  constexpr static auto event_desc = EventDescriptor(101, kLevelInfo);
  constexpr static auto event_meta = EventMetadata("Initialized");

  LogEventData(&event_desc, &event_meta);
}

inline void ExampleEtwProvider::StartSort(INT32 element_count) {
  constexpr static auto event_desc =
      EventDescriptor(102, kLevelInfo, 0 /*keyword*/, kOpCodeStart);
  constexpr static auto event_meta =
      EventMetadata("StartSort", Field("element_count", kTypeInt32));

  LogEventData(&event_desc, &event_meta, element_count);
}

inline void ExampleEtwProvider::StopSort() {
  constexpr static auto event_desc =
      EventDescriptor(103, kLevelInfo, 0, kOpCodeStop);
  constexpr static auto event_meta = EventMetadata("StopSort");

  LogEventData(&event_desc, &event_meta);
}

inline void ExampleEtwProvider::Finished(INT32 total_elements) {
  constexpr static auto event_desc = EventDescriptor(104, kLevelInfo);
  constexpr static auto event_meta =
      EventMetadata("Finished", Field("element_count", kTypeInt32));

  LogEventData(&event_desc, &event_meta, total_elements);
}

}  // namespace example
