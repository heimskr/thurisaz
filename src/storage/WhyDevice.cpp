#include "Kernel.h"
#include "storage/WhyDevice.h"

WhyDevice::WhyDevice(long index_): index(index_) {}

ssize_t WhyDevice::read(void *buffer, size_t bytes, size_t byte_offset) {
	// We're going to be messing with the argument registers, so we have to save the arguments into something else in
	// case the arguments are optimized to live only in the argument registers or something like that.
	void * const buffer_copy = buffer;
	const size_t bytes_copy = bytes, byte_offset_copy = byte_offset;

	long e0, r0;

	asm("%0 -> $a1 \n %1 -> $a2" :: "r"(index), "r"(byte_offset_copy));
	asm("<io seekabs>");
	asm("$e0 -> %0" : "=r"(e0));
	if (e0 != 0)
		return -e0;

	asm("%0 -> $a1 \n %1 -> $a2 \n %2 -> $a3" :: "r"(index), "r"(buffer_copy), "r"(bytes_copy));
	asm("<io read>");
	asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));

	return e0 != 0? -e0 : r0;
}

ssize_t WhyDevice::write(const void *buffer, size_t bytes, size_t byte_offset) {
	// Same here.
	const void * const buffer_copy = buffer;
	const size_t bytes_copy = bytes, byte_offset_copy = byte_offset;

	long e0, r0;

	asm("%0 -> $a1 \n %1 -> $a2" :: "r"(index), "r"(byte_offset_copy));
	asm("<io seekabs>");
	asm("$e0 -> %0" : "=r"(e0));
	if (e0 != 0)
		return -e0;

	asm("%0 -> $a1 \n %1 -> $a2 \n %2 -> $a3" :: "r"(index), "r"(buffer_copy), "r"(bytes_copy));
	asm("<io write>");
	asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));

	return e0 != 0? -e0 : r0;
}

long WhyDevice::size() const {
	long a0, a1, r0, e0;
	asm("$a0 -> %0 \n $a1 -> %1" : "=r"(a0), "=r"(a1));
	asm("%0 -> $a1" :: "r"(index));
	asm("<io getsize>");
	asm("%0 -> $a0 \n %1 -> $a1" :: "r"(a0), "r"(a1));
	asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));
	if (e0 != 0)
		Kernel::panicf("getsize failed: %ld", e0);
	return r0;
}

size_t WhyDevice::count() {
	long a0, r0;
	asm("$a0 -> %0" : "=r"(a0));
	asm("<io devcount>");
	asm("$r0 -> %0" : "=r"(r0));
	asm("%0 -> $a0" :: "r"(a0));
	return r0;
}

bool WhyDevice::operator==(const StorageDevice &other) const {
	if (this == &other)
		return true;
	if (const WhyDevice *other_why_device = dynamic_cast<const WhyDevice *>(&other))
		return index == other_why_device->index;
	return false;
}
