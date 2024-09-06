#include <stdatomic.h>
#include <unistd.h>

#include "target.h"
#include "simpleds.h"
#include "table.h"

#define DO_PRAGMA(x) _Pragma(#x)

#undef DECLARE
#ifdef POISON_EXPORTS
#	define DECLARE(T, X, A) DO_PRAGMA(GCC poison X)
#else
#	ifndef INCLUDE_BUILD_H
#		define DECLARE(T, X, A) T X A
#	else
#		define DECLARE(T, X, A)
#	endif
#endif

#ifndef INCLUDE_BUILD_H
struct Depgraph {
	size_t n_targets;
	struct Table targets;
};
#endif

DECLARE(
	char *,
	format_cmd,
	(const char *s, const char *name, struct Array *deps)
);

DECLARE(int, target_run, (struct Target * targ));

DECLARE(void, graph_init, (struct Depgraph * graph));

DECLARE(
	struct Target *,
	graph_add_target,
	(struct Depgraph * graph, struct Target *target)
);

DECLARE(
	struct Target *,
	graph_get_target,
	(struct Depgraph * graph, const char *targ_name)
);

DECLARE(
	void,
	graph_build,
	(struct Depgraph * graph, struct Target *final_targ, unsigned max_jobs)
);

#ifndef INCLUDE_BUILD_H
#	define INCLUDE_BUILD_H
#endif
