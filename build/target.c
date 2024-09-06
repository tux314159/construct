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
		if (frag->buf)                                                    \
			str_free(frag->buf);                                          \
		frag->buf = str_alloc();                                          \
		FRAG_RENDERER_IMPL(_frag_type)((FRAG_T(_frag_type) *)frag, rule); \
		(void)rule;                                                       \
	}

#define SUPER(_frag) ((struct FragBase *)_frag)

#define SUPER_INIT(_frag, _frag_type) \
	init_fragbase(&_frag->base, FRAG_RENDERER(_frag_type))

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

FRAGTYPE_DEF(alldeps);

FRAG_T(alldeps) *
FRAG_CONSTRUCTOR(alldeps)(void)
{
	FRAG_T(alldeps) *frag;
	frag = xmalloc(sizeof(*frag));
	SUPER_INIT(frag, alldeps);
	return frag;
}

static void
FRAG_RENDERER_IMPL(alldeps)(FRAG_T(alldeps) *frag, struct Rule *rule)
{
	for (void **dep = rule->deps.data; dep < rule->deps.data + rule->deps.len;
	     dep++)
		SUPER(frag)->buf = str_concat(
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
	if (deps)
		for (void **dep = deps->data; dep < deps->data + deps->len; dep++)
			array_push(&rule->deps, *dep);
	if (frags)
		for (struct FragBase **cmd = frags; cmd < frags + n_frags; cmd++)
			array_push(&rule->frags, *cmd);

	return rule;
}

/* Renders all fragments, and also constructs the command */
Str
render_rule(struct Rule *rule)
{
	Str buf;
	buf = str_alloc();

	SUPER(rule->targ)->render_frag(SUPER(rule->targ), rule);
	for (void **dep = rule->deps.data; dep < rule->deps.data + rule->deps.len;
	     dep++)
		SUPER(*dep)->render_frag(SUPER(*dep), rule);
	for (void **cmd = rule->frags.data;
	     cmd < rule->frags.data + rule->frags.len;
	     cmd++)
		SUPER(*cmd)->render_frag(SUPER(*cmd), rule);

	for (void **cmd = rule->frags.data;
	     cmd < rule->frags.data + rule->frags.len;
	     cmd++)
		buf = str_merge(buf, SUPER(*cmd)->buf); /* XXX destructive */
	return buf;
}

struct Target *
target_from_rule(struct Rule *rule)
{
	struct Target *target;

	target = xmalloc(sizeof(*target));
	target->cmd = render_rule(rule);

	target->name = SUPER(rule->targ)->buf;
	array_init(&target->deps);
	array_init(&target->codeps);

	atomic_init(&target->n_sat_dep, 0);
	target->visited = 0;

	for (size_t i = 0; i < rule->deps.len; i++)
		array_push(&target->deps, SUPER((FRAG_T(dep) *)rule->deps.data[i])->buf);

	return target;
}
