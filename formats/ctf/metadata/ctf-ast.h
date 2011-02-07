#ifndef _CTF_PARSER_H
#define _CTF_PARSER_H

#include <helpers/list.h>

enum node_type {
	NODE_UNKNOWN,
	NODE_ROOT,
	NODE_EVENT,
	NODE_STREAM,
	NODE_TYPE,
	NODE_TRACE,

	NR_NODE_TYPES,
};

struct ctf_node;

struct ctf_node {
	enum node_type type;
	char *str;
	long long ll;
	struct ctf_node *parent;
	char *ident;
	struct cds_list_head siblings;
	struct cds_list_head children;
	struct cds_list_head gc;
};

int is_type(const char *id);

#endif /* _CTF_PARSER_H */
