#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "common.h"

#define H(N) (N == NULL ? 0 : N->height)
#define MAX(A, B) (A > B ? A : B)

/* AVL tree function */
static avl_node* _avl_add(avl_node*, const char*, void*);
static avl_node* _avl_del(avl_node*, const char*);
static avl_node* _avl_get(avl_node*, const char*, size_t);
static avl_node* avl_new_node(const char*, void*);
static avl_node* avl_rotate_L(avl_node*);
static avl_node* avl_rotate_R(avl_node*);

static jmp_buf jmpbuf;

/* TODO:
 *
 * this should just be rewritten as an implementation of strsep, and would replace
 * the faulty calls to strtok/strtok_r
 * */
char*
getarg(char **str, int set_null)
{
	char *ptr, *ret;

	/* Check that str isnt pointing to NULL */
	if (!(ptr = *str))
		return NULL;
	else while (*ptr && *ptr == ' ')
		ptr++;

	if (*ptr)
		ret = ptr;
	else
		return NULL;

	while (*ptr && *ptr != ' ')
		ptr++;

	if (set_null && *ptr == ' ')
		*ptr++ = '\0';

	*str = ptr;

	return ret;
}

void
auto_nick(char **autonick, char *nick)
{
	char *p = *autonick;
	while (*p == ' ' || *p == ',')
		p++;

	if (*p == '\0') {

		/* Autonicks exhausted, generate a random nick */
		char *base = "rirc_";
		char *cset = "0123456789ABCDEF";

		strcpy(nick, base);
		nick += strlen(base);

		int i, len = strlen(cset);
		for (i = 0; i < 4; i++)
			*nick++ = cset[rand() % len];
	} else {
		int c = 0;
		while (*p != ' ' && *p != ',' && *p != '\0' && c++ < NICKSIZE)
			*nick++ = *p++;
		*autonick = p;
	}

	*nick = '\0';
}


char*
strdup(const char *str)
{
	char *ret;

	if ((ret = malloc(strlen(str) + 1)) == NULL)
		fatal("malloc");

	strcpy(ret, str);

	return ret;
}

/* FIXME:
 *
 * Parsing of the 15 arg max doesn't work correctly
 *
 * if no args, point args to trailing
 *
 * */
int
parse(parsed_mesg *p, char *mesg)
{
	/* RFC 2812, section 2.3.1 */
	/* message = [ ":" prefix SPACE ] command [ params ] crlf */
	/* nospcrlfcl =  %x01-09 / %x0B-0C / %x0E-1F / %x21-39 / %x3B-FF */
	/* middle =  nospcrlfcl *( ":" / nospcrlfcl ) */

	*p = (parsed_mesg){0};

	/* prefix = servername / ( nickname [ [ "!" user ] "@" host ] ) */

	if (*mesg == ':') {

		p->from = ++mesg;

		while (*mesg) {
			if (*mesg == '!' || (*mesg == '@' && !p->hostinfo)) {
				*mesg++ = '\0';
				p->hostinfo = mesg;
			} else if (*mesg == ' ') {
				*mesg++ = '\0';
				break;
			}
			mesg++;
		}
	}

	/* command = 1*letter / 3digit */
	if (!(p->command = getarg(&mesg, 1)))
		return 0;

	/* params = *14( SPACE middle ) [ SPACE ":" trailing ] */
	/* params =/ 14( SPACE middle ) [ SPACE [ ":" ] trailing ] */
	/* trailing   =  *( ":" / " " / nospcrlfcl ) */

	char *param;
	if ((param = getarg(&mesg, 0))) {
		if (*param == ':') {
			p->params = NULL;
			*param++ = '\0';
			p->trailing = param;
		} else {
			p->params = param;

			int paramcount = 1;
			while ((param = getarg(&mesg, 0))) {
				if (paramcount == 14 || *param == ':') {
					*param++ = '\0';
					p->trailing = param;
					break;
				}
				paramcount++;
			}
		}
	}

	return 1;
}

/* TODO:
 * Consider cleaning up the policy here. Ideally a match should be:
 * match = nick *[chars] (space / null)
 * chars = Any printable characters not allowed in a nick by this server */
int
check_pinged(char *mesg, char *nick)
{
	int len = strlen(nick);

	while (*mesg) {

		/* skip to next word */
		while (*mesg == ' ')
			mesg++;

		/* nick prefixes the word, following character is space or symbol */
		if (!strncasecmp(mesg, nick, len) && !isalnum(*(mesg + len))) {
			putchar('\a');
			return 1;
		}

		/* skip to end of word */
		while (*mesg && *mesg != ' ')
			mesg++;
	}

	return 0;
}

/* AVL tree functions */

void
free_avl(avl_node *n)
{
	/* Recusrively free an AVL tree */

	if (n == NULL)
		return;

	free_avl(n->l);
	free_avl(n->r);
	free(n->key);
	free(n->val);
	free(n);
}

int
avl_add(avl_node **n, const char *key, void *val)
{
	/* Entry point for adding a node to an AVL tree */

	if (setjmp(jmpbuf))
		return 0;

	*n = _avl_add(*n, key, val);

	return 1;
}

int
avl_del(avl_node **n, const char *key)
{
	/* Entry point for removing a node from an AVL tree */

	if (setjmp(jmpbuf))
		return 0;

	*n = _avl_del(*n, key);

	return 1;
}

const avl_node*
avl_get(avl_node *n, const char *key, size_t len)
{
	/* Entry point for fetching an avl node with prefix key */

	if (setjmp(jmpbuf))
		return NULL;

	return _avl_get(n, key, len);
}

static avl_node*
avl_new_node(const char *key, void *val)
{
	avl_node *n;

	if ((n = calloc(1, sizeof(*n))) == NULL)
		fatal("calloc");

	n->height = 1;
	n->key = strdup(key);
	n->val = val;

	return n;
}

static avl_node*
avl_rotate_R(avl_node *r)
{
	/* Rotate right for root r and pivot p
	 *
	 *     r          p
	 *    / \   ->   / \
	 *   p   c      a   r
	 *  / \            / \
	 * a   b          b   c
	 *
	 */

	avl_node *p = r->l;
	avl_node *b = p->r;

	p->r = r;
	r->l = b;

	r->height = MAX(H(r->l), H(r->r)) + 1;
	p->height = MAX(H(p->l), H(p->r)) + 1;

	return p;
}

static avl_node*
avl_rotate_L(avl_node *r)
{
	/* Rotate left for root r and pivot p
	 *
	 *   r            p
	 *  / \    ->    / \
	 * a   p        r   c
	 *    / \      / \
	 *   b   c    a   b
	 *
	 */

	avl_node *p = r->r;
	avl_node *b = p->l;

	p->l = r;
	r->r = b;

	r->height = MAX(H(r->l), H(r->r)) + 1;
	p->height = MAX(H(p->l), H(p->r)) + 1;

	return p;
}

static avl_node*
_avl_add(avl_node *n, const char *key, void *val)
{
	/* Recursively add key to an AVL tree.
	 *
	 * If a duplicate is found (case insensitive) longjmp is called to indicate failure */

	if (n == NULL)
		return avl_new_node(key, val);

	int ret = strcasecmp(key, n->key);

	if (ret == 0)
		/* Duplicate found */
		longjmp(jmpbuf, 1);

	else if (ret > 0)
		n->r = _avl_add(n->r, key, val);

	else if (ret < 0)
		n->l = _avl_add(n->l, key, val);

	/* Node was successfully added, recaculate height and rebalance */

	n->height = MAX(H(n->l), H(n->r)) + 1;

	int balance = H(n->l) - H(n->r);

	/* right rotation */
	if (balance > 1) {

		/* left-right rotation */
		if (strcasecmp(key, n->l->key) > 0)
			n->l = avl_rotate_L(n->l);

		return avl_rotate_R(n);
	}

	/* left rotation */
	if (balance < -1) {

		/* right-left rotation */
		if (strcasecmp(n->r->key, key) > 0)
			n->r = avl_rotate_R(n->r);

		return avl_rotate_L(n);
	}

	return n;
}

static avl_node*
_avl_del(avl_node *n, const char *key)
{
	/* Recursive function for deleting nodes from an AVL tree
	 *
	 * If the node isn't found (case insensitive) longjmp is called to indicate failure */

	if (n == NULL)
		/* Node not found */
		longjmp(jmpbuf, 1);

	int ret = strcasecmp(key, n->key);

	if (ret == 0) {
		/* Node found */

		if (n->l && n->r) {
			/* Recusrively delete nodes with both children to ensure balance */

			/* Find the next largest value in the tree (the leftmost node in the right subtree) */
			avl_node *next = n->r;

			while (next->l)
				next = next->l;

			/* Swap it's value with the node being deleted */
			char *t = n->key;

			n->key = next->key;
			next->key = t;

			/* Recusively delete in the right subtree */
			n->r = _avl_del(n->r, t);

		} else {
			/* If n has a child, return it */
			avl_node *tmp = (n->l) ? n->l : n->r;

			free(n);

			return tmp;
		}
	}

	else if (ret > 0)
		n->r = _avl_del(n->r, key);

	else if (ret < 0)
		n->l = _avl_del(n->l, key);

	/* Node was successfully deleted, recalculate height and rebalance */

	n->height = MAX(H(n->l), H(n->r)) + 1;

	int balance = H(n->l) - H(n->r);

	/* right rotation */
	if (balance > 1) {

		/* left-right rotation */
		if (H(n->l->l) - H(n->l->r) < 0)
			n->l =  avl_rotate_L(n->l);

		return avl_rotate_R(n);
	}

	/* left rotation */
	if (balance < -1) {

		/* right-left rotation */
		if (H(n->r->l) - H(n->r->r) > 0)
			n->r = avl_rotate_R(n->r);

		return avl_rotate_L(n);
	}

	return n;
}

static avl_node*
_avl_get(avl_node *n, const char *key, size_t len)
{
	/* Case insensitive search for a node whose value is prefixed by key */

	/* Failed to find node */
	if (n == NULL)
		longjmp(jmpbuf, 1);

	int ret = strncasecmp(key, n->key, len);

	if (ret > 0)
		return _avl_get(n->r, key, len);

	if (ret < 0)
		return _avl_get(n->l, key, len);

	/* Match found */
	return n;
}
