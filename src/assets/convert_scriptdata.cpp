//
// Created by Campbell on 14/07/2026.
//

#include "convert.h"
#include "subhook.h"
#include "util/util.h"
#include "dbutil/Archive.h"

#include <diesel/modern/scriptdata.h>

struct DslVector
{
	void* allocator;
	char padding[24];
};

struct ScriptdataHeader
{
	DslVector numbers;
	DslVector strings;
	DslVector vector3s;
	DslVector quaternions;
	DslVector idstrings;
	DslVector tables;
};
static_assert(sizeof(ScriptdataHeader) == 192);

struct DslVector32
{
	uint32_t allocator;
	uint32_t padding[3];
};

struct ScriptdataHeader32
{
	DslVector32 numbers;
	DslVector32 strings;
	DslVector32 vector3s;
	DslVector32 quaternions;
	DslVector32 idstrings;
	DslVector32 tables;
};
static_assert(sizeof(ScriptdataHeader32) == 96);

std::vector<uint8_t> ConvertScriptData(std::vector<uint8_t>&& data, const std::string& path)
{
	char msg[100];
	snprintf(msg, sizeof(msg), "Script data: %d bytes", (int)data.size());
	RAIDHOOK_LOG_LOG(msg);

	if (data.size() < sizeof(ScriptdataHeader32))
		return data;

	ScriptdataHeader* header = (ScriptdataHeader*)data.data();

	// Check if this is a 32-bit file.
	//
	// Due to the pointer size differences, it's very likely the allocator pointers (which are null
	// in the files, and IIRC overwritten with an allocator at load time) will overlap with one of the
	// pointer/size values in a 32-bit file.
	if (data.size() >= sizeof(ScriptdataHeader) &&
		(header->numbers.allocator == nullptr && header->strings.allocator == nullptr &&
	    header->vector3s.allocator == nullptr && header->quaternions.allocator == nullptr &&
	    header->idstrings.allocator == nullptr && header->tables.allocator == nullptr))
	{
		return data;
	}

	// Parse the contents in 32-bit format
	diesel::modern::ScriptData sd;
	Reader reader((char*)data.data(), data.size(), false);

	if (!sd.Read(reader, diesel::DieselFormatsLoadingParameters(diesel::EngineVersion::PAYDAY_2_LATEST,
	                                                            diesel::Renderer::UNSPECIFIED,
	                                                            diesel::FileSourcePlatform::WINDOWS_32)))
	{
		char msg[512];
		snprintf(msg, sizeof(msg), "Error occurred while reading 32bit ScriptData, is the file corrupt? File: %s",
		         path.c_str());
		RAIDHOOK_LOG_LOG(msg);

		return data;
	}

	reader.Close();

	// Now write it back out to our data vector

	Writer writer;
	MemoryWriterContainer* container = (MemoryWriterContainer*)writer.GetContainer();

	sd.Write(writer,
	         diesel::DieselFormatsLoadingParameters(diesel::EngineVersion::DIESEL_V3, diesel::Renderer::UNSPECIFIED,
	                                                diesel::FileSourcePlatform::WINDOWS_64));

	writer.Close();

	// Nasty bodge, I'm sure this is undefined behaviour but it will work here :)
	std::vector<char> signedData = container->TakeData();
	std::vector<uint8_t>* aliasingViolationLivesHere = (std::vector<uint8_t>*)&signedData;
	std::vector<uint8_t> unsignedData = std::move(*aliasingViolationLivesHere);

	return unsignedData;
}


static subhook::Hook ScriptSerializer__from_binary_hook;

void* ScriptSerializer__from_binary_h(void* this_, void* lua_arg_result, const PDString& data, void* metatable_registry)
{
	std::vector<uint8_t> to_convert(data.begin(), data.end());

	to_convert = ConvertScriptData(std::move(to_convert), "");

	PDString converted;
	converted.set_data((char*)to_convert.data(), to_convert.size());

	ScriptSerializer__from_binary_hook.Remove();
	void* ret = ScriptSerializer__from_binary(this_, lua_arg_result, converted, metatable_registry);
	ScriptSerializer__from_binary_hook.Install();
	return ret;
}

void setup_scriptserializer_hooks()
{
	ScriptSerializer__from_binary_hook.Install(ScriptSerializer__from_binary, (void*)&ScriptSerializer__from_binary_h,
	                                           subhook::HookFlag64BitOffset);
}
