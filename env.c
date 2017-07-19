#define _POSIX_C_SOURCE 200809L
#include <err.h>
#include <search.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "env.h"
#include "graph.h"
#include "lex.h"
#include "util.h"

struct binding {
	char *var;
	struct string *val;
};

struct rulebinding {
	char *var;
	struct evalstring *val;
};

struct environment {
	struct environment *parent;
	void *bindings;
	void *rules;
};

struct rule *phonyrule;

static int
bindingcmp(const void *k1, const void *k2)
{
	const struct binding *b1 = k1, *b2 = k2;

	return strcmp(b1->var, b2->var);
}

static int
rulebindingcmp(const void *k1, const void *k2)
{
	const struct rulebinding *b1 = k1, *b2 = k2;

	return strcmp(b1->var, b2->var);
}

struct environment *
mkenv(struct environment *parent)
{
	struct environment *env;

	env = xmalloc(sizeof(*env));
	env->parent = parent;
	env->bindings = NULL;
	env->rules = NULL;

	return env;
}

static struct string *
lookupvar(struct environment *env, char *var)
{
	struct binding key = {var}, **node;

	node = tfind(&key, &env->bindings, bindingcmp);
	if (!node) {
		if (!env->parent)
			return NULL;
		return lookupvar(env->parent, var);
	}

	return (*node)->val;
}

void
envaddvar(struct environment *env, char *var, struct string *val)
{
	struct binding *b, **node;

	b = xmalloc(sizeof(*b));
	b->var = var;
	node = tsearch(b, &env->bindings, bindingcmp);
	if (!node)
		err(1, "tsearch");
	if (*node != b) {
		free((*node)->val);
		free(var);
		free(b);
	}
	(*node)->val = val;
}

static struct string *
merge(struct evalstring *str, size_t n)
{
	struct string *result;
	struct evalstringpart *p;
	char *s;

	result = mkstr(n);
	s = result->s;
	for (p = str ? str->parts : NULL; p; p = p->next) {
		if (!p->str)
			continue;
		memcpy(s, p->str->s, p->str->n);
		s += p->str->n;
	}
	*s = '\0';

	return result;
}

struct string *
enveval(struct environment *env, struct evalstring *str)
{
	size_t n;
	struct evalstringpart *p;

	n = 0;
	for (p = str ? str->parts : NULL; p; p = p->next) {
		if (p->var)
			p->str = lookupvar(env, p->var);
		if (p->str)
			n += p->str->n;
	}

	return merge(str, n);
}

static int
rulecmp(const void *k1, const void *k2)
{
	const struct rule *r1 = k1, *r2 = k2;

	return strcmp(r1->name, r2->name);
}

void
envaddrule(struct environment *env, struct rule *r)
{
	struct rule **node;

	node = tsearch(r, &env->rules, rulecmp);
	if (!node)
		err(1, "tsearch");
	if (*node != r)
		errx(1, "rule %s already defined", r->name);
}

struct rule *
envrule(struct environment *env, char *name)
{
	struct rule key = {name}, **node;

	node = tfind(&key, &env->rules, rulecmp);
	if (!node) {
		if (!env->parent)
			errx(1, "undefined rule %s", name);
		return envrule(env->parent, name);
	}

	return *node;
}

struct string *
pathlist(struct node **nodes, size_t n, char sep)
{
	size_t i, len;
	struct string *path, *result;
	char *s;

	for (i = 0, len = 0; i < n; ++i)
		len += nodes[i]->path->n;
	result = mkstr(len + n - 1);
	s = result->s;
	for (i = 0; i < n; ++i) {
		path = nodes[i]->path;
		memcpy(s, path->s, path->n);
		s += path->n;
		*s++ = sep;
	}
	*--s = '\0';

	return result;
}

struct rule *
mkrule(char *name)
{
	struct rule *r;

	r = xmalloc(sizeof(*r));
	r->name = name;
	r->bindings = NULL;

	return r;
}

void
ruleaddvar(struct rule *r, char *var, struct evalstring *val)
{
	struct rulebinding *b, **node;

	b = xmalloc(sizeof(*b));
	b->var = var;
	b->val = 0;
	node = tsearch(b, &r->bindings, rulebindingcmp);
	if (!node)
		err(1, "tsearch");
	if (*node != b) {
		free((*node)->val);
		free(var);
		free(b);
	}
	(*node)->val = val;
}

struct string *
edgevar(struct edge *e, char *var)
{
	struct rulebinding key = {var}, **node;
	struct string *result;
	struct evalstring *str;
	struct evalstringpart *p;
	size_t n;

	if (strcmp(var, "in") == 0) {
		return pathlist(e->in, e->inimpidx, ' ');
	} else if (strcmp(var, "in_newline") == 0) {
		return pathlist(e->in, e->inimpidx, '\n');
	} else if (strcmp(var, "out") == 0) {
		return pathlist(e->out, e->outimpidx, ' ');
	}

	result = lookupvar(e->env, var);
	if (result)
		return result;

	node = tfind(&key, &e->rule->bindings, rulebindingcmp);
	if (!node)
		return NULL;
	str = (*node)->val;

	n = 0;
	for (p = str ? str->parts : NULL; p; p = p->next) {
		if (p->var)
			p->str = edgevar(e, p->var);
		if (p->str)
			n += p->str->n;
	}

	return merge(str, n);
}
