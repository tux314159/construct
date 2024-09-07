#include <stdbool.h>
#include <string.h>

#include "target.h"
#include "util.h"

static void
init_fragbase(struct FragBase *base, FragRenderer render_frag)
{
	base->buf = NULL;
	base->render_frag = render_frag;
}

#define FRAGTYPE_DEF(_frag_type)                                          \
	static void FRAG_RENDERER_IMPL(_frag_type)(                           \
		FRAG_T(_frag_type) * frag,                                        \
		struct Rule * rule                                                \
	);                                                                    \
	static void FRAG_RENDERER(_frag_type)(                                \
		struct FragBase * frag,                                           \
		struct Rule * rule                                                \
	)                                                                     \
	{                                                                     \
		FRAG_RENDERER_IMPL(_frag_type)((FRAG_T(_frag_type) *)frag, rule); \
	}

#define SUPER(_frag) ((struct FragBase *)_frag)

#define SUPER_INIT(_frag, _frag_type)                           \
	do {                                                        \
		init_fragbase(&_frag->base, FRAG_RENDERER(_frag_type)); \
		SUPER(_frag)->destructor = FRAG_DESTRUCTOR(_frag_type); \
	} while (0)

FRAGTYPE_DEF(target);

FRAG_T(target) *
FRAG_CONSTRUCTOR(target)(const char *name)
{
	FRAG_T(target) *frag;
	frag = xmalloc(sizeof(*frag));
	SUPER_INIT(frag, target);
	frag->name = str_fromraw(NULL, name);
	return frag;
}

Str
FRAG_DESTRUCTOR(target)(struct FragBase *frag)
{
	Str ret = SUPER(frag)->buf;
	str_free(((FRAG_T(target) *)frag)->name);
	free(frag);
	return ret;
}

static void
FRAG_RENDERER_IMPL(target)(FRAG_T(target) *frag, struct Rule *rule)
{
	frag->base.buf = str_dupl(frag->name);
	(void)rule;
}

FRAGTYPE_DEF(dep);

FRAG_T(dep) *
FRAG_CONSTRUCTOR(dep)(const char *name)
{
	FRAG_T(dep) *frag;
	frag = xmalloc(sizeof(*frag));
	SUPER_INIT(frag, dep);
	frag->name = str_fromraw(NULL, name);
	return frag;
}

Str
FRAG_DESTRUCTOR(dep)(struct FragBase *frag)
{
	Str ret = SUPER(frag)->buf;
	str_free(((FRAG_T(dep) *)frag)->name);
	free(frag);
	return ret;
}

static void
FRAG_RENDERER_IMPL(dep)(FRAG_T(dep) *frag, struct Rule *rule)
{
	frag->base.buf = str_dupl(frag->name);
	(void)rule;
}

FRAGTYPE_DEF(lit);

FRAG_T(lit) *
FRAG_CONSTRUCTOR(lit)(const char *name)
{
	FRAG_T(lit) *frag;
	frag = xmalloc(sizeof(*frag));
	SUPER_INIT(frag, lit);
	frag->str = str_fromraw(NULL, name);
	return frag;
}

Str
FRAG_DESTRUCTOR(lit)(struct FragBase *frag)
{
	Str ret = SUPER(frag)->buf;
	str_free(((FRAG_T(lit) *)frag)->str);
	free(frag);
	return ret;
}

static void
FRAG_RENDERER_IMPL(lit)(FRAG_T(lit) *frag, struct Rule *rule)
{
	SUPER(frag)->buf = str_dupl(frag->str);
	(void)rule;
}

FRAGTYPE_DEF(depidx);

FRAG_T(depidx) *
FRAG_CONSTRUCTOR(depidx)(size_t idx)
{
	FRAG_T(depidx) *frag;
	frag = xmalloc(sizeof(*frag));
	SUPER_INIT(frag, depidx);
	frag->idx = idx;
	return frag;
}

static void
FRAG_RENDERER_IMPL(depidx)(FRAG_T(depidx) *frag, struct Rule *rule)
{
	if (frag->idx == 0)
		SUPER(frag)->buf = str_dupl(((FRAG_T(dep) *)rule->targ)->base.buf);
	else
		SUPER(frag)->buf =
			str_dupl(((FRAG_T(dep) *)rule->deps.data[frag->idx - 1])->base.buf);
}

Str
FRAG_DESTRUCTOR(depidx)(struct FragBase *frag)
{
	Str ret = SUPER(frag)->buf;
	free(frag);
	return ret;
}

FRAGTYPE_DEF(alldeps);

FRAG_T(alldeps) *
FRAG_CONSTRUCTOR(alldeps)(void)
{
	FRAG_T(alldeps) *frag;
	frag = xmalloc(sizeof(*frag));
	SUPER_INIT(frag, alldeps);
	return frag;
}

Str
FRAG_DESTRUCTOR(alldeps)(struct FragBase *frag)
{
	Str ret = SUPER(frag)->buf;
	free(frag);
	return ret;
}

static void
FRAG_RENDERER_IMPL(alldeps)(FRAG_T(alldeps) *frag, struct Rule *rule)
{
	SUPER(frag)->buf = str_alloc();
	for (void **dep = rule->deps.data; dep < rule->deps.data + rule->deps.len;
	     dep++)
		SUPER(frag)->buf = str_merge(
			SUPER(frag)->buf,
			str_concatraw(str_dupl(SUPER(*dep)->buf), " ")
		);
}

struct Rule *
make_rule(
	FRAG_T(target) *targ,
	struct Array *deps,
	struct FragBase **frags,
	size_t n_frags
)
{
	struct Rule *rule;

	rule = xmalloc(sizeof(*rule));
	array_init(&rule->deps);
	array_init(&rule->frags);
	rule->targ = targ;
	if (deps) {
		for (void **dep = deps->data; dep < deps->data + deps->len; dep++)
			array_push(&rule->deps, *dep);
		array_destroy(deps);
		free(deps);
	}
	if (frags)
		for (struct FragBase **cmd = frags; cmd < frags + n_frags; cmd++)
			array_push(&rule->frags, *cmd);

	return rule;
}

/* Renders all fragments, and constructs the command */
Str
render_rule(struct Rule *rule)
{
	Str buf;
	buf = str_alloc();

	SUPER(rule->targ)->render_frag(SUPER(rule->targ), rule);
	for (void **dep = rule->deps.data; dep < rule->deps.data + rule->deps.len;
	     dep++)
		SUPER(*dep)->render_frag(SUPER(*dep), rule);
	for (void **frag = rule->frags.data;
	     frag < rule->frags.data + rule->frags.len;
	     frag++)
		SUPER(*frag)->render_frag(SUPER(*frag), rule);

	for (void **cmd = rule->frags.data;
	     cmd < rule->frags.data + rule->frags.len;
	     cmd++)
		buf = str_concat(buf, SUPER(*cmd)->buf);
	return buf;
}

/* Besides just constructing the target it also quasi-destructs
 * the entire rule and all its constituent objects. This is probably
 * the worst way I could've possibly done this. */
struct Target *
target_from_rule(struct Rule *rule)
{
	struct Target *targ;

	targ = xmalloc(sizeof(*targ));
	targ->cmd = render_rule(rule);
	targ->name = SUPER(rule->targ)->destructor(SUPER(rule->targ));
	targ->visited = false;
	atomic_init(&targ->n_sat_dep, 0);
	array_init(&targ->codeps);
	array_init(&targ->deps);
	for (size_t i = 0; i < rule->deps.len; i++) {
		FRAG_T(dep) *dep = rule->deps.data[i];
		array_push(&targ->deps, SUPER(dep)->destructor(SUPER(dep)));
	}

	for (size_t i = 0; i < rule->frags.len; i++) {
		struct FragBase *frag = rule->frags.data[i];
		str_free(frag->destructor(frag));
	}
	array_destroy(&rule->deps);
	array_destroy(&rule->frags);
	free(rule);

	return targ;
}

void
target_destroy(struct Target *targ)
{
	str_free(targ->name);
	str_free(targ->cmd);
	for (size_t i = 0; i < targ->deps.len; i++)
		str_free(targ->deps.data[i]);
	array_destroy(&targ->deps);
	array_destroy(&targ->codeps);
	free(targ);
}
