/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include <string.h>
#include "threads/mmu.h"
#define PGSIZE (1 << 12)

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	file_page->file = NULL;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

	struct info_file *aux = (struct info_file *)page->uninit.aux;
	// printf("try to write\n");
	if (pml4_is_dirty(thread_current()->pml4, page->va)){
		file_write_at(file_page->file, page->va, aux->length, aux->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, false);
	}
	// printf("destroy: %p\n", page->va);

	pml4_clear_page(thread_current()->pml4, page->va);
	// file_close(file_page->file);
}

static bool
lazy_load_file (struct page *page, void *aux) {
	struct info_file *info = (struct info_file *)aux;
	struct file *file = info->file;
	off_t ofs = info->ofs;
	uint32_t page_length = info->length;
	bool writable = info->writable;

	file_seek (file, ofs);

	size_t page_read_bytes = file_read (file, page->va, PGSIZE);
	size_t page_zero_bytes = PGSIZE - page_read_bytes;

	memset (page->va + page_read_bytes, 0, page_zero_bytes);
	page->file.exist_bytes = page_read_bytes;
	page->file.file = file;
	pml4_set_dirty(thread_current()->pml4, page->va, false);
	// printf("is dirty: %d\n", pml4_is_dirty(thread_current()->pml4, page->va));
	// printf("page->file.exist_bytes: %d\n",page->file.exist_bytes);

	return true;
}
/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	struct file *reopen_file = file_reopen(file);
	void *cur_addr = addr;

	for (int i = 0; i < (length + PGSIZE - 1) / PGSIZE; i++){
		// printf("i: %d\n", i);
		struct info_file *info = malloc(sizeof(struct info_file));
		info->file = reopen_file;
		info->ofs = offset;
		info->length = length;
		info->writable = writable;
		info->page_order = i;

		if (!vm_alloc_page_with_initializer (VM_FILE, cur_addr,
					writable, lazy_load_file, info))
			return NULL;
		
		/* Advance. */
		cur_addr += PGSIZE;
		offset += PGSIZE;
	}

	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct page *page = spt_find_page(&thread_current()->spt, addr);
	struct info_file *aux = (struct info_file *)page->uninit.aux;
	void *cur_addr = addr;
	for (int i = 0; i < (aux->length + PGSIZE - 1) / PGSIZE; i++){
		// printf("i: %d\n", i);
		struct page *page = spt_find_page(&thread_current()->spt, cur_addr);
		// file_backed_destroy(page);
		spt_remove_page(&thread_current()->spt, page);
		cur_addr += PGSIZE;
	}

	// file_close(page->file.file);
	// printf("fin munmap\n");
	// struct info_file *aux = (struct info_file *)page->uninit.aux;
	// file_seek(aux->file, aux->ofs);
	// file_write(aux->file, page->file.file, aux->page_length);
}
