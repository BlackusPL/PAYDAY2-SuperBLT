//
// Created by HW12Dev on 27/07/2026
//

#include "convert.h"

#include "util/util.h"

#include <diesel/font.h>

struct FontHeader
{
	size_t glyphs_size;
	size_t glyphs_capacity;
	size_t glyphs_data;
	size_t glyphs_allocator;
	// there's more, but above is all we need to determine bitness
};

std::vector<uint8_t> ConvertFont(std::vector<uint8_t>&& data, const std::string& path)
{
	if (data.size() < sizeof(FontHeader))
		return data;

	FontHeader* header = (FontHeader*)data.data();

	if (header->glyphs_allocator == 0 && header->glyphs_data != 0)
	{
		return data;
	}

	diesel::AngelCodeFont font;
	Reader reader((char*)data.data(), data.size(), false);

	if (!font.Read(reader, diesel::DieselFormatsLoadingParameters(diesel::EngineVersion::PAYDAY_2_LATEST,
	                                                              diesel::Renderer::UNSPECIFIED,
	                                                              diesel::FileSourcePlatform::WINDOWS_32)))
	{
		char msg[512];
		snprintf(msg, sizeof(msg), "Error occurred while reading 32bit Font, is the file corrupt? File: %s",
		         path.c_str());
		RAIDHOOK_LOG_LOG(msg);

		return data;
	}

	reader.Close();

	
	// Now write it back out to our data vector

	Writer writer;
	MemoryWriterContainer* container = (MemoryWriterContainer*)writer.GetContainer();

	font.Write(writer,
	         diesel::DieselFormatsLoadingParameters(diesel::EngineVersion::DIESEL_V3, diesel::Renderer::UNSPECIFIED,
	                                                diesel::FileSourcePlatform::WINDOWS_64));

	writer.Close();

	// Nasty bodge, I'm sure this is undefined behaviour but it will work here :)
	std::vector<char> signedData = container->TakeData();
	std::vector<uint8_t>* aliasingViolationLivesHere = (std::vector<uint8_t>*)&signedData;
	std::vector<uint8_t> unsignedData = std::move(*aliasingViolationLivesHere);

	return unsignedData;
}