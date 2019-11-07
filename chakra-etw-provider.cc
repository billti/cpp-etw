// Copyright 2019 Bill Ticehurst. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "./chakra-etw-provider.h"

#include <string>

namespace example {

ExampleEtwProvider::ExampleEtwProvider()
    : EtwProvider(example_provider_guid, example_provider_name) {}

ExampleEtwProvider& ExampleEtwProvider::GetProvider() {
  // The below pattern means the destructor will not run at process exit. (Which
  // is unnecessary anyway as the provider will unregister at process exit).
  // See "Static and Global Variables" in https://google.github.io/styleguide/cppguide.html
  static ExampleEtwProvider &the_provider = *(new ExampleEtwProvider());
  return the_provider;
}

// Any non-trivial logging should be a separate function call, not inlined
void ExampleEtwProvider::Log3Fields(int val,
    const std::string& msg, void* addr) {
  constexpr static auto event_desc = EventDescriptor(100);
  constexpr static auto event_meta = EventMetadata("my1stEvent",
      Field("MyIntVal", etw::kTypeInt32),
      Field("MyMsg", etw::kTypeAnsiStr),
      Field("Address", etw::kTypePointer));

  LogEventData(&event_desc, &event_meta, val, msg, addr);
}

}  // namespace example
