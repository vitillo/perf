/* based on addr2line */

#include "a2l.h"

#define PACKAGE 'perf'

#include <bfd.h>
#include <stdlib.h>
#include <stdio.h>

static const char *filename;
static const char *functionname;
static unsigned int line;
static asymbol **syms;
static bfd_vma pc;
static bfd_boolean found;
static bfd *abfd;

static void bfd_nonfatal(const char *string)
{
	const char *errmsg;

	errmsg = bfd_errmsg(bfd_get_error());
	fflush(stdout);
	if (string)
		fprintf(stderr, "%s: %s\n", string, errmsg);
	else
		fprintf(stderr, "%s\n", errmsg);
}

static int bfd_fatal(const char *string)
{
	bfd_nonfatal(string);
	return -1;
}

static int slurp_symtab(void)
{
	long storage;
	long symcount;
	bfd_boolean dynamic = FALSE;

	if ((bfd_get_file_flags (abfd) & HAS_SYMS) == 0)
		return bfd_fatal(bfd_get_filename (abfd));

	storage = bfd_get_symtab_upper_bound (abfd);
	if (storage == 0) {
		storage = bfd_get_dynamic_symtab_upper_bound (abfd);
		dynamic = TRUE;
	}
	if (storage < 0)
		return bfd_fatal(bfd_get_filename (abfd));

	syms = (asymbol **) malloc(storage);
	if (dynamic)
		symcount = bfd_canonicalize_dynamic_symtab (abfd, syms);
	else
		symcount = bfd_canonicalize_symtab (abfd, syms);

	if (symcount < 0)
		return bfd_fatal(bfd_get_filename (abfd));

	return 0;
}

static void find_address_in_section(bfd *self, asection *section, void *data ATTRIBUTE_UNUSED)
{
	bfd_vma vma;
	bfd_size_type size;

	if (found)
		return;

	if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0)
		return;

	vma = bfd_get_section_vma (abfd, section);
	if (pc < vma)
		return;

	size = bfd_get_section_size (section);
	if (pc >= vma + size)
		return;

	found = bfd_find_nearest_line (self, section, syms, pc - vma,
			&filename, &functionname, &line);
}

int addr2line_init(const char *file_name)
{
	abfd = bfd_openr(file_name, NULL);
	if (abfd == NULL)
		return -1;

	if (!bfd_check_format(abfd, bfd_object))
		return bfd_fatal(bfd_get_filename (abfd));

	return slurp_symtab();

}

void addr2line_cleanup(void)
{
	if (syms != NULL) {
		free(syms);
		syms = NULL;
	}

	bfd_close(abfd);
	line = found = 0;
}

int addr2line_inline(const char **file, unsigned *line_nr)
{

	found = bfd_find_inliner_info (abfd, &filename, &functionname, &line);
	*file = filename;
	*line_nr = line;

	return found;
}

int addr2line(unsigned long addr, const char **file, unsigned *line_nr)
{
	found = 0;
	pc = addr;
	bfd_map_over_sections(abfd, find_address_in_section, NULL);

	*file = filename;
	*line_nr = line;

	return found;
}
