#pragma once

#include "etw-events.h"

namespace v8 {
namespace etw {

/*
Note: Below should be run from an admin prompt.

For simple testing, use "logman" to create a trace for this provider via:

  logman create trace -n v8js -o v8js.etl -p {c6c2b481-a1d8-5a54-638c-2dd5fd3eec2e}

To capture events, start/stop the trace via:

  logman start v8js
  logman stop v8js

When finished recording, remove the configured trace via:

  logman delete v8js

Alternatively, use a tool such as PerfView or WPR to configure and record traces.
*/

// {c6c2b481-a1d8-5a54-638c-2dd5fd3eec2e}
constexpr GUID v8_provider_guid = { 0xc6c2b481, 0xa1d8, 0x5a54, 0x63, 0x8c, 0x2d, 0xd5, 0xfd, 0x3e, 0xec, 0x2e };
constexpr char v8_provider_name[] = "v8js";

class V8EtwProvider : public EtwEvents {
  public:
	static V8EtwProvider* GetProvider();
	void Initialized();
	void StartSort(INT32 element_count);
	void StopSort();
	void Finished(INT32 total_elements);
	void Log3Fields(INT32 val, const std::string& msg, void* addr);

  private:
	V8EtwProvider();
	static V8EtwProvider* v8_provider;
};

// For minimal overhead in instrumented code, make the functions inline to avoid a call.
inline void V8EtwProvider::Initialized() {
	constexpr static auto event_desc = EventDescriptor(101, kLevelInfo);
	constexpr static auto event_meta = EventMetadata("Initialized");

	LogEventData(&event_desc, &event_meta);
}

inline void V8EtwProvider::StartSort(INT32 element_count) {
	constexpr static auto event_desc = EventDescriptor(102, kLevelInfo, 0 /*keyword*/, kOpCodeStart);
	constexpr static auto event_meta = EventMetadata("StartSort",
		Field("element_count", kTypeInt32));

	LogEventData(&event_desc, &event_meta, element_count);
}

inline void V8EtwProvider::StopSort() {
	constexpr static auto event_desc = EventDescriptor(103, kLevelInfo, 0, kOpCodeStop);
	constexpr static auto event_meta = EventMetadata("StopSort");

	LogEventData(&event_desc, &event_meta);
}

inline void V8EtwProvider::Finished(INT32 total_elements) {
	constexpr static auto event_desc = EventDescriptor(104, kLevelInfo);
	constexpr static auto event_meta = EventMetadata("Finished",
		Field("element_count", kTypeInt32));

	LogEventData(&event_desc, &event_meta, total_elements);
}

} // namespace etw
} // namespace v8
