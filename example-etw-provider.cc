#include "example-etw-provider.h"

namespace example {

ExampleEtwProvider::ExampleEtwProvider()
    : EtwEvents(example_provider_guid, example_provider_name) {}

ExampleEtwProvider* ExampleEtwProvider::GetProvider() {
  static ExampleEtwProvider the_provider{};
  return &the_provider;
}

// Any non-trivial logging should be a separate function call, not an inlined
void ExampleEtwProvider::Log3Fields(INT32 val, const std::string& msg, void* addr) {
  constexpr static auto event_desc = EventDescriptor(100);
  constexpr static auto event_meta = EventMetadata("my1stEvent",
      Field("MyIntVal", kTypeInt32),
      Field("MyMsg", kTypeAnsiStr),
      Field("Address", kTypePointer));

  LogEventData(&event_desc, &event_meta, val, msg, addr);
}

}  // namespace example
