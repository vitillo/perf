#ifndef __A2L_H_
#define __A2L_H_

int addr2line_init(const char *file_name);
int addr2line(unsigned long addr, const char **file, unsigned *line_nr);
int addr2line_inline(const char **file, unsigned *line_nr);
void addr2line_cleanup(void);

#endif
