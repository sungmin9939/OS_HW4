#include "vm/page.h"

#include "userprog/process.h"
#include "userprog/syscall.h"

extern struct lock file_lock;


void page_table_entry_clear(struct page_table_entry * pt_entry)
{
	list_remove (&pt_entry->elem);
	uint8_t * kpage = pagedir_get_page(thread_current()->pagedir, pt_entry->upage);
	pagedir_clear_page (thread_current()->pagedir, pt_entry->upage);
	frame_free_page(kpage);
	free (pt_entry);
}



bool 
load_page_in_page_table(struct page_table_entry * pt_entry)
{
	pt_entry->pinning = true;

	bool load_success = false;

	enum palloc_flags flags = PAL_USER;
	if (pt_entry->read_bytes == 0) {
		flags |= PAL_ZERO;
	}

	uint8_t * new_frame = frame_allocate(flags);

	if (new_frame == NULL) {
		return false;
	}


	if (pt_entry->read_bytes != 0) {
		file_seek(pt_entry->file, pt_entry->offset);

		if (file_read (pt_entry->file, new_frame, pt_entry->read_bytes) != (int) pt_entry->read_bytes)
		{
		  	palloc_free_page (new_frame);
		  	return false; 
		}
		memset (new_frame + pt_entry->read_bytes, 0, pt_entry->zero_bytes);

	}

	if (!install_page (pt_entry->upage, new_frame, pt_entry->writable)) 
	{
	    palloc_free_page (new_frame);
	    return false; 
	}

	pt_entry->is_in_memory = true;
	pt_entry->pinning = false;

	return true;
}

bool
load_page_of_mmap (struct page_table_entry * pt_entry)
{
	bool load_success = false;
	enum palloc_flags flags = PAL_USER|PAL_ZERO;
	void * new_frame = frame_allocate(flags);

	if (new_frame == NULL) {
		return false;
	}

	file_seek(pt_entry->file, pt_entry->offset);
	
	if (file_read (pt_entry->file, new_frame, pt_entry->read_bytes) != (int) pt_entry->read_bytes)
    {
      	palloc_free_page (new_frame);
      	return false; 
    }
	memset (new_frame + pt_entry->read_bytes, 0, pt_entry->zero_bytes);

	if (!install_page (pt_entry->upage, new_frame, pt_entry->writable)) 
	{
	    palloc_free_page (new_frame);
	    return false; 
	}

	pt_entry->is_in_memory = true;

	return true;
}


bool
grow_stack_one_page (void * upage) 
{

	if ( (size_t) (PHYS_BASE - pg_round_down(upage)) > MAX_STACK_SIZE) {
    	return false;
  	}

	struct page_table_entry * pt_entry = malloc(sizeof(struct page_table_entry));

	if (pt_entry == NULL) {
		return false;
	}

	pt_entry->upage = pg_round_down(upage);
	pt_entry->is_in_memory = true;
	pt_entry->writable = true;
	pt_entry->owner = thread_current ();
	pt_entry->type = PAGE_STACK;
	pt_entry->pinning = false;

	uint8_t * new_frame = frame_allocate(PAL_USER);

	if (new_frame == NULL) {
		free(pt_entry);
		return false;
	}

	bool install_success = install_page(pt_entry->upage, new_frame, pt_entry->writable);
	if (install_success == false) 
	{
		free(pt_entry);
		free(new_frame);
		return false;
	}

	struct list * page_table = &thread_current()->page_table;
	struct lock * page_table_lock = &thread_current()->page_table_lock;

	if (!lock_held_by_current_thread(&thread_current()->page_table_lock)) {
		lock_acquire(&thread_current()->page_table_lock);
	}
	list_push_back(page_table, &pt_entry->elem);
	lock_release(page_table_lock);

	return true;
}

bool
is_accessing_stack (void *ptr, uint32_t *esp)
{
  return  ((PHYS_BASE - pg_round_down (ptr)) <= MAX_STACK_SIZE && (uint32_t*)ptr >= (esp - 32));
}
//프로세스 종료시 호출
void
clear_page_table_of_cur_thread()
{
	struct list * page_table = &thread_current()->page_table;

	struct list_elem * e;
	for (e = list_begin(page_table); e != list_end(page_table); e = list_next(e))
	{
		struct page_table_entry * pt_entry = list_entry(e, struct page_table_entry, elem);
		
		frame_free_page(pagedir_get_page(thread_current()->pagedir, pt_entry->upage));
		pagedir_clear_page(thread_current()->pagedir, pt_entry->upage);

	}
}