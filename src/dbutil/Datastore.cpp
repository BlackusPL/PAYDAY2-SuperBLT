#include "Datastore.h"
#include "util/util.h"

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <io.h>
#define lseek64 _lseeki64

// BLTAbstractDataStore

size_t BLTAbstractDataStore::write(uint64_t position_in_file, uint8_t const* data, size_t length)
{
	// Writing is unsupported
	RAIDHOOK_LOG_ERROR("BLTAbstractDataStore::write called - writing is not supported!");
	abort();
}

void BLTAbstractDataStore::set_asynchronous_completion_callback(void* /*dsl::LuaRef*/)
{
	RAIDHOOK_LOG_ERROR("BLTAbstractDataStore::set_asynchronous_completion_callback called - async unimplemented!");
	abort();
}

uint64_t BLTAbstractDataStore::state()
{
	RAIDHOOK_LOG_ERROR("BLTAbstractDataStore::state called - unimplemented!");
	abort();
}

// BLTFileDataStore

BLTFileDataStore* BLTFileDataStore::Open(std::string filePath)
{
	int flags = O_RDONLY | O_BINARY;
	int fd = open(filePath.c_str(), flags);

	// Make sure the file opened correctly
	if (fd == -1)
	{
		return nullptr;
	}

	auto obj = new BLTFileDataStore();
	obj->fd = fd;

	int64_t res = lseek64(fd, 0, SEEK_END);
	assert(res != -1);
	obj->file_size = (size_t)res;

	return obj;
}

BLTFileDataStore::~BLTFileDataStore()
{
	::close(fd);
}

size_t BLTFileDataStore::read(uint64_t position_in_file, uint8_t* data, size_t length)
{
	lseek64(fd, position_in_file, SEEK_SET);
	size_t count = ::read(fd, data, length);
	assert(count == length);

	return count;
}

bool BLTFileDataStore::close()
{
	RAIDHOOK_LOG_ERROR("BLTAbstractDataStore::close called - unimplemented!");
	abort();
}

size_t BLTFileDataStore::size() const
{
	return file_size;
}

bool BLTFileDataStore::is_asynchronous() const
{
	// TODO this would probably be good to implement if possible
	return false;
}

bool BLTFileDataStore::good() const
{
	RAIDHOOK_LOG_ERROR("BLTAbstractDataStore::good called - unimplemented!");
	abort();
}

// BLTStringDataStore

BLTStringDataStore::BLTStringDataStore(std::string contents) : contents(std::move(contents))
{
}

size_t BLTStringDataStore::read(uint64_t position_in_file, uint8_t* data, size_t length)
{
	// If the start of the read is past the end, stop here
	if (position_in_file >= contents.size())
		return 0;

	// If the end of the read is past the end, shrink it down so it'll fit
	size_t remaining = contents.size() - position_in_file;
	if (remaining < length)
		length = remaining;

	memcpy(data, contents.c_str() + position_in_file, length);
	return length;
}

bool BLTStringDataStore::close()
{
	RAIDHOOK_LOG_ERROR("BLTStringDataStore::close called - unimplemented!");
	abort();
	// What are we supposed to return?
}

size_t BLTStringDataStore::size() const
{
	return contents.size();
}

bool BLTStringDataStore::is_asynchronous() const
{
	return false;
}

bool BLTStringDataStore::good() const
{
	return true;
}

// BLTFormatConversionDataStore

BLTFormatConversionDataStore::~BLTFormatConversionDataStore()
{
	DeleteUnderlyingDatastore();
}

BLTFormatConversionDataStore::BLTFormatConversionDataStore(ConversionFn&& fn, BLTAbstractDataStore* baseFile,
                                                           int baseFileRefCountId)
	: baseFile(baseFile), baseFileRefCountId(baseFileRefCountId), conversionFn(std::move(fn))
{
}

size_t BLTFormatConversionDataStore::read(uint64_t position_in_file, uint8_t* data, size_t length)
{
	CheckConverted();

	// Past EOF
	if (position_in_file >= convertedData.size())
		return 0;

	// If the end of the read is past the end, shrink it down so it'll fit
	size_t remaining = convertedData.size() - position_in_file;
	if (remaining < length)
		length = remaining;

	memcpy(data, convertedData.data() + position_in_file, length);
	return length;
}

bool BLTFormatConversionDataStore::close()
{
	RAIDHOOK_LOG_ERROR("BLTFormatConversionDataStore::good called - unimplemented!");
	abort();
}

size_t BLTFormatConversionDataStore::size() const
{
	BLTFormatConversionDataStore* mutThis = (BLTFormatConversionDataStore*)this; // Ugly bodge
	mutThis->CheckConverted();
	return convertedData.size();
}

bool BLTFormatConversionDataStore::is_asynchronous() const
{
	return false;
}

bool BLTFormatConversionDataStore::good() const
{
	return true;
}

void BLTFormatConversionDataStore::CheckConverted()
{
	// Do we need a lockless happy path here? No, but I like writing lockless concurrent code :3

	// Acquiring this happens-after the write setting it to true after the conversion
	if (hasConverted.load(std::memory_order_acquire))
		return;

	std::lock_guard guard(lock);

	// Check again under the mutex to avoid races
	if (hasConverted.load(std::memory_order_acquire))
		return;

	// Load the base data
	size_t rawSize = baseFile->size();
	std::vector<uint8_t> rawData(rawSize);
	baseFile->read(0, rawData.data(), rawSize);

	// We don't need the old datastore any more
	DeleteUnderlyingDatastore();

	// Covert the data
	convertedData = conversionFn(std::move(rawData));

	// Release the flag, which happens-after the write to the data array
	hasConverted.store(true, std::memory_order_release);
}

void BLTFormatConversionDataStore::DeleteUnderlyingDatastore()
{
	// Do the same thing as an Archive would
	// Datastores use this big global reference count system. Objects have an ID, which you can then
	// use to increment and decrement their reference count.
	// If we're the last one to use this object - which we almost certainly are - then delete it.

	if (!baseFile)
		return;

	int datastoreRefCount = DecreaseRefCountById(baseFileRefCountId);
	if (datastoreRefCount != 0)
		return;

	using DtorFn = void (*)(void* thisPtr, bool freeMemory);
	void* vtable = *(void***)baseFile;
	DtorFn dtor = *(DtorFn*)vtable;
	dtor(baseFile, true);

	baseFile = nullptr;
	baseFileRefCountId = -1; // If this ever runs again, we hopefully won't get a heisenbug.
}
