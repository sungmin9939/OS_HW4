#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "vm/frame.h"

#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

#define MAX_STACK_SIZE (1 << 23)
#define STACK_INDICATOR 0xbfff7f80

// page types
#define PAGE_CODE 1
#define PAGE_IN_SWAP 2
#define PAGE_MMAP 3
#define PAGE_STACK 4
#define PAGE_FILE 5

struct page_table_entry
{
	// list
	struct list_elem elem;

	// for load_file_to_page_table
	struct file * file;
	off_t offset;
	void * upage;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;
	bool is_in_memory;

	struct thread * owner;
	// type
	uint8_t type;

	bool pinning;

};


void page_table_clear(struct thread*t);



bool load_page_in_page_table(struct page_table_entry * );

bool grow_stack_one_page(void *);



#endif /* vm/page.h */