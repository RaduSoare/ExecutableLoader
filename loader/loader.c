/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "exec_parser.h"

static so_exec_t *exec;
int fd; 
struct sigaction old_act;

static void sigsegv_handler(int signum, siginfo_t *info, void *context)
{

	int i, page_size, page_no;
	void *page_fault_addr;
	char *mmap_addr;
	int page_fault_in_segment = 0;

	// Obtine adresa care a cauzat page fault-ul
	page_fault_addr = info->si_addr;

	// Obtine dimensiunea unei pagini
	page_size = getpagesize();
	
	// Parcurge segmentele executabilului
	for (i = 0; i < exec->segments_no; i++) {
		so_seg_t segment = exec->segments[i];
		// Verific daca segmentul contine adresa care a cauzat page fault-ul
		if ((int)page_fault_addr >= segment.vaddr 
			&& (int)page_fault_addr <= segment.vaddr + segment.mem_size) {
				
				// Marchez faptul ca page fault-ul vine dintr-un segment cunoscut
				page_fault_in_segment = 1;
				
				// Aflu pe cate pagini se intinde segmentul curent
				page_no = segment.mem_size / page_size;

				/* Aloc memorie pentru datele pe care le voi retine despre paginile segmentului
				 * atunci cand are loc primul page fault
				 * Initial paginile sunt marcate ca nemapate deci data = 0
				 */
				if (segment.data == NULL) {
					segment.data = calloc(page_no, sizeof(int));
					if (segment.data == NULL) {
						perror("Calloc error!");
						return;
					}
				}

				mmap_addr =  mmap((void*)segment.vaddr,
									page_size, segment.perm,
									MAP_FIXED | MAP_PRIVATE,
									fd,
									segment.offset);
				if (mmap_addr == MAP_FAILED) {
					perror("Mmap failed");
					return;
				}
				
		}
		
	}

	/* Daca page fault-ul nu face parte din niciun segment cunoscut
	 * Se foloseste vechiul handler
	 */
	if (page_fault_in_segment == 0) {
		old_act.sa_sigaction(signum, info, context);
	}

	exit(0);
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;
	
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = &sigsegv_handler;
	sa.sa_flags = SA_SIGINFO;
	 

	rc = sigaction(SIGSEGV, &sa, &old_act);
	if (rc < 0) {
		perror("Sigaction failed\n");
	}
	return -1;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	if (!exec)
		return -1;
	
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("Open failed");
		return -1;
	}

	so_start_exec(exec, argv);

	return -1;
}
