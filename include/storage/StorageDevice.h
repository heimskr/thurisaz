#pragma once

#include <stddef.h>
#include <sys/types.h>

struct StorageDevice {
	virtual ~StorageDevice() {}
	virtual ssize_t read(void *buffer, size_t bytes, size_t byte_offset) = 0;
	virtual ssize_t write(const void *buffer, size_t bytes, size_t byte_offset) = 0;
	// virtual int clear(size_t offset, size_t size) = 0;
	// virtual std::string getName() const = 0;
	virtual bool operator==(const StorageDevice &) const = 0;
};
