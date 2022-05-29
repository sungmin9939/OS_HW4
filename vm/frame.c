#include "vm/frame.h"
#include <stdlib.h>

#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/page.h"

void *
frame_allocate (enum palloc_flags flag) {

	uint8_t * pd = palloc_get_page(flag);

	if (pd != NULL) {
		struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
		fte->page = pd;
		fte->owner = thread_current();
		list_push_back(&ft_list, &fte->elem);
	} else {
		evict();
		pd = palloc_get_page(flag);
		struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
		fte->page = pd;
		fte->owner = thread_current();
		list_push_back(&ft_list, &fte->elem);
	}
	return pd;
}

void frame_free_page(void * page)
{
	
	struct frame_table_entry * frame_to_free  = NULL;
	struct list_elem * e;

	for (e = list_begin(&ft_list); e != list_end(&ft_list);
			e = list_next(e))
	{
		struct frame_table_entry *fte = list_entry(e, struct frame_table_entry, elem);
		if (fte->page == page)
		{
			frame_to_free = fte;
		}
	}
	if (frame_to_free != NULL) {
		list_remove(&frame_to_free->elem);
		palloc_free_page(frame_to_free->page);
	}
}



void
evict() {
	struct frame_table_entry * victim_frame = select_frame_to_evict();

	struct page_table_entry * victim_pt_entry = ft_entry_to_pt_entry(victim_frame);

	while (victim_pt_entry->pinning == true) {
		victim_frame = select_frame_to_evict();
		victim_pt_entry = ft_entry_to_pt_entry(victim_frame);
	}

	victim_pt_entry->pinning = true;
	struct thread * owner_thread = victim_frame->owner;

	pagedir_clear_page(owner_thread->pagedir, victim_pt_entry->upage);

	list_remove(&victim_frame->elem);
	palloc_free_page(victim_frame->page);

	victim_pt_entry->is_in_memory = false;
	victim_pt_entry->type = PAGE_IN_SWAP;
	victim_pt_entry->pinning = false;
}

struct page_table_entry *
ft_entry_to_pt_entry(struct frame_table_entry * ft_entry)
{
	struct page_table_entry * pt_entry;
	struct thread * owner_thread;

	owner_thread = ft_entry->owner;

	struct list * page_table = &owner_thread->page_table;

	struct list_elem * e;

	for (e = list_begin(page_table); e != list_end(page_table); e = list_next(e))
	{
		pt_entry = list_entry(e, struct page_table_entry, elem);
		uint32_t * kpage = pagedir_get_page(owner_thread->pagedir, pt_entry->upage);
		if (kpage == ft_entry->page)
		{
			return pt_entry;
		}
	}
	return NULL;
}

struct frame_table_entry * 
select_frame_to_evict()
{
	struct frame_table_entry * evict_frame;
	return evict_frame;
}