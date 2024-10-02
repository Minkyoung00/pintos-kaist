/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
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
	// printf("\n\nalloc page addr: %p\n\n", upage);
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page* new_page = malloc(sizeof(struct page));

		if (VM_TYPE(type) == VM_ANON){
			uninit_new(new_page, upage, init, type, aux, anon_initializer);
		}
		else {
			uninit_new(new_page, upage, init, type, aux, file_backed_initializer);
		}
		
		/* TODO: Insert the page into the spt. */
		new_page->writable = writable;
		if (spt_insert_page(spt, new_page)){
			return true;
		};
	}
err:
	return false;
}

/* Returns the page containing the given virtual address, or a null pointer if no such page exists. */
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
	struct page *page = NULL;
	/* TODO: Fill this function. */
	page = page_lookup(&spt->hash_table, pg_round_down(va));

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	if (hash_insert (&spt->hash_table, &page->hash_elem) == NULL){
		succ = true;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->hash_table, &page->hash_elem);
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	struct hash_iterator i;
	hash_first (&i, &thread_current()->spt.hash_table);
	while (hash_next (&i)) {
		struct page *p = hash_entry (hash_cur (&i), struct page, hash_elem);
		if (!pml4_is_accessed(thread_current()->pml4, p->va) == 0) {
			return p->frame;
		}
		else{
			pml4_set_accessed(thread_current()->pml4, p->va, false);
		}
	}
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	// printf("victim->page: %p\n",victim->page);
	if (swap_out(victim->page))
		return victim;
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	void *kpage = palloc_get_page(PAL_USER);
	if (kpage){
		frame = malloc(sizeof(struct frame));
		frame->kva = kpage;
		frame->page = NULL;
	}
	else{
		// PANIC ("todo");
		frame = vm_evict_frame();
		// frame->kva = kpage;
		frame->page = NULL;
	}

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	void *stack_bottom = addr - PGSIZE;
	// if (!is_user_vaddr (stack_bottom)) {
	// 	thread_current()->exit_code = -1;
	// 	thread_exit();
	// }
	
	while (stack_bottom != thread_current()->stack_bottom){
		// printf("stack_bottom: %p\n",stack_bottom);
		vm_alloc_page_with_initializer(VM_ANON | VM_MARKER_0, stack_bottom, true, NULL, NULL);
		vm_claim_page(stack_bottom);
		stack_bottom += PGSIZE;
		// thread_current()->rsp = addr;
	} 
	thread_current()->stack_bottom = stack_bottom;
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

#define is_stack_addr(vaddr) ((uint64_t)(vaddr) <= USER_STACK && (uint64_t)(vaddr) >= USER_STACK-256*PGSIZE)
/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// printf("\n\npage: %p\n\n", pg_round_down(addr));
	page = spt_find_page(spt, addr);
	if (page == NULL) 
	{
		uintptr_t cur_rsp;
		if (user)
			cur_rsp = f->rsp;
		else
			cur_rsp = thread_current()->rsp;

		// printf("cur_rsp: %p\n", cur_rsp);

		if (write && is_stack_addr(addr) && addr <= cur_rsp){
		// if (write && user && USER_STACK > addr  && addr > USER_STACK-256*PGSIZE){
		// if (write && user && is_user_vaddr(addr) && is_user_vaddr(pg_round_down(addr))){
		// if (write && user && is_user_vaddr(addr) && is_user_vaddr(pg_round_up(addr)-PGSIZE)){
			vm_stack_growth(pg_round_up(addr));
			return true;
		}
		return false; 
	}
	else{
		// printf("%d\n", spt_find_page(&thread_current()->spt, page->va)->operations->type & (VM_ANON | VM_MARKER_0));
		/* 읽기 시도이고 page writable이 false면 */
		if(write && !page->writable) return false;
	}
	
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
	struct page *page = NULL;
	/* TODO: Fill this function */
	// page = malloc(sizeof(struct page));
	// page->va = va;
	page = spt_find_page(&thread_current()->spt, va);

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	// if (is_user_vaddr (page->va))
	pml4_set_page(thread_current()->pml4, page->va,frame->kva,page->writable);
	// else {
	// 	thread_current()->exit_code = -1;
	// 	thread_exit();
	// }

	// struct supplemental_page_table *spt = &thread_current()->spt;
	// spt_insert_page(spt,page);
	// printf("alloc page: %p\n", page->va);
	return swap_in (page, frame->kva);
}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry (p_, struct page, hash_elem);
	return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
	const struct page *a = hash_entry (a_, struct page, hash_elem);
	const struct page *b = hash_entry (b_, struct page, hash_elem);

	return a->va < b->va;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->hash_table, page_hash, page_less, NULL);
}
struct info_binary{
	struct file *file;
	off_t ofs;
	uint8_t *upage;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;
};
/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct hash_iterator i;
	hash_first (&i, &src->hash_table);
	while (hash_next (&i)) {
		struct page *p = hash_entry (hash_cur (&i), struct page, hash_elem);
		enum vm_type type = VM_TYPE(p->operations->type);
		void *upage = p->va;
		bool writable = p->writable;

		if (type == VM_ANON || type == VM_FILE){
			// vm_alloc_page_with_initializer(page_get_type(p), upage, writable, NULL, NULL);
			vm_alloc_page_with_initializer(p->uninit.type, upage, writable, NULL, NULL);
			/* for pass fork */
			struct page *find_page = spt_find_page(dst, upage);
			vm_do_claim_page(find_page);
			memcpy(find_page->frame->kva, p->frame->kva, PGSIZE);
		}
		else{
			vm_initializer *init = p->uninit.init;
			void *aux = p->uninit.aux;

			void *info = malloc(sizeof(struct info_binary));
			memcpy(info, aux, sizeof(struct info_binary));

			vm_alloc_page_with_initializer(p->uninit.type, upage, writable, init, info);
			// spt_insert_page(dst, p);
		}
	}
	return true;
}


/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// struct hash_iterator i;
	// hash_first (&i, &spt->hash_table);
	// while (hash_next (&i)) {
	// 	struct page *p = hash_entry (hash_cur (&i), struct page, hash_elem);
	// 	hash_destroy()
	// 	if (!spt_insert_page(dst, p)) 
	// 		return false;
	// }
	/*===============================================*/
	struct hash *h = &spt->hash_table;
	// hash_clear(h, NULL);
	size_t i;

	for (i = 0; i < h->bucket_cnt; i++) {
		struct list *bucket = &h->buckets[i];

		while (!list_empty (bucket)) {
			struct list_elem *list_elem = list_pop_front (bucket);
			struct hash_elem *hash_elem = list_entry(list_elem, struct hash_elem, list_elem);
			struct page *page = hash_entry(hash_elem, struct page, hash_elem);

			// enum vm_type type = VM_TYPE(page->operations->type);
			// printf("page->frame->kva: %p\n",page->frame->kva );
			vm_dealloc_page(page);
			// (page->operations->destroy)(page);
			
			// free(page);
		}

		list_init (bucket);
	}
	// printf("thread_name: %s\n", thread_current()->name);
	// printf("function table_kill\n");

	h->elem_cnt = 0;

	return ;
}
