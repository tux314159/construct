#include <stdatomic.h>
#include <unistd.h>

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
struct Target {
	char *name;
	char *raw_cmd;
	struct Array deps;
	struct Array codeps; // NOTE: array of struct Target *
	atomic_size_t *n_sat_dep;
	char visited;
};

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

// WARNING: must end arglist with null!
DECLARE(struct Target *, target_make, (const char *name, const char *cmd, ...));

DECLARE(void, target_add_dep, (struct Target * parent, const char *dep_name));

DECLARE(int, target_check_ood, (struct Target * targ));

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
	(struct Depgraph * graph, struct Target *final_targ, int max_jobs)
);

#ifndef INCLUDE_BUILD_H
#	define INCLUDE_BUILD_H
#endif
