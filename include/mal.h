#pragma once

// #include <assert.h>
// #include <stdio.h>
// #include <string.h>

#define ENABLE_SPLIT
#define ALIGN 32

int *memory;
char *end;
struct BlockMeta *global_base;

struct BlockMeta {
	size_t size;
	struct BlockMeta *next;
	int free;
};

int mem_init(int size);
char *realign(char **val);
struct BlockMeta *find_free_block(struct BlockMeta **last, size_t size);
struct BlockMeta *request_space(struct BlockMeta* last, size_t size);
void *mal(size_t size);
void memsplit(struct BlockMeta *block, size_t size);
struct BlockMeta *get_block_ptr(void *ptr);
void fr(void *ptr);
int merge_blocks();
