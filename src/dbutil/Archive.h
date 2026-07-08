//
// Created by Campbell on 7/07/2026.
//

#pragma once

#include <string>
#include <windows.h>

class BLTAbstractDataStore;

// A wrapper class that matches the MSVC STL C++ string class
// https://github.com/microsoft/STL/blob/c430582bc114/stl/inc/xstring#L490
// Note this is a copy-sensitive class!
class PDString
{
  public:
	static void FromCXX(PDString& out, std::string str)
	{
	}

  private:
	union
	{
		char* data;
		char ssoStorage[16];
	} storage = {};

	size_t _Mysize = 0; // current length of string (size)
	size_t _Myres = 0; // current storage reserved for string (capacity)
};
static_assert(sizeof(PDString) == 32, "PDString is the wrong size!");

// Approximate structure from IDA, zero idea how accurate it is, but we've got the important ones here.
struct Archive
{
	PDString name;

	uint64_t position;
	uint64_t length;
	uint64_t probablyReadCounter;

	bool probablyNotLoadedFlag;
	uint8_t _padding57[7];

	uint64_t maybeCompressedSize;

	RTL_CRITICAL_SECTION lock;

	BLTAbstractDataStore* datastore;
	int datastoreRefCountId;

	static Archive* Constructor(Archive* archive, const std::string& name, BLTAbstractDataStore* datastore, int64_t pos,
	                            int64_t len, bool probablyNotLoadedFlag = false);
};
