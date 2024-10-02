/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

#include "bitmap.h"
#include <round.h>
#include "threads/mmu.h"

#define PGSIZE (1 << 12)
struct bitmap *swap_table;

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1,1);
	uint32_t sector_n = disk_size(swap_disk) / 8;
	swap_table = bitmap_create(sector_n);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->disk_slot = 0;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	// printf("swap_in:\n");
	struct anon_page *anon_page = &page->anon;
	for (int i = 0; i < 8; i++){
		disk_read(swap_disk, (8 * (anon_page->disk_slot)) + i, kva + (DISK_SECTOR_SIZE * i));
	}
	bitmap_reset(swap_table, anon_page->disk_slot);

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	disk_sector_t empty_sector = bitmap_scan (swap_table, 0, 1, false);
	// printf("swap_out: %d\n", empty_sector);

	for (int i = 0; i < 8; i++){
		disk_write(swap_disk, (8 * empty_sector) + i, page->frame->kva + (DISK_SECTOR_SIZE * i));
	}

	anon_page->disk_slot = empty_sector;
	bitmap_mark(swap_table, empty_sector);
	// pml4_set_accessed(thread_current()->pml4,page->va,false);
	pml4_clear_page(thread_current()->pml4, page->va);
	// page->frame = NULL;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	// free(page->anon->aux);
	// free(anon_page);
}
