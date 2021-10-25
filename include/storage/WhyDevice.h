#include "storage/StorageDevice.h"

struct WhyDevice: StorageDevice {
	const int index;
	WhyDevice(int index_);
	ssize_t read(void *buffer, size_t bytes, size_t byte_offset) override;
	ssize_t write(const void *buffer, size_t bytes, size_t byte_offset) override;
	static size_t count();
};
