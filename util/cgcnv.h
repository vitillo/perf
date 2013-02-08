#ifndef CGCNV_H
#define CGCNV_H

#include "evlist.h"
#include "evsel.h"
#include "session.h"

#include <stdio.h>

int cg_cnv_header(FILE *output, struct perf_session *session);
int cg_cnv_sample(struct perf_evsel *evsel,
   		  struct perf_sample *sample,
		  struct addr_location *al,
		  struct machine *machine);
void cg_cnv_unresolved(FILE *output, u32 ev_id, struct hist_entry *he);
int cg_cnv_symbol(FILE *output, struct symbol *sym, struct map *map);

#endif

