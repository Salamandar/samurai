#pragma once

struct node;

struct buildoptions {
	size_t maxjobs, maxfail;
	double maxload;
	_Bool verbose, explain, keepdepfile, keeprsp;
	const char *statusfmt;
};

extern struct buildoptions buildopts;

/* reset state, so a new build can be executed */
void buildreset(void);
/* schedule a particular target to be built */
void buildadd(struct node *);
/* execute rules to build the scheduled targets */
void build(void);
