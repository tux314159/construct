#ifndef INCLUDE_TARGET_H
#define INCLUDE_TARGET_H

#include "simpleds.h"
#include <stdatomic.h>

#define FRAG_T(_frag_type) struct Frag_##_frag_type
#define FRAG_RENDERER_IMPL(_frag_type) render_frag_##_frag_type
#define FRAG_RENDERER(_frag_type) render_frag_##_frag_type##_
#define FRAG_CONSTRUCTOR(_frag_type) make_frag_##_frag_type##_
#define FRAG_DESTRUCTOR(_frag_type) destroy_frag_##_frag_type##_

#define FRAGTYPE_DECL(_frag_type, _frag_fields, _frag_init_args)        \
	struct Frag_##_frag_type {                                          \
		struct FragBase base;                                           \
		_frag_fields                                                    \
	};                                                                  \
	FRAG_T(_frag_type) * FRAG_CONSTRUCTOR(_frag_type)(_frag_init_args); \
	Str FRAG_DESTRUCTOR(_frag_type)(struct FragBase * frag); /* cursed */

struct Rule {
	FRAG_T(target) *targ; /* TargetFrag */
	struct Array deps;  /* DepFrag */
	struct Array frags; /* FragBase */
};

struct Target {
	Str name;
	Str cmd;
	struct Array deps;
	struct Array codeps; /* NOTE: array of struct Target * */
	atomic_size_t n_sat_dep;
	char visited;
};

struct FragBase {
	void (*render_frag)(struct FragBase *, struct Rule *);
	Str (*destructor)(struct FragBase *frag);
	Str buf; /* WARNING: DO NOT FREE! */
};

typedef void (*FragRenderer)(struct FragBase *, struct Rule *);

FRAGTYPE_DECL(target, char *name;, const char *name)

FRAGTYPE_DECL(dep, char *name;, const char *name)

FRAGTYPE_DECL(lit, char *str;, const char *str)

FRAGTYPE_DECL(depidx, size_t idx;, size_t idx)

FRAGTYPE_DECL(alldeps, , void)

struct Rule *
make_rule(
	FRAG_T(target) *targ,
	struct Array *deps,
	struct FragBase **frags,
	size_t n_frags
);

Str
render_rule(struct Rule *rule);

/* WARNING: destroys the rule */
struct Target *
target_from_rule(struct Rule *rule);

void
target_destroy(struct Target *targ);

#endif
