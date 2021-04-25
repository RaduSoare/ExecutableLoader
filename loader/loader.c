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
	int i, page_size, page_index;
	void *page_fault_addr, *aligned_addr;
	char *mmap_addr;
	int length;
	int page_fault_in_segment = 0;
	so_seg_t segment;

	// Obtine adresa care a cauzat page fault-ul
	page_fault_addr = info->si_addr;

	// Obtine dimensiunea unei pagini
	page_size = getpagesize();

	// Parcurge segmentele executabilului
	for (i = 0; i < exec->segments_no; i++) {

		segment = exec->segments[i];

		// Verific daca segmentul contine adresa care a cauzat page fault-ul
		if ((int)page_fault_addr >= segment.vaddr &&
			(int)page_fault_addr <= segment.vaddr + segment.mem_size) {

			// Marchez faptul ca page fault-ul vine dintr-un segment cunoscut
			page_fault_in_segment = 1;

			// Obtin indexul paginii din segment ce trebuie mapata
			page_index = ((int)page_fault_addr - segment.vaddr) / page_size;

			// Daca pagina este marcata ca mapata, folosesc vechiul handler
			if (*((int *)exec->segments[i].data + page_index) == 1) {

				old_act.sa_sigaction(signum, info, context);
				return;
			}

			// Aliniez adresa unde trebuie mapata pagina
			aligned_addr = (void *)segment.vaddr + page_index * page_size;

			mmap_addr =  mmap(aligned_addr,
								page_size, segment.perm,
								MAP_FIXED | MAP_PRIVATE,
								fd,
								segment.offset + ((int)aligned_addr - segment.vaddr));
			if (mmap_addr == MAP_FAILED) {
				perror("Mmap failed");
				return;
			}

			// Calculez range-ul de adrese intre care trebuie sa zeroizez
			void *start_zero_zone = (void *)segment.vaddr + segment.file_size;
			void *end_zero_zone = (void *)segment.vaddr + segment.mem_size;


			void *start_addr = mmap_addr;

			// Cazul in care pagina se afla cu prima parte in afara intervalului de zeroizat
			if (start_addr <  start_zero_zone && start_addr + page_size >= start_zero_zone) {
				length = start_addr + page_size - start_zero_zone;
				if (segment.mem_size > segment.file_size)
					memset(start_zero_zone, 0, length);

				length = start_addr + page_size - start_zero_zone;
			// Cazul in care pagina se afla cu totul in intervalul de zeroizat
			} else if (start_addr >= start_zero_zone  && start_addr + page_size <= end_zero_zone) {
				memset(start_addr, 0, page_size);

			// Cazul in care pagina se afla cu a doua parte in afara intervalului de zeroizat
			} else if (start_addr < end_zero_zone && start_addr + page_size > end_zero_zone) {
				memset(start_addr, 0, end_zero_zone - start_addr);
			}

			// Marchez pagina ca mapata
			*((int *)exec->segments[i].data + page_index) = 1;

			return;

		}
	}

	/* Daca page fault-ul nu face parte din niciun segment cunoscut
	 * Se foloseste vechiul handler
	 */
	if (page_fault_in_segment == 0) {
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
	int page_no;

	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("Open failed");
		return -1;
	}

	/* Aloc memorie pentru datele pe care le voi retine despre paginile segmentului
	 * atunci cand are loc primul page fault
	 * Initial paginile sunt marcate ca nemapate deci data = 0
	 */
	for (int i = 0; i < exec->segments_no; i++) {

		// Calculeaza pe cate pagini se intinde segmentul
		page_no = exec->segments[i].mem_size / getpagesize();

		// Aloc un array in care retin starea paginilor (mapate/nemapate)
		exec->segments[i].data = calloc(page_no + 1, sizeof(int));
		if (exec->segments[i].data == NULL) {
			perror("Calloc error!");
			return -1;
		}
	}

	so_start_exec(exec, argv);

	return -1;
}
