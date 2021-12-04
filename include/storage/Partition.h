#pragma once

#include <stddef.h>
#include <sys/types.h>

struct MBREntry;
struct StorageDevice;

struct Partition {
	std::shared_ptr<StorageDevice> parent;
	/** Number of bytes after the start of the disk. */
	size_t offset;
	/** Length of the partition in bytes. */
	size_t length;

	Partition(std::shared_ptr<StorageDevice> parent_, size_t offset_, size_t length_):
		parent(parent_), offset(offset_), length(length_) {}

	Partition(std::shared_ptr<StorageDevice>, const MBREntry &);

	/** Returns the number of bytes read if successful, or a negative error code otherwise. */
	ssize_t read(void *buffer, size_t size, size_t byte_offset);
	/** Returns the number of bytes written if successful, or a negative error code otherwise. */
	ssize_t write(const void *buffer, size_t size, size_t byte_offset);

	// int clear();

	bool operator==(const Partition &) const;
};
