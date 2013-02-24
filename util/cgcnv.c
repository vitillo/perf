#include "cgcnv.h"
#include "parse-events.h"
#include "symbol.h"
#include "map.h"
#include "util.h"
#include "annotate.h"
#include "sort.h"
#include "a2l.h"

#include <linux/list.h>
#include <linux/rbtree.h>

static const char *last_source_name;
static unsigned last_line;
static u64 last_off;

int cg_cnv_header(FILE *output, struct perf_session *session)
{
	struct perf_evsel *pos;

	fprintf(output, "positions: instr line\nevents:");
	list_for_each_entry(pos, &session->evlist->entries, node) {
		const char *evname = NULL;
		struct hists *hists = &pos->hists;
		u32 nr_samples = hists->stats.nr_events[PERF_RECORD_SAMPLE];

		if (nr_samples > 0) {
			evname = perf_evsel__name(pos);
			fprintf(output, " %s", evname);
		}
	}
	fprintf(output, "\n");

	return 0;
}

int cg_cnv_sample(struct perf_evsel *evsel, struct perf_sample *sample,
		  struct addr_location *al, struct machine *machine){
	struct hist_entry *he;
	int ret = 0;

	if (sample->callchain) {
		struct symbol *parent = NULL;
		ret = machine__resolve_callchain(machine, evsel, al->thread,
						 sample, &parent);
		if (ret)
			return ret;
	}

	he = __hists__add_entry(&evsel->hists, al, NULL, 1);
	if (he == NULL)
		return -ENOMEM;

	ret = callchain_append(he->callchain, &callchain_cursor, sample->period);
	if (ret)
		return ret;

	ret = 0;
	if (he->ms.sym != NULL) {
		struct annotation *notes = symbol__annotation(he->ms.sym);
		if (notes->src == NULL && symbol__alloc_hist(he->ms.sym) < 0)
			return -ENOMEM;

		ret = hist_entry__inc_addr_samples(he, evsel->idx, al->addr);
	}

	evsel->hists.stats.total_period += sample->period;
	hists__inc_nr_events(&evsel->hists, PERF_RECORD_SAMPLE);
	return ret;
}

static void cg_sym_header_printf(FILE *output, struct symbol *sym,
				 struct map *map, struct annotation *notes,
				 u64 offset)
{
	int idx, ret, ret_callee, ret_caller = 0;
	u64 address = map__rip_2objdump(map, sym->start) + offset;
	unsigned caller_line;
	const char *caller_name;

	ret_callee = addr2line(address, &last_source_name, &last_line);
	while ((ret = addr2line_inline(&caller_name, &caller_line)))
		ret_caller = ret;

	/* Needed to display correctly the inlining relationship in kcachegrind */
	if (ret_caller && caller_line)
		fprintf(output, "fl=%s\n0 0\n", caller_name);

	if (ret_callee && last_line)
		fprintf(output, "fl=%s\n", last_source_name);
	else
		fprintf(output, "fl=\n");

	fprintf(output, "%#" PRIx64 " %u", address, last_line);
	for (idx = 0; idx < notes->src->nr_histograms; idx++)
		fprintf(output, " %" PRIu64,
			annotation__histogram(notes, idx)->addr[offset]);

	fprintf(output, "\n");
	last_off = offset;
}

static void cg_sym_events_printf(FILE *output, struct symbol *sym,
				 struct map *map, struct annotation *notes,
				 u64 offset)
{
	int ret, idx;
	unsigned line;
	const char *filename;

	ret = addr2line(map__rip_2objdump(map, sym->start) + offset,
			&filename, &line);
	if (filename && last_source_name && strcmp(filename, last_source_name)) {
		fprintf(output, "fl=%s\n", filename);
		last_source_name = filename;
	}

	if (ret)
		fprintf(output, "+%" PRIu64 " %+d", offset - last_off,
			(int)(line - last_line));
	else
		fprintf(output, "+%" PRIu64 " %u", offset - last_off, line);

	for (idx = 0; idx < notes->src->nr_histograms; idx++) {
		u64 cnt = annotation__histogram(notes, idx)->addr[offset];
		fprintf(output, " %" PRIu64, cnt);
	}

	fprintf(output, "\n");
	last_off = offset;
	last_line = line;
}

static inline bool cg_check_events(struct annotation *notes, u64 offset)
{
	int idx;

	for (idx = 0; idx < notes->src->nr_histograms; idx++)
		if (annotation__histogram(notes, idx)->addr[offset])
			return true;

	return false;
}

void cg_cnv_unresolved(FILE *output, u32 ev_id, struct hist_entry *he)
{
	u32 idx;

	fprintf(output, "ob=%s\n", he->ms.map->dso->long_name);
	fprintf(output, "fn=%#" PRIx64 "\n", he->ip);

	fprintf(output, "0 0");
	for (idx = 0; idx < ev_id; idx++)
		fprintf(output, " 0");
	fprintf(output, " %" PRIu32, he->stat.nr_events);
	fprintf(output, "\n");
}

int cg_cnv_symbol(FILE *output, struct symbol *sym, struct map *map)
{
	const char *filename = map->dso->long_name;
	struct annotation *notes = symbol__annotation(sym);
	u64 sym_len = sym->end - sym->start, i;

	if (addr2line_init(map->dso->long_name))
		return -EINVAL;

	fprintf(output, "ob=%s\n", filename);
	fprintf(output, "fn=%s\n", sym->name);

	for (i = 0; i < sym_len; i++) {
		if (cg_check_events(notes, i)) {
			cg_sym_header_printf(output, sym, map, notes, i);
			break;
		}
	}

	for (++i; i < sym_len; i++) {
		if (cg_check_events(notes, i))
			cg_sym_events_printf(output, sym, map, notes, i);
	}

	addr2line_cleanup();

	return 0;
}
