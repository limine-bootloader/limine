#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <mm/pmm.h>

static void * sbrk(size_t s) {
	return ext_mem_alloc_type_aligned(s, MEMMAP_BOOTLOADER_RECLAIMABLE, 4096);
}

#if defined(__x86_64__) || defined(__aarch64__) || defined(__riscv64)
#define NUM_BINS 10U								/* Number of bins, total, under 64-bit. */
#define SMALLEST_BIN_LOG 3U							/* Logarithm base two of the smallest bin: log_2(sizeof(int32)). */
#else
#define NUM_BINS 11U								/* Number of bins, total, under 32-bit. */
#define SMALLEST_BIN_LOG 2U							/* Logarithm base two of the smallest bin: log_2(sizeof(int32)). */
#endif
#define BIG_BIN (NUM_BINS - 1)						/* Index for the big bin, (NUM_BINS - 1) */
#define SMALLEST_BIN (1UL << SMALLEST_BIN_LOG)		/* Size of the smallest bin. */

#define PAGE_SIZE 0x1000							/* Size of a page (in bytes), should be 4KB */
#define PAGE_MASK (PAGE_SIZE - 1)					/* Block mask, size of a page * number of pages - 1. */

#define BIN_MAGIC 0xDEFAD00D

static void * __attribute__ ((malloc)) klmalloc(uintptr_t size);
static void * __attribute__ ((malloc)) klrealloc(void * ptr, uintptr_t size);
static void * __attribute__ ((malloc)) klcalloc(uintptr_t nmemb, uintptr_t size);
static void klfree(void * ptr);

void * __attribute__ ((malloc)) malloc(uintptr_t size) {
	return klmalloc(size);
}

void * __attribute__ ((malloc)) realloc(void * ptr, uintptr_t size) {
	return klrealloc(ptr, size);
}

void * __attribute__ ((malloc)) calloc(uintptr_t nmemb, uintptr_t size) {
	return klcalloc(nmemb, size);
}

void free(void * ptr) {
	klfree(ptr);
}

static inline uintptr_t __attribute__ ((always_inline, pure)) klmalloc_adjust_bin(uintptr_t bin)
{
	if (bin <= (uintptr_t)SMALLEST_BIN_LOG)
	{
		return 0;
	}
	bin -= SMALLEST_BIN_LOG + 1;
	if (bin > (uintptr_t)BIG_BIN) {
		return BIG_BIN;
	}
	return bin;
}

static inline uintptr_t __attribute__ ((always_inline, pure)) klmalloc_bin_size(uintptr_t size) {
	uintptr_t bin = sizeof(size) * 8 - __builtin_clzl(size);
	bin += !!(size & (size - 1));
	return klmalloc_adjust_bin(bin);
}

typedef struct _klmalloc_bin_header {
	struct _klmalloc_bin_header *  next;	/* Pointer to the next node. */
	void * head;							/* Head of this bin. */
	uintptr_t size;							/* Size of this bin, if big; otherwise bin index. */
	uint32_t bin_magic;
} klmalloc_bin_header;

typedef struct _klmalloc_bin_header_head {
	klmalloc_bin_header * first;
} klmalloc_bin_header_head;

static klmalloc_bin_header_head klmalloc_bin_head[NUM_BINS - 1];	/* Small bins */

static inline void __attribute__ ((always_inline)) klmalloc_list_decouple(klmalloc_bin_header_head *head, klmalloc_bin_header *node) {
	klmalloc_bin_header *next	= node->next;
	head->first = next;
	node->next = NULL;
}

static inline void __attribute__ ((always_inline)) klmalloc_list_insert(klmalloc_bin_header_head *head, klmalloc_bin_header *node) {
	node->next = head->first;
	head->first = node;
}

static inline klmalloc_bin_header * __attribute__ ((always_inline)) klmalloc_list_head(klmalloc_bin_header_head *head) {
	return head->first;
}

static void * klmalloc_stack_pop(klmalloc_bin_header *header) {
	void *item = header->head;
	uintptr_t **head = header->head;
	uintptr_t *next = *head;
	header->head = next;
	return item;
}

static void klmalloc_stack_push(klmalloc_bin_header *header, void *ptr) {
	uintptr_t **item = (uintptr_t **)ptr;
	*item = (uintptr_t *)header->head;
	header->head = item;
}

static inline int __attribute__ ((always_inline)) klmalloc_stack_empty(klmalloc_bin_header *header) {
	return header->head == NULL;
}

static void * __attribute__ ((malloc)) klmalloc(uintptr_t size) {
	if (__builtin_expect(size == 0, 0))
		return NULL;
	unsigned int bucket_id = klmalloc_bin_size(size);

	if (bucket_id < BIG_BIN) {
		klmalloc_bin_header * bin_header = klmalloc_list_head(&klmalloc_bin_head[bucket_id]);
		if (!bin_header) {
			bin_header = (klmalloc_bin_header*)sbrk(PAGE_SIZE);
			bin_header->bin_magic = BIN_MAGIC;
			bin_header->head = (void*)((uintptr_t)bin_header + sizeof(klmalloc_bin_header));
			klmalloc_list_insert(&klmalloc_bin_head[bucket_id], bin_header);
			uintptr_t adj = SMALLEST_BIN_LOG + bucket_id;
			uintptr_t i, available = ((PAGE_SIZE - sizeof(klmalloc_bin_header)) >> adj) - 1;

			uintptr_t **base = bin_header->head;
			for (i = 0; i < available; ++i) {
				base[i << bucket_id] = (uintptr_t *)&base[(i + 1) << bucket_id];
			}
			base[available << bucket_id] = NULL;
			bin_header->size = bucket_id;
		}
		uintptr_t ** item = klmalloc_stack_pop(bin_header);
		if (klmalloc_stack_empty(bin_header)) {
			klmalloc_list_decouple(&(klmalloc_bin_head[bucket_id]),bin_header);
		}
		return item;
	} else {
		uintptr_t pages = (size + sizeof(klmalloc_bin_header)) / PAGE_SIZE + 1;
		klmalloc_bin_header * bin_header = (klmalloc_bin_header*)sbrk(PAGE_SIZE * pages);
		bin_header->bin_magic = BIN_MAGIC;
		bin_header->size = pages * PAGE_SIZE - sizeof(klmalloc_bin_header);
		bin_header->head = NULL;
		return (void*)((uintptr_t)bin_header + sizeof(klmalloc_bin_header));
	}
}

static void klfree(void *ptr) {
	if (__builtin_expect(ptr == NULL, 0)) {
		return;
	}

	if ((uintptr_t)ptr % PAGE_SIZE == 0) {
		ptr = (void *)((uintptr_t)ptr - 1);
	}

	klmalloc_bin_header * header = (klmalloc_bin_header *)((uintptr_t)ptr & (uintptr_t)~PAGE_MASK);

	if (header->bin_magic != BIN_MAGIC)
		return;

	uintptr_t bucket_id = header->size;
	if (bucket_id > (uintptr_t)NUM_BINS) {
		bucket_id = BIG_BIN;
		klmalloc_bin_header *bheader = (klmalloc_bin_header*)header;
		pmm_free(header, bheader->size + sizeof(klmalloc_bin_header));
	} else {
		if (klmalloc_stack_empty(header)) {
			klmalloc_list_insert(&klmalloc_bin_head[bucket_id], header);
		}
		klmalloc_stack_push(header, ptr);
	}
}

static void * __attribute__ ((malloc)) klrealloc(void *ptr, uintptr_t size) {
	if (__builtin_expect(ptr == NULL, 0))
		return klmalloc(size);

	if (__builtin_expect(size == 0, 0))
	{
		free(ptr);
		return NULL;
	}

	klmalloc_bin_header * header_old = (void *)((uintptr_t)ptr & (uintptr_t)~PAGE_MASK);
	if (header_old->bin_magic != BIN_MAGIC) {
		return NULL;
	}

	uintptr_t old_size = header_old->size;
	if (old_size < (uintptr_t)BIG_BIN) {
		old_size = (1UL << (SMALLEST_BIN_LOG + old_size));
	}

	if (old_size == size) return ptr;

	void * newptr = klmalloc(size);
	if (__builtin_expect(newptr != NULL, 1)) {
		memcpy(newptr, ptr, (old_size < size) ? old_size : size);
		klfree(ptr);
		return newptr;
	}

	return NULL;
}

static void * __attribute__ ((malloc)) klcalloc(uintptr_t nmemb, uintptr_t size) {
	void *ptr = klmalloc(nmemb * size);
	if (ptr) memset(ptr,0x00,nmemb * size);
	return ptr;
}
