#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include <string.h>
#include "threads/palloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <console.h>
#include "devices/input.h"
#include "userprog/process.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	thread_current()->is_user = true;
	// printf("%d",f->R.rax);
	
	// is_valid_mem(f);
	
	switch (f->R.rax) {
	case SYS_HALT:                   /* Halt the operating system. */ 
		power_off();
		break;
	case SYS_EXIT:                   /* Terminate this process. */
	{
		thread_current()->exit_code = f->R.rdi;
		thread_exit();
		break;
	}
	case SYS_FORK:                   /* Clone current process. */
	{
		char *thread_name = f->R.rdi;
		// f->R.rbx = 
		// f->rsp = 
		// f->R.rbp = 
		// f->R.r12 =
		// f->R.r13 =
		// f->R.r14 =
		// f->R.r15 =
		f->R.rax = process_fork(thread_name, f);
		break;
	}
	case SYS_EXEC:                   /* Switch current process. */
		break;
	case SYS_WAIT:                   /* Wait for a child process to die. */
		break;
	case SYS_CREATE:                 /* Create a file. */
	{
		char *file = f->R.rdi; 
		unsigned initial_size = f->R.rsi;

		// if (file == NULL || is_kernel_vaddr(file))
		if (file == NULL)
		{
			thread_current()->exit_code = -1;
			thread_exit();
		}
		else
			f->R.rax = filesys_create(file, initial_size);
		
		break;
	}
	case SYS_REMOVE:                 /* Delete a file. */
		break;

	case SYS_OPEN: {				/* Open a file. */
		char *file_name = f->R.rdi;
		if (file_name == NULL || file_name == "")
		{
			f->R.rax = -1;
			break;
		}

		struct file* open_file = filesys_open(file_name);
		
		if (open_file == NULL)
			f->R.rax = -1;
		else
		{
			int i = 0;
			while(thread_current()->fd_table[i])
				i ++;
			thread_current()->fd_table[i] = open_file;

			f->R.rax = i;
		}
		break;
	}                   
	case SYS_FILESIZE:               /* Obtain a file's size. */
	{
		int fd = f->R.rdi;
		struct file* open_file = thread_current()->fd_table[fd]; 
		f->R.rax = file_length(open_file);
	
		break;
	}

	case SYS_READ:                   /* Read from a file. */
	{
		int fd = f->R.rdi;
		void *buffer = f->R.rsi;
		unsigned size = f->R.rdx;

		if (fd == 0) 
		{
			input_getc();
			f->R.rax = size;
		}
		else
		{
			struct file* read_file = (struct file*)(thread_current()->fd_table[fd]);
			f->R.rax = file_read(read_file, buffer, size);
		}

		break;
	}
	case SYS_WRITE: {                /* Write to a file. */
		int fd = f->R.rdi;
		void *buffer = f->R.rsi;
		unsigned size = f->R.rdx;

		// printf("fd: %d, buffer: %p, size: %d", fd, buffer, size);

		if (fd == 1) 
		{
			putbuf(buffer, size);
			f->R.rax = size;
		}
		else
		{
			char *contents = palloc_get_page (0);
			if (contents == NULL)
				return TID_ERROR;
			strlcpy (contents, (char *)buffer, PGSIZE);

			struct file* write_file = (struct file*)(thread_current()->fd_table[fd]);
			f->R.rax = file_write(write_file, contents, size);
			
			palloc_free_page (contents);
		}
		break;
	}
	case SYS_SEEK:                   /* Change position in a file. */
		break;
	case SYS_TELL:                   /* Report current position in a file. */
		break;
	case SYS_CLOSE:                  /* Close a file. */
	{
		int fd = f->R.rdi;
		struct file* close_file = thread_current()->fd_table[fd]; 
		file_close(close_file);
		thread_current()->fd_table[fd] = NULL;
		break;
	}

	default:
	{
		thread_current()->exit_code = -1;
		thread_exit();
    	break;
	}

	// /* Project 3 and optionally project 4. */
	// SYS_MMAP,                   /* Map a file into memory. */
	// SYS_MUNMAP,                 /* Remove a memory mapping. */

	// /* Project 4 only. */
	// SYS_CHDIR,                  /* Change the current directory. */
	// SYS_MKDIR,                  /* Create a directory. */
	// SYS_READDIR,                /* Reads a directory entry. */
	// SYS_ISDIR,                  /* Tests if a fd represents a directory. */
	// SYS_INUMBER,                /* Returns the inode number for a fd. */
	// SYS_SYMLINK,                /* Returns the inode number for a fd. */

	// /* Extra for Project 2 */
	// SYS_DUP2,                   /* Duplicate the file descriptor */

	// SYS_MOUNT,
	// SYS_UMOUNT,
	}
}

void 
is_valid_mem(struct intr_frame *f){
	if (is_kernel_vaddr(f->R.rdi)||
	is_kernel_vaddr(f->R.rsi)||
	is_kernel_vaddr(f->R.rdx)||
	is_kernel_vaddr(f->R.r10)||
	is_kernel_vaddr(f->R.r8)||
	is_kernel_vaddr(f->R.r9)) thread_exit();
}
