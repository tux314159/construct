#ifndef INCLUDE_DSL_H
#define INCLUDE_DSL_H

#include "build.h"
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

#define needs(...) __VA_ARGS__,
#define nodeps
#define target(name, deps, cmd)                                                \
	_DSL_DECL_STMT(                                                            \
		graph_add_target(_construct_graph, target_make(name, cmd, deps NULL)); \
	)

#define add_dep(parent, dep)                                             \
	_DSL_DECL_STMT(                                                      \
		target_add_dep(graph_get_target(_construct_graph, parent), dep); \
	)

#endif

#define POISON_EXPORTS
#include "build.h"
