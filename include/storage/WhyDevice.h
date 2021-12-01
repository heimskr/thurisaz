#include "storage/StorageDevice.h"

struct WhyDevice: StorageDevice {
	const long index;
	WhyDevice(long index_);
	ssize_t read(void *buffer, size_t bytes, size_t byte_offset) override;
	ssize_t write(const void *buffer, size_t bytes, size_t byte_offset) override;
	long size() const;
	static size_t count();
};
