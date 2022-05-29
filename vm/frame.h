#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"
#include "threads/synch.h"

struct list ft_list;

struct frame_table_entry {
	struct thread * owner;
	void * page;
	struct list_elem elem;
};

void * frame_allocate (enum palloc_flags flags);
void frame_free_page(void *)
void evict ();
struct page_table_entry * ft_entry_to_pt_entry(struct frame_table_entry * );
struct frame_table_entry * select_frame_to_evict ();
#endif