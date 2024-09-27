/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "lib/kernel/hash.h"
#include "../include/threads/mmu.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
int page_hash(struct hash_elem* e);
struct list frame_table;
bool page_less(const struct hash_elem* a, const struct hash_elem* b, void *aux);

void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
	
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */

enum vm_type
page_get_type (struct page *page) {
	// printf("page_get_type");
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */

bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	// if (!spt->hash) printf("is spt null? ");
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */		
		/* TODO: Insert the page into the spt. */
		struct page *p = (struct page *)malloc(sizeof(struct page));

		if (p == NULL) 
		{
			printf("page Null?");
			return false;
		}
		// bool (*page_initializer)(struct page *, enum vm_type, void *);
		
		switch (VM_TYPE(type))
		{
		case VM_ANON:{
			uninit_new(p, upage, init, type, aux, anon_initializer);
			}
			break;
		case VM_FILE:{
			uninit_new(p, upage, init, type, aux, file_backed_initializer);
			}
			break;
		default:{
			printf("VM type is trash");
			free(p);
			return false;
			}
		}

		p->wrt = writable;
		bool a = spt_insert_page(spt, p);
		return a;
	}



err:
	printf("alloc - error");
	return false;
}

struct page *
page_lookup (struct hash *h UNUSED, const void *address) {
  struct page p;
  struct hash_elem *e;

  p.va = address;
  e = hash_find (h, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	// printf("spt_find_page");
	struct page *page = NULL;
	/* TODO: Fill this function. */
	// if (spt == NULL || va == NULL) {
	// 	printf("spt or va %d is null", va);
    //     return NULL;
    // }
	// struct page temp_page;
	// temp_page.va = va;
	// struct hash_elem* e = hash_find(&spt->hash, &temp_page.hash_elem);
	// if (e != NULL)
	// {
	// 	page = hash_entry(e,struct page, hash_elem);
	// }
	page = page_lookup(&spt->hash, va);

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	// printf("spt_insert_page");
	int succ = false;
	/* TODO: Fill this function. */

	struct hash hash = spt->hash;
	struct hash_elem* hash_elem = &page->hash_elem;
	if (!hash_insert(&hash, hash_elem)) // 성공시(중복데이터가 없을 시) Null을 반환하는 독특한 친구
	{
		succ = true;
	}
	return succ;
}
/* Insert PAGE into spt with validation. */


void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	// printf("page_get_type");
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {

	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/

static struct frame *
vm_get_frame (void) {
	// 왜 말록일까?
	// printf("get frame");
	struct frame *frame = (struct frame*)malloc(sizeof(struct frame));
	
	/* TODO: Fill this function. */
	// palloc_get_page(PAL_USER);
	// 방출 어케하지
	//여기서 방출을 해야되네? -> 시계 알고리즘?
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);

	if (frame->kva == NULL) PANIC("todo"); // 이곳이 eviction 처리하는곳

	// list_push_back(&frame_table, &frame->frame_elem); // 이거 맞나?

	
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {


}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {

}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	// printf("try handle fault");
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (!not_present)
	{
		return false;
	}

	if(!is_user_vaddr (addr)){
		return false;
	}

	page = spt_find_page(spt, pg_round_down(addr));

	if (page == NULL) return false;

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {

	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	// printf("claim page");
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct supplemental_page_table * spt = &thread_current()->spt;
	page = spt_find_page(spt, va);

	if (page == NULL)
	{
		printf("claim page - is page Null?");
		return false;
	}

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	// printf("do claim page");
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->wrt);
	// thread_current()->pml4;
	// page->va;
	// frame->kva;

	return swap_in (page, frame->kva);
}

int page_hash(struct hash_elem* e)
{	
	// 해쉬엘렘에서 해당 페이지 불러와서
	struct page* p = hash_entry(e, struct page, hash_elem);
	// va를 hash_bytes를 사용해서 변환
	return hash_bytes(&p->va, sizeof(p->va));
}

bool page_less(const struct hash_elem* a, const struct hash_elem* b, void *aux)
{
	struct page* pa = hash_entry(a, struct page, hash_elem);
	struct page* pb = hash_entry(b, struct page, hash_elem);
	// 걍 크기비교

	return pa->va < pb->va;
}


/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {


	hash_init(&spt->hash, page_hash, page_less, NULL);// aux에 어떤값을 넣을까? 0넣고 있었네 ㅋㅋ

}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	
	struct hash old = src->hash;
	dst->hash = old;

}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */

	
	spt_find_page
	destroy();
}



