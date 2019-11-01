#pragma once

#include <Windows.h>
#include <evntprov.h>

#include <string>

#include "etw-metadata.h"

namespace v8 {
namespace etw {

// All "manifest-free" events should go to channel 11 by default
const UCHAR kManifestFreeChannel = 11;

// Taken from the TRACE_LEVEL_* macros in <evntrace.h>
enum EtwLevel : UCHAR {
	kLevelNone    = 0,
	kLevelFatal   = 1,
	kLevelError   = 2,
	kLevelWarning = 3,
	kLevelInfo    = 4,
	kLevelVerbose = 5
};

// Taken from the EVENT_TRACE_TYPE_* macros in <evntrace.h>
enum EtwOpCode : UCHAR {
	kOpCodeInfo  = 0,
	kOpCodeStart = 1,
	kOpCodeStop  = 2
};

// See "enum TlgIn_t" in <TraceLoggingProvider.h>
enum EtwFieldType : UCHAR {
	kTypeAnsiStr = 2,
	kTypeInt8    = 3,
	kTypeInt32   = 7,
	kTypeDouble  = 12,
	kTypePointer = 21
};

class EtwEvents {
public:
	EtwEvents(const GUID& provider_guid, const std::string& provider_name) :
			provider(provider_guid), name(provider_name), 
			reg_handle(0), traits_size(0), provider_traits(nullptr),
			is_enabled(false), current_level(0), current_keywords(0) {
		ULONG result = EventRegister(&provider, EtwEvents::EnableCallback, this, &reg_handle);
		if (result != ERROR_SUCCESS) {
			// TODO: Should this fail silently somehow? ETW isn't critical.
			throw new std::exception("Failed to register provider");
		}

		// Copy the provider name, prefixed by a UINT16 length, to a buffer.
		// The string in the buffer should be null terminated.
		// See https://docs.microsoft.com/en-us/windows/win32/etw/provider-traits
		traits_size = static_cast<UINT16>(sizeof(UINT16) + name.size() + 1);
		provider_traits = new char[traits_size];
		*reinterpret_cast<UINT16*>(provider_traits) = traits_size;
		name.copy(provider_traits + sizeof(UINT16), name.size(), 0);
		provider_traits[traits_size - 1] = 0x00;
	}

	~EtwEvents() {
		EventUnregister(reg_handle);
		reg_handle = 0;
		delete[] provider_traits;
		provider_traits = nullptr;
		traits_size = 0;
	}

	// For use by this class before calling EventWrite
	bool IsEventEnabled(const EVENT_DESCRIPTOR* pEventDesc) {
		if (reg_handle == 0 || this->is_enabled == false) return false;
		return (pEventDesc->Level <= this->current_level) && 
			   (pEventDesc->Keyword == 0 || ((pEventDesc->Keyword & this->current_keywords) != 0));
	}

	// For use by user-code before constructing event data
	bool IsEventEnabled(UCHAR level, ULONGLONG keywords) {
		if (reg_handle == 0 || this->is_enabled == false) return false;
		return (level <= this->current_level) &&
			(keywords == 0 || ((keywords & this->current_keywords) != 0));
	}

	void SetMetaDescriptors(EVENT_DATA_DESCRIPTOR* pDesc, const void* pMetadata, size_t size) {
		EventDataDescCreate(pDesc, provider_traits, traits_size);
		pDesc->Type = EVENT_DATA_DESCRIPTOR_TYPE_PROVIDER_METADATA;
		++pDesc;
		EventDataDescCreate(pDesc, pMetadata, static_cast<ULONG>(size));
		pDesc->Type = EVENT_DATA_DESCRIPTOR_TYPE_EVENT_METADATA;
	}

	ULONG LogEvent(const EVENT_DESCRIPTOR* pEventDesc, 
			EVENT_DATA_DESCRIPTOR* pDataDesc, ULONG desc_count) {
		if (reg_handle == 0) return ERROR_SUCCESS;
		return EventWriteTransfer(reg_handle, pEventDesc,
			NULL /* ActivityId */, NULL /* RelatedActivityId */, 
			desc_count, pDataDesc);
	}

	// One or more fields to set
	template<typename T, typename... Ts>
	void SetFieldDescriptors(PEVENT_DATA_DESCRIPTOR pFields, const T& val, const Ts&... rest) {
		EventDataDescCreate(pFields, &val, sizeof val);
		SetFieldDescriptors(++pFields, rest...);
	}

	// Specialize for strings
	template<typename... Ts>
	void SetFieldDescriptors(PEVENT_DATA_DESCRIPTOR pFields, const std::string& val, const Ts&... rest) {
		EventDataDescCreate(pFields, val.data(), static_cast<ULONG>(val.size() + 1));
		SetFieldDescriptors(++pFields, rest...);
	}

	// Base case, no fields left to set
	void SetFieldDescriptors(PEVENT_DATA_DESCRIPTOR pFields) {}

	// Template LogEvent used to simplify call
	template<typename T, typename... Fs>
	void LogEventData(const EVENT_DESCRIPTOR* p_event_desc, T* meta, const Fs&... fields) {
		if (!IsEventEnabled(p_event_desc)) return;

		const size_t desc_count = sizeof...(fields) + 2;
		EVENT_DATA_DESCRIPTOR descriptors[sizeof...(fields) + 2];

		SetMetaDescriptors(descriptors, meta->bytes, meta->size);

		PEVENT_DATA_DESCRIPTOR pFields = descriptors + 2;
		SetFieldDescriptors(pFields, fields...);

		LogEvent(p_event_desc, descriptors, desc_count);
	}

	// See https://docs.microsoft.com/en-us/windows/win32/api/evntprov/nc-evntprov-penablecallback
	static void NTAPI EnableCallback(
			LPCGUID SourceId,
			ULONG IsEnabled,
			UCHAR Level,                // Is 0xFF if not specified by the session
			ULONGLONG MatchAnyKeyword,  // Is 0xFF...FF if not specified by the session
			ULONGLONG MatchAllKeyword,
			PEVENT_FILTER_DESCRIPTOR FilterData,
			PVOID CallbackContext) {
		if (CallbackContext == nullptr) return;
		EtwEvents* p_this = static_cast<EtwEvents*>(CallbackContext);
		switch (IsEnabled) {
		  case 0: // EVENT_CONTROL_CODE_DISABLE_PROVIDER
			p_this->is_enabled = false;
			break;
		  case 1: // EVENT_CONTROL_CODE_ENABLE_PROVIDER
			p_this->is_enabled = true;
			p_this->current_level = Level;
			p_this->current_keywords = MatchAnyKeyword;
			break;
		}
	}

	bool is_enabled; // Here temporarily to manipulate for testing. Make private after.
private:
	const GUID provider;
	const std::string name;
	REGHANDLE reg_handle;
	char* provider_traits;
	UINT16 traits_size;
	UCHAR current_level;
	ULONGLONG current_keywords;
};

constexpr auto EventDescriptor(USHORT id, UCHAR level = 0, ULONGLONG keyword = 0, UCHAR opcode = 0, USHORT task = 0) {
	return EVENT_DESCRIPTOR{
		id,
		0,  // Version
		kManifestFreeChannel,
		level,
		opcode,
		0,   // Task
		keyword
	};
}

} // namespace etw
} // namespace v8
