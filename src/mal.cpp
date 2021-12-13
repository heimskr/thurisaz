#include "Kernel.h"
#include "mal.h"
#include "memset.h"
#include "Print.h"
#include "printf.h"

Memory *global_memory = nullptr;
bool mal_debug = false;

// #define DEBUG_ALLOCATION
// #define PROACTIVE_PAGING

Memory::Memory(char *start_, char *high_): start(start_), high(high_), end(start_) {
	start = (char *) realign((uintptr_t) start);
	global_memory = this;
	highestAllocated = reinterpret_cast<uintptr_t>(start_);
}

Memory::Memory(): Memory((char *) 0, (char *) 0) {}

uintptr_t Memory::realign(uintptr_t val, size_t alignment) {
#ifdef DEBUG_ALLOCATION
	if (mal_debug)
		printf("realign(%ld, %ld)\n", val, alignment);
#endif
	if (alignment == 0)
		return val;
	size_t offset = (val + sizeof(BlockMeta)) % alignment;
	if (offset)
		val += alignment - offset;
	return val;
}

Memory::BlockMeta * Memory::findFreeBlock(BlockMeta * &last, size_t size) {
#ifdef DEBUG_ALLOCATION
	if (mal_debug)
		printf("findFreeBlock(%ld, %lu)\n", last, size);
#endif
	BlockMeta *current = base;
	while (current && !(current->free && size <= current->size)) {
		last = current;
		current = current->next;
	}
	return current;
}

Memory::BlockMeta * Memory::requestSpace(BlockMeta *last, size_t size, size_t alignment) {
#ifdef DEBUG_ALLOCATION
	if (mal_debug)
		printf("requestSpace(%ld, %lu)\n", last, size);
#endif
	BlockMeta *block = (BlockMeta *) realign((uintptr_t) end, alignment);

	if (last)
		last->next = block;

#ifdef PROACTIVE_PAGING
	// auto &pager = Kernel::getPager();
	// while (highestAllocated <= (uintptr_t) block + size) {
	// 	pager.assignAddress(reinterpret_cast<void *>(highestAllocated));
	// 	highestAllocated += PAGE_LENGTH;
	// }
#endif

	block->size = size;
	block->next = nullptr;
	block->free = 0;

	end = reinterpret_cast<char *>(block) + block->size + sizeof(BlockMeta) + 1;
	return block;
}

void * Memory::allocate(size_t size, size_t alignment) {
#ifdef DEBUG_ALLOCATION
	if (mal_debug)
		printf("allocate(%lu)\n", size);
#endif
	BlockMeta *block = nullptr;

	if (size <= 0)
		return nullptr;

	if (!base) {
		block = requestSpace(nullptr, size, alignment);
		if (!block)
			return nullptr;
		base = block;
	} else {
		BlockMeta *last = base;
		block = findFreeBlock(last, size);
		if (!block) {
			block = requestSpace(last, size, alignment);
			if (!block)
				return nullptr;
		} else {
			split(*block, size);
			block->free = 0;
		}
	}

	allocated += block->size + sizeof(BlockMeta);
	return block + 1;
}

void Memory::split(BlockMeta &block, size_t size) {
#ifdef DEBUG_ALLOCATION
	if (mal_debug)
		printf("split(%ld, %lu)\n", &block, size);
#endif
	if (block.size > size + sizeof(BlockMeta)) {
		// We have enough space to split the block, unless alignment takes up too much.
		BlockMeta *new_block = (BlockMeta *) realign((uintptr_t) &block + size + sizeof(BlockMeta) + 1);

		// After we realign, we need to make sure that the new block's new size isn't negative.

		if (block.next) {
			const long new_size = (char *) block.next - (char *) new_block - sizeof(BlockMeta);

			// Realigning the new block can make it too small, so we need to make sure the new block is big enough.
			if (new_size > 0) {
				new_block->size = new_size;
				new_block->next = block.next;
				new_block->free = 1;
				block.next = new_block;
				block.size = size;
			}
		} else {
			const long new_size = (char *) &block + block.size - (char *) new_block;

			if (new_size > 0) {
				new_block->size = new_size;
				new_block->free = 1;
				new_block->next = nullptr;
				block.size = size;
				block.next = new_block;
			}
		}
	}
}

Memory::BlockMeta * Memory::getBlock(void *ptr) {
#ifdef DEBUG_ALLOCATION
	if (mal_debug)
		printf("getBlock(%ld)\n", ptr);
#endif
	return (BlockMeta *) ptr - 1;
}

void Memory::free(void *ptr) {
#ifdef DEBUG_ALLOCATION
	if (mal_debug)
		printf("free(%ld)\n", ptr);
#endif
	if (!ptr)
		return;

	BlockMeta *block_ptr = getBlock(ptr);
	block_ptr->free = 1;
	allocated -= block_ptr->size + sizeof(BlockMeta);
	merge();
}

int Memory::merge() {
#ifdef DEBUG_ALLOCATION
	if (mal_debug)
		strprint("merge()\n");
#endif
	int count = 0;
	BlockMeta *current = base;
	while (current && current->next) {
		if (current->free && current->next->free) {
			current->size += sizeof(BlockMeta) + current->next->size;
			current->next = current->next->next;
			++count;
		} else
			current = current->next;
	}

	return count;
}

void Memory::setBounds(char *new_start, char *new_high) {
#ifdef DEBUG_ALLOCATION
	// if (mal_debug)
		printf("setBounds(0x%lx, 0x%lx)\n", new_start, new_high);
#endif
	if (new_high <= new_start)
		Kernel::panicf("Invalid heap bounds: 0x%lx through 0x%lx\n", new_start, new_high);
	start = (char *) realign((uintptr_t) new_start);
	highestAllocated = reinterpret_cast<uintptr_t>(start);
	high = new_high;
	end = new_start;
}

size_t Memory::getAllocated() const {
	return allocated;
}

size_t Memory::getUnallocated() const {
	return high - start - allocated;
}

extern "C" void * malloc(size_t size) {
#ifdef DEBUG_ALLOCATION
	if (mal_debug)
		printf("\e[2mmalloc(%lu)\e[22m\n", size);
#endif
	if (global_memory == nullptr)
		return nullptr;
	return global_memory->allocate(size);
}

extern "C" void * calloc(size_t count, size_t size) {
#ifdef DEBUG_ALLOCATION
	if (mal_debug)
		printf("\e[2mcalloc(%lu, %lu)\e[22m\n", count, size);
#endif
	void *chunk = malloc(count * size);
	if (chunk)
		memset(chunk, 0, count * size);
	return chunk;
}

extern "C" void free(void *ptr) {
	if (global_memory)
		global_memory->free(ptr);
}

extern "C" int posix_memalign(void **memptr, size_t alignment, size_t size) {
	// Return EINVAL if the alignment isn't zero or a power of two or is less than the size of a void pointer.
#ifdef DEBUG_ALLOCATION
	if (mal_debug)
		printf("\e[2mmemalign(%ld)\e[22m\n", memptr);
#endif
	if ((alignment & (alignment - 1)) != 0 || alignment < sizeof(void *)) {
		printf("Bad alignment: %lu\n", alignment);
		return 22; // EINVAL
	}
	if (memptr) {
		*memptr = global_memory? global_memory->allocate(size, alignment) : nullptr;
#ifdef DEBUG_ALLOCATION
		if (mal_debug)
			printf("\e[2m(a=%lu, s=%lu) -> %ld\e[22m\n", alignment, size, *memptr);
#endif
	}
	return 0;
}

static void * checked_malloc(size_t size) {
	void *out = malloc(size);
	if (!out)
		Kernel::panicf("Can't allocate %lu bytes: out of memory", size);
	return out;
}

#ifdef __clang__
void * operator new(size_t size)   { return checked_malloc(size); }
void * operator new[](size_t size) { return checked_malloc(size); }
void * operator new(size_t, void *ptr)   { return ptr; }
void * operator new[](size_t, void *ptr) { return ptr; }
void operator delete(void *ptr)   noexcept { free(ptr); }
void operator delete[](void *ptr) noexcept { free(ptr); }
void operator delete(void *, void *)   noexcept {}
void operator delete[](void *, void *) noexcept {}
void operator delete(void *, unsigned long)   noexcept {}
void operator delete[](void *, unsigned long) noexcept {}
#endif
