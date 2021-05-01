#include "mal.h"

int mem_init(int size) {
	asm("$g -> %0" : "=r"(memory));
	realign((char **) &memory);
	end = (char *) memory;
	global_base = nullptr;
	return !!memory;
}

char * realign(char **val) {
	if ((long) *val % ALIGN)
		*val += ALIGN - ((long) *val % ALIGN);
	return *val;
}

struct BlockMeta * find_free_block(struct BlockMeta **last, size_t size) {
	struct BlockMeta *current = global_base;
	while (current && !(current->free && current->size >= size)) {
		*last = current;
		current = current->next;
	}

	return current;
}

struct BlockMeta * request_space(struct BlockMeta *last, size_t size) {
	struct BlockMeta *block = (struct BlockMeta *) realign(&end);
	if (last)
		last->next = block;
	block->size = size;
	block->next = nullptr;
	block->free = 0;
	end = (char *) block + block->size + sizeof(struct BlockMeta) + 1;
	return block;
}

void * mal(size_t size) {
	struct BlockMeta *block;

	if (size <= 0)
		return nullptr;

	if (!global_base) {
		block = request_space(nullptr, size);
		if (!block)
			return nullptr;
		global_base = block;
	} else {
		struct BlockMeta *last = global_base;
		long old = (long) last - (long) memory;
		block = find_free_block(&last, size);
		if (!block) {
			block = request_space(last, size);
			if (!block)
				return nullptr;
		} else {
			memsplit(block, size);
			block->free = 0;
		}
	}

	return block + 1;
}

void memsplit(struct BlockMeta *block, size_t size) {
	if (block->size > size + sizeof(struct BlockMeta)) {
		// We have enough space to split the block, unless alignment takes up too much.
		struct BlockMeta *new_block = (struct BlockMeta *) ((char *) block + size + sizeof(struct BlockMeta) + 1);
		
		// After we realign, we need to make sure that the new block's new size isn't negative.
		realign((char **) &new_block);

		if (block->next) {
			int new_size = (char *) block->next - (char *) new_block - sizeof(struct BlockMeta);

			// Realigning the new block can make it too small, so we need to make sure the new block is large enough.
			if (new_size > 0) {
				new_block->size = new_size;
				new_block->next = block->next;
				new_block->free = 1;
				block->next = new_block;
				block->size = size;
			}
		} else {
			int new_size = (char *) block + block->size - (char *) new_block;
			if (new_size > 0) {
				new_block->size = new_size;
				new_block->free = 1;
				new_block->next = nullptr;
				block->size = size;
				block->next = new_block;
			}
		}
	}
}

struct BlockMeta * get_block_ptr(void *ptr) {
	return (struct BlockMeta*) ptr - 1;
}

void fr(void *ptr) {
	if (!ptr) {
		return;
	}

	struct BlockMeta *block_ptr = get_block_ptr(ptr);
	assert(block_ptr->free == 0);
	block_ptr->free = 1;
	merge_blocks();
}

int merge_blocks() {
	int count = 0;
	struct BlockMeta *current = global_base;
	while (current && current->next) {
		if (current->free && current->next->free) {
			current->size += sizeof(struct BlockMeta) + current->next->size;
			current->next = current->next->next;
			count++;
		} else {
			current = current->next;
		}
	}
	return count;
}
