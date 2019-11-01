#include "v8-etw.h"

namespace v8 {
namespace etw {

V8EtwProvider::V8EtwProvider() : EtwEvents(v8_provider_guid, v8_provider_name) {}

V8EtwProvider* V8EtwProvider::GetProvider() {
	static V8EtwProvider the_provider{};
	return &the_provider;
}

// Any non-trivial logging should be a separate function call, not an inlined
void V8EtwProvider::Log3Fields(INT32 val, const std::string& msg, void* addr) {
	constexpr static auto event_desc = EventDescriptor(100);
	constexpr static auto event_meta = EventMetadata("my1stEvent",
		Field("MyIntVal", kTypeInt32),
		Field("MyMsg", kTypeAnsiStr),
		Field("Address", kTypePointer));

	LogEventData(&event_desc, &event_meta, val, msg, addr);
}

} // namespace etw
} // namespace v8
