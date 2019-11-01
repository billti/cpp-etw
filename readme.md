# cpp-etw

This repo demonstrates how to log manifest-free ETW events using only the Win32
APIs and C++, and without the `TraceLoggingProvider.h` header file, which uses
a large number of macros, and various pragmas, declspecs, etc. which may not
work correctly on different compilers.

The	`manifest-free` ETW mechanism consists of using ETW channel 11 to log the
events, and providing a couple of additional data descriptors to describe the
event metadata. (One for the provider info, and one for the event layout).

This metadata must be tightly packed in a specific format, which is
the purpose of the `constexpr` templates in `etw-metadata.h`. This allows for
the declaring of the event metadata (the event name, followed by the field
names and types), using a `constexpr` such as the below:

```cpp
constexpr static auto event_meta = EventMetadata("myEventName",
    Field("MyIntVal", kTypeInt32),
    Field("MyMsg", kTypeAnsiStr),
    Field("Address", kTypePointer));
```

The event descriptor to describe the event id, severity level, flags, etc.
is declared similarly:

```cpp
constexpr static auto event_desc =
    EventDescriptor(102, kLevelInfo, 0 /*keyword*/, kOpCodeStart);
```

The logging of the event uses a variadic template function, thus the field
values are simply provided in order after the metadata. Thus an entire logging
function in a provider class would appear something like the below:

```cpp
void MyProvider::Log3Fields(INT32 val, const std::string& msg, void* addr) {
	constexpr static auto event_desc = EventDescriptor(100, kLevelVerbose);
	constexpr static auto event_meta = EventMetadata("myEventName",
		Field("MyIntVal", kTypeInt32),
		Field("MyMsg", kTypeAnsiStr),
		Field("Address", kTypePointer));

	LogEventData(&event_desc, &event_meta, val, msg, addr);
}
```

As the first 2 lines are `constexpr` variables, these result in no runtime code
and are evaluated entirely at compile time.

The code in `etw-events.h` declares the `EtwEvents` class, which your provider
should derive from. See the example implementation in the `v8-etw.*` files. The
`v8-etw.cc` file also shows code to ensure only a single instance of the ETW
provider is instantiated. (It does not demonstrate unregistering currently).

The `main.cc` file shows running a simple experiment for measuring perf impact.

Comments near the top of `v8-etw.h` indicate how to start/stop a simple trace
from the command line.

## Implementation notes

 - Using only `constexpr auto...` and not `static constexpr auto...` in
   functions results in a large amount of assembly in the function to push
   a representation of the constant data onto the stack. Using `static` results
   in this data being in the read-only segment, with the function simply
   referencing it - which is much cheaper.
 - Enabling tracing of the provider on my Surface Book 2 laptop with a CPU and
   memory bound process, logging around 10,000 ETW events/sec, resulted in
   about a 3 percent increase in execution time. (Impact was not observable if
   there was no session recording the events).
 - Even logging 3 fields plus metadata (as per the `Log3Fields` function shown
   above), which also checks if the provider is enabled first, only added 44
   assembly instructions inline into the instrumented function, none of which
   were loops or calls. Most of this was to prepare the data descriptors for
   the event instance specific data for `EventWrite`, which seems unavoidable.
   If the provider is not enabled (likely most of the time), only 2 to 4
   assembly instructions are executed before jumping over the rest of the
   event logging code.

## TODO

 - Not all field data types have been implemented yet.
 - Add support for activities.
 - Add support for unregistering the provider.
