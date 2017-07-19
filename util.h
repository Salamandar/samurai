struct buffer {
	char *data;
	size_t len, cap;
};

struct string {
	size_t n;
	char s[];
};

void *xmalloc(size_t);
void *xrealloc(void *, size_t);
void *xcalloc(size_t, size_t);
char *xstrndup(const char *, size_t);

struct string *mkstr(size_t);
