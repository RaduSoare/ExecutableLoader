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
	int rc, bytes_read, bytes_to_be_read;
	int page_fault_in_segment = 0;
	so_seg_t segment;

	

	// Obtine adresa care a cauzat page fault-ul
	page_fault_addr = info->si_addr;
	

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
				
				// Aflu pe cate pagini se intinde segmentul curent
				page_no = segment.mem_size / page_size;
				//write(1, "aici1\n", strlen("aici1\n"));
				/* Aloc memorie pentru datele pe care le voi retine despre paginile segmentului
				 * atunci cand are loc primul page fault
				 * Initial paginile sunt marcate ca nemapate deci data = 0
				 */
				
					
					
				// Obtin indexul paginii din segment ce trebuie mapata
				page_index = ((int)page_fault_addr - segment.vaddr) / page_size;
				

				
				// Daca pagina este marcata ca mapata, folosesc vechiul handler
				if (*((uint8_t *)exec->segments[i].data + page_index) == 1) {
					
					old_act.sa_sigaction(signum, info, context);
					return;
				} else {
					
					// Aliniez adresa unde trebuie mapata pagina
					aligned_addr = (void *)segment.vaddr + page_index * page_size;					
					

					
					
					mmap_addr =  mmap(aligned_addr,
										page_size, segment.perm,
										MAP_FIXED | MAP_PRIVATE,
										fd,
										segment.offset + ((int)aligned_addr - segment.vaddr));
					if (mmap_addr == MAP_FAILED) {
						
						write(1, "failed mmap\n", strlen("failed mmap\n"));
						perror("Mmap failed");
						return;
					}
					// char buf[20];

					// if (i == 0) {
					// 	sprintf(buf, "%d\n", (int)i);
					// 	write(1, buf, strlen(buf));

					// 	sprintf(buf, "%d\n", (int)page_fault_addr);
					// 	write(1, buf, strlen(buf));

					// sprintf(buf, "%d\n", (int)segment.vaddr);
					// write(1, buf, strlen(buf));

					// sprintf(buf, "%d\n", segment.file_size);
					// write(1, buf, strlen(buf));

					// sprintf(buf, "%d\n", segment.mem_size);
					// write(1, buf, strlen(buf));

					// sprintf(buf, "%d\n", (int)mmap_addr);
					// write(1, buf, strlen(buf));
					// }
					

					void* start_zero_zone = (void *)segment.vaddr + segment.file_size;
					void* end_zero_zone = (void *)segment.vaddr + segment.mem_size;
					
					
					void* start_addr = mmap_addr;
					int length;
					if (start_addr <  start_zero_zone && start_addr + page_size >= start_zero_zone) {
						//write(1, "aici1\n", strlen("aici1\n"));
						length = start_addr + page_size - start_zero_zone;
						// sprintf(buf, "%d\n", (int)length);
						// write(1, buf, strlen(buf));
						if (segment.mem_size > segment.file_size) {
							//write(1, "aici1_2\n", strlen("aici1_2\n"));
							memset(start_zero_zone, 0, length);
						}
						// write(1, "aici1\n", strlen("aici1\n"));
						length = start_addr + page_size - start_zero_zone;
					} else if (start_addr >= start_zero_zone  && start_addr + page_size <= end_zero_zone) {
						//write(1, "aici2\n", strlen("aici2\n"));
						memset(start_addr, 0, page_size);

					} else if (start_addr < end_zero_zone && start_addr + page_size > end_zero_zone) {
						//write(1, "aici3\n", strlen("aici3\n"));
						memset(start_addr, 0, end_zero_zone - start_addr);
					} else {
						//write(1, "aici4\n", strlen("aici4\n"));
						//old_act.sa_sigaction(signum, info, context);
						//return;
						
					}
					
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
	for (int i = 0; i < exec->segments_no; i++) {
		int page_no = exec->segments[i].mem_size / getpagesize();
		exec->segments[i].data = calloc(page_no + 1, sizeof(uint8_t));
		if (exec->segments[i].data == NULL) {
			perror("Calloc error!");
			return -1;
		}
	}
	
	
	so_start_exec(exec, argv);

	return -1;
}
