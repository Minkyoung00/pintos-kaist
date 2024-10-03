/* vm.c: Generic interface for virtual memory objects. */
#include <stdio.h>
#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
// project 3
// #include "kernel/hash.h"
#include "threads/mmu.h"
#include "lib/string.h"
#include "vm/uninit.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{
	// 페이지 구조체를 할당하고 페이지 타입에 맞는 적절한 초기화 함수를 세팅함으로써
	// 새로운 페이지를 초기화를 하고 유저 프로그램으로 제어권을 넘긴다.

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;
	/* Check wheter the upage is already occupied or not. */
	// 페이지가 없으면 만들어줘라.
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *new_page = malloc(sizeof(struct page));

		if (VM_TYPE(type) == VM_ANON)
		{
			uninit_new(new_page, upage, init, type, aux, anon_initializer);
		}
		else
		{
			uninit_new(new_page, upage, init, type, aux, file_backed_initializer);
		}

		/* TODO: Insert the page into the spt. */
		new_page->writable = writable;
		if (spt_insert_page(spt, new_page))
		{
			return true;
		};
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function. */
	page = page_lookup(&spt->hash_table, va);

	return page;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	int succ = false;
	/* TODO: Fill this function. */
	if (hash_insert(&spt->hash_table, &page->hash_elem) == NULL)
	{
		succ = true;
	}
	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */
	// project 3
	// swap_out(victim->page);

	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
// 유저 메모리 풀에서 새로운 물리 메모리 페이지를 가져옴.
// 성공시 프레임 할당, 프레임 구조체 멤버들을 초기화, 해당 프레임 반환.
// 이걸 구현한 뒤 모든 유저 공간 페이지(PALLOC_USER)를 이함수를 통해 할당해야함.
static struct frame *
vm_get_frame(void)
{
	// struct frame *frame = NULL;
	/* TODO: Fill this function. */
	// project 3
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER);
	if (frame->kva)
	{
		// list_push_back(&frame_table, &frame->elem);
		frame->page = NULL;
	}
	else
	{
		// evict 추후 구현 예정
		frame = vm_evict_frame();
		frame->page = NULL;
	}

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

static void
vm_stack_growth(void *addr UNUSED)
{
	addr = pg_round_up(addr);
	void *stack_bottom = addr - PGSIZE;
	// test code
	// void *stack_bottom = addr;
	// int i = 0;
	thread_current()->stack_bottom = pg_round_down(thread_current()->stack_bottom);
	while (stack_bottom < thread_current()->stack_bottom)
	{
		// printf("stack_bottom: %p\n",stack_bottom);
		vm_alloc_page_with_initializer(VM_ANON, stack_bottom, true, NULL, NULL);
		vm_claim_page(stack_bottom);
		stack_bottom += PGSIZE;
		// printf("%d@@@@@@@@@@@@@@\n", i);
		// i += 1;
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

#define is_stack_addr(vaddr) ((uint64_t)(vaddr) <= USER_STACK && (uint64_t)(vaddr) >= USER_STACK - 256 * PGSIZE)

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	page = spt_find_page(spt, pg_round_down(addr));
	if (page == NULL)
	{
		uintptr_t stack_p = thread_current()->stack_p;
		uintptr_t user_stack_p = f->rsp;
		if (user)
		{
			// write해줄 상황이 아니라면 스택을 늘려줄 필요가 없다.
			if (write && is_stack_addr(addr) && addr <= user_stack_p)
			{
				vm_stack_growth(addr);
				return true;
			}
		}
		else
		{
			// write해줄 상황이 아니라면 스택을 늘려줄 필요가 없다.
			if (write && is_stack_addr(addr) && addr <= stack_p)
			{
				vm_stack_growth(addr);
				return true;
			}
		}
		return false;
	}
	else
	{
		/* 읽기 시도이고 page writable이 false면 */
		if (write && !page->writable)
			return false;
	}

	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function */

	page = spt_find_page(&thread_current()->spt, va);

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);

	return swap_in(page, frame->kva);
}

// project 3///////////////////////////////////////////////////////////
unsigned
page_hash(const struct hash_elem *e, void *aux)
{
	struct page *page = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&page->va, sizeof page->va);
}

bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	struct page *page_a = hash_entry(a, struct page, hash_elem);
	struct page *page_b = hash_entry(b, struct page, hash_elem);

	return page_a->va < page_b->va;
}
/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	// project 3
	hash_init(&spt->hash_table, page_hash, page_less, NULL);
}

struct page *
page_lookup(struct hash *h UNUSED, const void *address)
{
	struct page p;
	struct hash_elem *e;

	p.va = address;
	e = hash_find(h, &p.hash_elem);
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}
void hash_page_destroy(struct hash_elem *e, void *aux)
{
	struct page *page = hash_entry(e, struct page, hash_elem);
	destroy(page);
	free(page);
}

///////////////////////////////////////////////////////////////////////

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	// src에서 dst로 spt를 복사해라.
	// 이것은 자식이 부모의 실행 context를 상속할 필요가 있을 때 사용된다(for example.... fork()).
	// src의 spt를 반복하면서 dst의 spt의 엔트리의 정확한 복사본을 만들어라.
	// 초기화되지 않은(uninit) 페이지를 할당하고 그것들을 바로 요청해야 한다.
	// page를 찾는 방법은 hash함수를 사용해서 elem을 찾아보자 -> 깃북 참고.s

	struct hash_iterator i;
	// hash_table을 순회하기 위해 i를 초기화 한다.
	// 그뒤 구조체 i에 저장된 정보들을 쓰면 될듯?
	hash_first(&i, &src->hash_table);
	while (hash_next(&i))
	{
		struct page *page = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type file_type = page->operations->type;

		// VM_TYPE(type) == VM_ANON
		if (file_type == VM_ANON || file_type == VM_FILE)
		{
			// struct frame *frame = vm_get_frame();
			// vm_alloc_page_with_initializer(page->uninit.type, page->va, page->writable, NULL, NULL);
			vm_alloc_page(file_type, page->va, page->writable);

			// hash_insert(&dst->hash_table, hash_cur(&i));
			spt_insert_page(dst, page);
			struct page *son_page = spt_find_page(dst, page->va);
			// 메모리에 적재.
			vm_do_claim_page(son_page);

			// 물리 메모리의 실제 주소를 복사해야 하기 떄문이다.
			// 프레임 주소를 사용하여 복사해야 가상 메모리 페이지에 매핑된 물리 메모리의 실제 데이터가 복사됨.
			memcpy(son_page->frame->kva, page->frame->kva, PGSIZE);
			// memcpy(son_page->va, page->va, PGSIZE);
		}
		else
		{
			// set aux
			void *auxx = malloc(sizeof(struct aux_box));
			memcpy(auxx, page->uninit.aux, sizeof(struct aux_box));

			// spt_insert_page(dst, page);
			// struct page *son_page = spt_find_page(dst, page->va);
			// son_page->uninit.aux = auxx;
			vm_alloc_page_with_initializer(page->uninit.type, page->va, page->writable, page->uninit.init, auxx);
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->hash_table, hash_page_destroy);
}

// // 페이지가 쓰기 가능한 지 아닌 지 확인하는 함수
// bool is_writable_page(struct page *page)
// {
// 	return (VM_TYPE(page->operations->type) & VM_WRITABLE) != 0;
// }
