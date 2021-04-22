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
	int i, page_size, page_no, page_index;
	void *page_fault_addr, *aligned_addr;
	char *mmap_addr;
	int rc;
	int page_fault_in_segment = 0;
	so_seg_t segment;

	

	// Obtine adresa care a cauzat page fault-ul
	page_fault_addr = info->si_addr;
	printf(" page_fault_addr %d\n", page_fault_addr);

	// Obtine dimensiunea unei pagini
	page_size = getpagesize();
	
	// Parcurge segmentele executabilului
	//printf("segment.vaddr %d mem_size %d offset %d\n", exec->segments[1].vaddr, exec->segments[1].mem_size, exec->segments[1].offset);
	for (i = 0; i < exec->segments_no; i++) {
		//printf("%d\n", i);
		segment = exec->segments[i];

		// Verific daca segmentul contine adresa care a cauzat page fault-ul
		if ((int)page_fault_addr >= segment.vaddr 
			&& (int)page_fault_addr <= segment.vaddr + segment.mem_size) {
				// Marchez faptul ca page fault-ul vine dintr-un segment cunoscut
				page_fault_in_segment = 1;
				printf("%d segment.vaddr %d mem_size %d file_size %d\n",i,  segment.vaddr, segment.mem_size, segment.file_size);
				// Aflu pe cate pagini se intinde segmentul curent
				page_no = segment.mem_size / page_size;
				
				/* Aloc memorie pentru datele pe care le voi retine despre paginile segmentului
				 * atunci cand are loc primul page fault
				 * Initial paginile sunt marcate ca nemapate deci data = 0
				 */
				if (exec->segments[i].data == NULL) {
					printf("aici calloc\n");
					exec->segments[i].data = calloc(page_no, sizeof(uint8_t));
					if (exec->segments[i].data == NULL) {
						perror("Calloc error!");
						return;
					}
				}
				

				// Obtin indexul paginii din segment ce trebuie mapata
				page_index = ((int)page_fault_addr - segment.vaddr) / page_size;
				printf("page_index %d\n", page_index);

				
				// Daca pagina este marcata ca mapata, folosesc vechiul handler
				if (*((uint8_t *)exec->segments[i].data + page_index) == 1) {
					printf("aici1\n");
					old_act.sa_sigaction(signum, info, context);
					return;
				} else {
					
					// Aliniez adresa unde trebuie mapata pagina
					aligned_addr = (void *)segment.vaddr + page_index * page_size;					
					printf("aligned_addr %d\n", aligned_addr);

					// Zeroizez tot mem_size pentru cazul cand mem_size > file_size
					//memset((void*)segment.vaddr, 0, segment.mem_size);
					mmap_addr =  mmap(aligned_addr,
										page_size, segment.perm,
										MAP_FIXED | MAP_PRIVATE,
										fd,
										segment.offset);
					if (mmap_addr == MAP_FAILED) {
						printf("fail mmap\n");
						perror("Mmap failed");
						return;
					}
					memset(aligned_addr + segment.file_size, 0,  segment.mem_size - segment.file_size);
					
					rc = mprotect(aligned_addr, page_size, segment.perm);
					if (rc < 0) {
						printf("fail mprotect\n");
						perror("Mprotect failed");
						return;
					}
					printf("marchez maparea\n");
					// Marchez pagina ca mapata
					*((uint8_t *)exec->segments[i].data + page_index) = 1;
					
					return;
				}
				
		}
	}
	
	/* Daca page fault-ul nu face parte din niciun segment cunoscut
	 * Se foloseste vechiul handler
	 */
	if (page_fault_in_segment == 0) {
		printf("aici1");
		old_act.sa_sigaction(signum, info, context);
		return;
	}

	
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = sigsegv_handler;
	
	 sigemptyset(&sa.sa_mask);
	 sigaddset(&sa.sa_mask, SIGSEGV);

	rc = sigaction(SIGSEGV, &sa, &old_act);
	if (rc < 0) {
		perror("Sigaction failed\n");
		exit(EXIT_FAILURE);
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
