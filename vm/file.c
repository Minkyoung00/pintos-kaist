/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "../include/threads/vaddr.h"
#include "../include/userprog/process.h"
#include "../include/threads/mmu.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	// printf("HI3\n\n");
	page->operations = &file_ops;
	struct info_binary *info = page->uninit.aux;

	struct file_page *file_page = &page->file;
	
	file_page->file = info->file;
	file_page->ofs = info->ofs;
	file_page->read_bytes = info->read_bytes;
	file_page->zero_bytes = info->zero_bytes;
	file_page->info = info;

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
	// 할당된 페이지들 보내주고
	// 수정사항 다시 써주고
	// dirty = 쓰기한적 있을떄 1인 pte
	struct file *f = file_page->info->file;
	struct info_binary *aux = file_page->info;

	if (pml4_is_dirty(thread_current()->pml4, page->va))
	{	

		file_write_at(aux->file, page->frame->kva, aux->read_bytes, aux->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}
	// printf("HI\n\n");

	pml4_clear_page(thread_current()->pml4, page->va);
	// file_close(file_page->file);
}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset)
{

	void *start_addr = addr;
	struct thread *t = thread_current();
	struct page *page;
	int page_cnt = 1;

	struct file *f = file_reopen(file);

	size_t file_len = file_length(file) - offset;

	while (length > 0)
	{
		// Fail : pages mapped overlaps other existing pages or kernel memory
		// printf("CHECK DO_MMAP... length : %d\n", length);

		if (spt_find_page(&t->spt, addr) != NULL || is_kernel_vaddr(addr))
		{
			void *free_addr = start_addr; // get page from this user vaddr and destroy them
			while (free_addr < addr)
			{
				// free allocated uninit page
				page = spt_find_page(&t->spt, free_addr);
				spt_remove_page(&t->spt, page);

				free_addr += PGSIZE;
			}
			return NULL;
		}
		size_t read_byte = length < file_len ? length : file_len;

		size_t page_read_bytes = read_byte < PGSIZE ? read_byte : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		// 여기부터 수정해야 함...
		struct info_binary *info = (struct info_binary *)malloc(sizeof(struct info_binary));

		info->file = f;
		info->ofs = offset;
		info->read_bytes = page_read_bytes;
		info->zero_bytes = page_zero_bytes;

		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
											writable, lazy_load_segment, info))
			return NULL;

		struct page *p = spt_find_page(&t->spt, start_addr);
		p->mapped_page_count = page_cnt;

		// printf("CHECK DO_MMAP... read_bytes : %d\n", page_read_bytes);
		offset += page_read_bytes;
		file_len -= page_read_bytes;
		length -= length < PGSIZE ? length : PGSIZE;
		addr += PGSIZE;
		page_cnt++;
	}

	return start_addr;
}

/* Do the munmap */
void do_munmap(void *addr)
{

	struct page *p = spt_find_page(&thread_current()->spt, addr);

	int count = p->mapped_page_count;


	for (int i = 0; i < count; i++)
	{
		// printf("1 %d in %d %p \n", i, count, addr);

		p = spt_find_page(&thread_current()->spt, addr);

		p->operations = &file_ops;

		// printf("type is :%d\n", VM_TYPE(p->operations->type));

		if (p != NULL) spt_remove_page(&thread_current()->spt, p);

		addr += PGSIZE;
	}
}
