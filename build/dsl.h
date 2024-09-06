#ifndef INCLUDE_DSL_H
#define INCLUDE_DSL_H

#include <stdarg.h>

#include "build.h"
#include "target.h"
#include "util.h"

#define _DSL_DECL_STMT(body)         \
	do {                             \
		_dbg_line_global = __LINE__; \
		_dbg_file_global = __FILE__; \
		body;                        \
		_dbg_line_global = 0;        \
		_dbg_file_global = NULL;     \
	} while (0)

#define construction_site(graph)              \
	struct Depgraph graph, *_construct_graph; \
	_construct_graph = &graph;                \
	graph_init(_construct_graph);             \
	(void)argc;                               \
	(void)argv; // later

#define construct(targ_name, njobs)                                   \
	_DSL_DECL_STMT(graph_build(                                       \
					   _construct_graph,                              \
					   graph_get_target(_construct_graph, targ_name), \
					   njobs                                          \
	);)

#define target(_targ_rule, _targ_deps, _targ_frags)    \
	graph_add_target(                                  \
		_construct_graph,                              \
		target_from_rule(make_rule(                    \
			FRAG_CONSTRUCTOR(target)(_targ_rule),      \
			_targ_deps,                                \
			(struct FragBase *[])_targ_frags,          \
			sizeof((struct FragBase *[])_targ_frags) / \
				sizeof(struct FragBase *)              \
		))                                             \
	)

#define ARRAY(...) {__VA_ARGS__}

#define executes ARRAY
#define dependencies ARRAY

#define _0 ),
#define _ _0 l " "_0
#define need(_dep) FRAG_CONSTRUCTOR(dep)(_dep)
#define needs(...)                                                    \
	_needs_impl(                                                      \
		sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *), \
		__VA_ARGS__                                                   \
	)

#define _FCONSTR(_frag_type) (struct FragBase *)FRAG_CONSTRUCTOR(_frag_type)
#define l _FCONSTR(lit)(
#define depn _FCONSTR(depidx)(
#define alldeps() _FCONSTR(alldeps)(
#define firstdep() depn(1)
#define targ() depn(0)

struct Array *
_needs_impl(size_t n, ...)
{
	struct Array *deps;
	va_list args;

	deps = xmalloc(sizeof(*deps));
	array_init(deps);
	va_start(args, n);
	for (size_t i = 0; i < n; ++i) {
		const char *s;
		s = va_arg(args, const char *);
		array_push(deps, FRAG_CONSTRUCTOR(dep)(s));
	}
	va_end(args);
	return deps;
}

#endif

#define POISON_EXPORTS
#include "build.h"
