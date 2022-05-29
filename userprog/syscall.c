#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/off_t.h"

struct file
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };
static void syscall_handler (struct intr_frame *);
void check_user_vaddr(const void *vaddr);
struct lock filesys_lock;

void check_user_vaddr(const void *vaddr) {
  if (!is_user_vaddr(vaddr)) {
    exit(-1);
  }
}
void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_user_vaddr(f->esp + 4);
      exit(*(uint32_t *)(f->esp + 4));
      break;
    case SYS_EXEC:
      check_user_vaddr(f->esp + 4);
      f->eax = exec((const char *)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_WAIT:
      f-> eax = wait((pid_t)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_CREATE:
      check_user_vaddr(f->esp + 4);
      check_user_vaddr(f->esp + 8);
      f->eax = create((const char *)*(uint32_t *)(f->esp + 4), (unsigned)*(uint32_t *)(f->esp + 8));
      break;
    case SYS_REMOVE:
      check_user_vaddr(f->esp + 4);
      f->eax = remove((const char*)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_OPEN:
      check_user_vaddr(f->esp + 4);
      f->eax = open((const char*)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_FILESIZE:
      check_user_vaddr(f->esp + 4);
      f->eax = filesize((int)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_READ:
      check_user_vaddr(f->esp + 4);
      check_user_vaddr(f->esp + 8);
      check_user_vaddr(f->esp + 12);
      f->eax = read((int)*(uint32_t *)(f->esp+4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*((uint32_t *)(f->esp + 12)));
      break;
    case SYS_WRITE:
      f->eax = write((int)*(uint32_t *)(f->esp+4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*((uint32_t *)(f->esp + 12)));
      break;
    case SYS_SEEK:
      check_user_vaddr(f->esp + 4);
      check_user_vaddr(f->esp + 8);
      seek((int)*(uint32_t *)(f->esp + 4), (unsigned)*(uint32_t *)(f->esp + 8));
      break;
    case SYS_TELL:
      check_user_vaddr(f->esp + 4);
      f->eax = tell((int)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_CLOSE:
      check_user_vaddr(f->esp + 4);
      close((int)*(uint32_t *)(f->esp + 4));
      break;
    // mmap, munmap 추가
    case SYS_MMAP:
        //f->eax = mmap ((int) f->esp + 4, (void *) f->esp + 8);
        break;
    case SYS_MUNMAP:
        //munmap((mapid_t) f->esp + 4);
        break;
  }
}
void halt (void) {
  shutdown_power_off();
}

void exit (int status) {
  int i;
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_current() -> exit_status = status;
  for (i = 3; i < 128; i++) {
      if (thread_current()->fd[i] != NULL) {
          close(i);
      }
  }
  thread_exit ();
}

pid_t exec (const char *cmd_line) {
  return process_execute(cmd_line);
}

int wait (pid_t pid) {
  return process_wait(pid);
}

int filesize (int fd) {
  if (thread_current()->fd[fd] == NULL) {
      exit(-1);
  }
  return file_length(thread_current()->fd[fd]);
}

int read (int fd, void* buffer, unsigned size) {
  int i;
  int ret;
  check_user_vaddr(buffer);
  lock_acquire(&filesys_lock);
  if (fd == 0) {
    for (i = 0; i < size; i ++) {
      if (((char *)buffer)[i] == '\0') {
        break;
      }
    }
    ret = i;
  } else if (fd > 2) {
    if (thread_current()->fd[fd] == NULL) {
      exit(-1);
    }
    ret = file_read(thread_current()->fd[fd], buffer, size);
  }
  lock_release(&filesys_lock);
  return ret;
}

int write (int fd, const void *buffer, unsigned size) {

  int ret = -1;
  check_user_vaddr(buffer);
  lock_acquire(&filesys_lock);
  if (fd == 1) {
    putbuf(buffer, size);
    ret = size;
  } else if (fd > 2) {
    if (thread_current()->fd[fd] == NULL) {
      lock_release(&filesys_lock);
      exit(-1);
    }
    if (thread_current()->fd[fd]->deny_write) {
        file_deny_write(thread_current()->fd[fd]);
    }
    ret = file_write(thread_current()->fd[fd], buffer, size);
  }
  lock_release(&filesys_lock);
  return ret;
}

bool create (const char *file, unsigned initial_size) {
  if (file == NULL) {
      exit(-1);
  }
  check_user_vaddr(file);
  return filesys_create(file, initial_size);
}

bool remove (const char *file) {
  if (file == NULL) {
      exit(-1);
  }
  check_user_vaddr(file);
  return filesys_remove(file);
}

int open (const char *file) {
  int i;
  int ret = -1;
  struct file* fp;
  if (file == NULL) {
      exit(-1);
  }
  check_user_vaddr(file);
  lock_acquire(&filesys_lock);
  fp = filesys_open(file);
  if (fp == NULL) {
      ret = -1;
  } else {
    for (i = 3; i < 128; i++) {
      if (thread_current()->fd[i] == NULL) {
        if (strcmp(thread_current()->name, file) == 0) {
            file_deny_write(fp);
        }
        thread_current()->fd[i] = fp;
        ret = i;
        break;
      }
    }
  }
  lock_release(&filesys_lock);
  return ret;
}

void seek (int fd, unsigned position) {
  if (thread_current()->fd[fd] == NULL) {
    exit(-1);
  }
  file_seek(thread_current()->fd[fd], position);
}

unsigned tell (int fd) {
  if (thread_current()->fd[fd] == NULL) {
    exit(-1);
  }
  return file_tell(thread_current()->fd[fd]);
}

void close (int fd) {
  struct file* fp;
  if (thread_current()->fd[fd] == NULL) {
    exit(-1);
  }
  fp = thread_current()->fd[fd];
  thread_current()->fd[fd] = NULL;
  return file_close(fp);
}


// // mmap, munmap 추가
// mapid_t 
// mmap (int fd, void * upage)
// {
//   struct file * f = get_file(fd);
//   uint32_t read_bytes = file_length (f);
//   int num_of_page = read_bytes / PGSIZE + 1;

//   int i;
//   thread_current()->map_id++;
//   mapid_t id = thread_current()->map_id;

//   off_t offset = 0;

//   while (read_bytes > 0)
//   {
//     size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
//     size_t page_zero_bytes = PGSIZE - page_read_bytes;
//     struct page_table_entry * pt_entry = malloc(sizeof(struct page_table_entry));

//     pt_entry->file = f;
//     pt_entry->offset = offset;
//     pt_entry->upage = upage;
//     pt_entry->read_bytes = page_read_bytes;
//     pt_entry->zero_bytes = page_zero_bytes;
//     pt_entry->writable = true;
//     pt_entry->type = PAGE_MMAP;
//     pt_entry->is_in_memory = false;
//     pt_entry->owner = thread_current();
//     struct list * page_table = &thread_current()->page_table;
//     struct lock * page_table_lock = &thread_current()->page_table_lock;

//     lock_acquire(page_table_lock);
//     list_push_back(page_table, &pt_entry->elem);
//     lock_release(page_table_lock);

//     if(add_to_mmap_file_list(s_pt_entry) == false)
//     {
//       munmap(id);
//       return -1;
//     }

//     /* Advance. */
//     read_bytes -= page_read_bytes;
//     upage += PGSIZE;
//     offset += PGSIZE;
//   }

//   return id;
// }

// void munmap(mapid_t mapping)
// {

//   struct list * map_list = &(thread_current()->mmap_file_list);
//   struct list_elem * e = list_begin(map_list);

//   for(e = list_begin(map_list); e != list_end(map_list); e = e)
//   {
//     struct map_item * item = list_entry(e, struct map_item, elem);

//     if(item->map_id == mapping)
//     {
      
//       struct page_table_entry * s_pt_entry = item->s_pt_entry;
//       file_seek(s_pt_entry->file, 0);

//       page_table_entry_clear(s_pt_entry);

//       e = list_remove(&item->elem);
//       free(item);
//     }
//     else 
//     {
//       e = list_next(e);
//     }

//   }

//   thread_current()->map_id--;
// }
