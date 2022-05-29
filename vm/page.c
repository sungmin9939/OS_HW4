#include "vm/page.h"

#include "userprog/process.h"
#include "userprog/syscall.h"


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

	uint8_t * new_frame = frame_allocate(PAL_USER);
	
	file_seek(pt_entry->file, pt_entry->offset);
	file_read (pt_entry->file, new_frame, pt_entry->read_bytes)
	memset (new_frame + pt_entry->read_bytes, 0, pt_entry->zero_bytes);

	install_page (pt_entry->upage, new_frame, pt_entry->writable)
	pt_entry->is_in_memory = true;
	pt_entry->pinning = false;

	return true;
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