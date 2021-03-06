#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../src/utils.c"

int _assert_strcmp(char*, char*);

#define fail_test(M) \
	do { \
		failures++; \
		printf("\t%s %d: " M "\n", __func__, __LINE__); \
	} while (0)

#define fail_testf(M, ...) \
	do { \
		failures++; \
		printf("\t%s %d: " M "\n", __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define assert_strcmp(X, Y) \
	do { \
		if (_assert_strcmp(X, Y)) \
			fail_testf(#X " expected '%s', got '%s'", (Y) ? (Y) : "NULL", (X) ? (X) : "NULL"); \
	} while (0)

int
_assert_strcmp(char *p1, char *p2)
{
	if (p1 == NULL || p2 == NULL)
		return p1 != p2;

	return strcmp(p1, p2);
}

/* Recusrive functions for testing AVL tree properties */

int
_avl_count(avl_node *n)
{
	/* Count the number of nodes in a tree */

	if (n == NULL)
		return 0;

	return 1 + _avl_count(n->l) + _avl_count(n->r);
}

int
_avl_is_binary(avl_node *n)
{
	if (n == NULL)
		return 1;

	if (n->l && (strcmp(n->key, n->l->key) <= 0))
		return 0;

	if (n->r && (strcmp(n->key, n->r->key) >= 0))
		return 0;

	return 1 & _avl_is_binary(n->l) & _avl_is_binary(n->r);
}

int
_avl_height(avl_node *n)
{
	if (n == NULL)
		return 0;

	return 1 + MAX(_avl_height(n->l), _avl_height(n->r));
}

/*
 * Tests
 * */

int test_avl(void);
int test_parse(void);

int
test_avl(void)
{
	/* Test AVL tree functions */

	int failures = 0;

	avl_node *root = NULL;

	/* Insert strings a-z, zz-za, aa-az to hopefully excersize all combinations of rotations */
	const char **ptr, *strings[] = {
		"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
		"n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
		"zz", "zy", "zx", "zw", "zv", "zu", "zt", "zs", "zr", "zq", "zp", "zo", "zn",
		"zm", "zl", "zk", "zj", "zi", "zh", "zg", "zf", "ze", "zd", "zc", "zb", "za",
		"aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj", "ak", "al", "am",
		"an", "ao", "ap", "aq", "ar", "as", "at", "au", "av", "aw", "ax", "ay", "az",
		NULL
	};

	int ret, count = 0;

	/* Add all strings to the tree */
	for (ptr = strings; *ptr; ptr++) {
		if (!avl_add(&root, *ptr, NULL))
			fail_testf("avl_add() failed to add %s", *ptr);
		else
			count++;
	}

	/* Check that all were added correctly */
	if ((ret = _avl_count(root)) != count)
		fail_testf("_avl_count() returned %d, expected %d", ret, count);

	/* Check that the binary properties of the tree hold */
	if (!_avl_is_binary(root))
		fail_test("_avl_is_binary() failed");

	/* Check that the height of root stays within the mathematical bounds AVL trees allow */
	double max_height = 1.44 * log2(count + 2) - 0.328;

	if ((ret = _avl_height(root)) >= max_height)
		fail_testf("_avl_height() returned %d, expected strictly less than %f", ret, max_height);

	/* Test adding a duplicate and case sensitive duplicate */
	if (avl_add(&root, "aa", NULL) && count++)
		fail_test("avl_add() failed to detect duplicate 'aa'");

	if (avl_add(&root, "aA", NULL) && count++)
		fail_test("avl_add() failed to detect case sensitive duplicate 'aA'");

	/* Delete about half of the strings */
	int num_delete = count / 2;

	for (ptr = strings; *ptr && num_delete > 0; ptr++, num_delete--) {
		if (!avl_del(&root, *ptr))
			fail_testf("avl_del() failed to delete %s", *ptr);
		else
			count--;
	}

	/* Check that all were deleted correctly */
	if ((ret = _avl_count(root)) != count)
		fail_testf("_avl_count() returned %d, expected %d", ret, count);

	/* Check that the binary properties of the tree still hold */
	if (!_avl_is_binary(root))
		fail_test("_avl_is_binary() failed");

	/* Check that the height of root is still within the mathematical bounds AVL trees allow */
	max_height = 1.44 * log2(count + 2) - 0.328;

	if ((ret = _avl_height(root)) >= max_height)
		fail_testf("_avl_height() returned %d, expected strictly less than %f", ret, max_height);

	/* Test deleting string that was previously deleted */
	if (avl_del(&root, *strings))
		fail_testf("_avl_del() should have failed to delete %s", *strings);

	return failures;
}

int
test_parse(void)
{
	/* Test the IRC message parsing function */

	int ret, failures = 0;

	parsed_mesg p;

	/* Test empty message */
	char mesg0[] = "";

	/* Should fail due to empty command */
	if ((ret = parse(&p, mesg0)) != 0)
		fail_testf("parse() returned %d, expected 0", ret);
	assert_strcmp(p.from,     NULL);
	assert_strcmp(p.hostinfo, NULL);
	assert_strcmp(p.command,  NULL);
	assert_strcmp(p.params,   NULL);
	assert_strcmp(p.trailing, NULL);

	/* Test ordinary message */
	char mesg1[] = ":nick!user@hostname.domain CMD args :trailing";

	parse(&p, mesg1);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "args ");
	assert_strcmp(p.trailing, "trailing");

	/* Test no nick/host */
	char mesg2[] = "CMD arg1 arg2 :  trailing message  ";

	parse(&p, mesg2);
	assert_strcmp(p.from,     NULL);
	assert_strcmp(p.hostinfo, NULL);
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2 ");
	assert_strcmp(p.trailing, "  trailing message  ");

	/* Test the 15 arg limit */
	char mesg3[] = "CMD a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 :trailing message";

	parse(&p, mesg3);
	assert_strcmp(p.from,     NULL);
	assert_strcmp(p.hostinfo, NULL);
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 ");
	assert_strcmp(p.trailing, "a15 :trailing message");

	/* Test ':' can exist in args */
	char mesg4[] = ":nick!user@hostname.domain CMD arg:1:2:3 arg:4:5:6 :trailing message";

	parse(&p, mesg4);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg:1:2:3 arg:4:5:6 ");
	assert_strcmp(p.trailing, "trailing message");

	/* Test no args */
	char mesg5[] = ":nick!user@hostname.domain CMD :trailing message";

	parse(&p, mesg5);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   NULL);
	assert_strcmp(p.trailing, "trailing message");

	/* Test no trailing */
	char mesg6[] = ":nick!user@hostname.domain CMD arg1 arg2 arg3";

	parse(&p, mesg6);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2 arg3");
	assert_strcmp(p.trailing, NULL);

	/* Test no user */
	char mesg7[] = ":nick@hostname.domain CMD arg1 arg2 arg3";

	parse(&p, mesg7);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2 arg3");
	assert_strcmp(p.trailing, NULL);

	/* Test no command */
	char mesg8[] = ":nick!user@hostname.domain";

	/* Should fail due to empty command */
	if ((ret = parse(&p, mesg8)) != 0)
		fail_testf("parse() returned %d, expected 0", ret);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  NULL);
	assert_strcmp(p.params,   NULL);
	assert_strcmp(p.trailing, NULL);

	if (failures)
		printf("\t%d failure%c\n", failures, (failures > 1) ? 's' : 0);

	return failures;
}


int
main(void)
{
	printf(__FILE__":\n");

	int failures = 0;

	failures += test_avl();
	failures += test_parse();

	if (failures) {
		printf("%d failure%c total\n\n", failures, (failures > 1) ? 's' : 0);
		exit(EXIT_FAILURE);
	}

	printf("OK\n\n");

	return EXIT_SUCCESS;
}
