/*
 * Resources/Dependencies parser for ViewFS.
 * 
 * Copyright (C) 2008 Lucas C. Villa Real <lucasvr@gobolinux.org>
 *
 * Released under the GNU GPL version 2.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include "list.h"

static char * const currentString = "Current";
static char * const goboPrograms = "/Programs";

typedef enum {
	GREATER_THAN,			// >
	GREATER_THAN_OR_EQUAL,	// >=
	EQUAL,					// =
	NOT_EQUAL,				// !=
	LESS_THAN,				// <
	LESS_THAN_OR_EQUAL,		// <=
} operator_t;

struct version {
	char *version;			// version as listed in Resources/Dependencies
	operator_t op;			// one of the operators listed above
};

struct parse_data {
	char *workbuf;			// buffers the latest line read in Resources/Dependencies
	char *saveptr;			// strtok_r pointer for safe reentrancy
	bool hascomma;			// cache if a comma was found glued together with the first version
	char *depname;			// dependency name, as listed in Resources/Dependencies
	struct version v1;		// first optional restriction operator
	struct version v2;		// second optional restriction operator
	char fversion[NAME_MAX];// final version after restrictions were applied.
};

struct list_data {
	struct list_head list;	// link to the list on which we're inserted
	char path[PATH_MAX];	// full path to dependency
};

bool MatchRule(char *candidate, struct version *v)
{
	if (*candidate == '.' || !strcmp(candidate, "Variable") || !strcmp(candidate, "Settings") || !strcmp(candidate, "Current"))
		return false;
	switch (v->op) {
		case GREATER_THAN:
			return strcmp(candidate, v->version) > 0 ? true : false;
		case GREATER_THAN_OR_EQUAL:
			return strcmp(candidate, v->version) >= 0 ? true : false;
		case EQUAL: 
			return strcmp(candidate, v->version) == 0 ? true : false;
		case NOT_EQUAL: 
			return strcmp(candidate, v->version) != 0 ? true : false;
		case LESS_THAN: 
			return strcmp(candidate, v->version) < 0 ? true : false;
		case LESS_THAN_OR_EQUAL:
			return strcmp(candidate, v->version) <= 0 ? true : false;
		default:
			return false;
	}
}

bool RuleBestThanLatest(char *candidate, char *latest)
{
	if (latest[0] == '\0')
		return true;
	// If both versions are valid, the more recent one is taken
	return strcmp(candidate, latest) >= 0 ? true : false;
}

bool GetCurrentVersion(struct parse_data *data)
{
	ssize_t ret;
	char buf[PATH_MAX], path[PATH_MAX];

	snprintf(path, sizeof(path)-1, "%s/%s/Current", goboPrograms, data->depname);
	ret = readlink(path, buf, sizeof(buf));
	if (ret < 0) {
		fprintf(stderr, "%s: %s, ignoring dependency.\n", path, strerror(errno));
		return false;
	}
	buf[ret] = '\0';
	data->fversion[sizeof(data->fversion)-1] = '\0';
	strncpy(data->fversion, buf, sizeof(data->fversion)-1);
	return true;
}

bool GetBestVersion(struct parse_data *data)
{
	DIR *dp;
	struct dirent *entry;
	char path[PATH_MAX], latest[NAME_MAX];

	snprintf(path, sizeof(path)-1, "%s/%s", goboPrograms, data->depname);
	dp = opendir(path);
	if (! dp) {
		fprintf(stderr, "%s: %s, ignoring dependency.\n", path, strerror(errno));
		return false;
	}
	memset(latest, 0, sizeof(latest));
	while ((entry = readdir(dp))) {
		if (MatchRule(entry->d_name, &data->v1) && RuleBestThanLatest(entry->d_name, latest)) {
			strcpy(latest, entry->d_name);
			continue;
		}
	}
	if (! data->v2.version) {
		data->fversion[sizeof(data->fversion)-1] = '\0';
		strncpy(data->fversion, latest, sizeof(data->fversion)-1);
		return true;
	}

	if (! MatchRule(latest, &data->v2))
		memset(latest, 0, sizeof(latest));

	// TODO: although the data read from the previous readdir() is still in cache it would be better 
	// to keep its contents in an in-memory structure and just lookup on that instead.
	rewinddir(dp);
	while ((entry = readdir(dp))) {
		if (MatchRule(entry->d_name, &data->v2) && RuleBestThanLatest(entry->d_name, latest)) {
			strcpy(latest, entry->d_name);
			continue;
		}
	}
	if (! MatchRule(latest, &data->v1))
		memset(latest, 0, sizeof(latest));

	closedir(dp);
	if (! latest[0]) {
		fprintf(stderr, "No packages matching requirements were found, skipping dependency %s\n", data->depname);
		return false;
	}
	data->fversion[sizeof(data->fversion)-1] = '\0';
	strncpy(data->fversion, latest, sizeof(data->fversion)-1);
	return true;
}

void ListAppend(struct list_head *head, struct parse_data *data)
{
	struct list_data *ldata = (struct list_data *) malloc(sizeof(struct list_data));
	snprintf(ldata->path, sizeof(ldata->path), "%s/%s/%s", goboPrograms, data->depname, data->fversion);
	list_add_tail(&ldata->list, head);
}

char *ReadLine(char *buf, int size, FILE *fp)
{
	char *ptr, *ret = fgets(buf, size, fp);
	if (ret) {
		if (buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = '\0';
		ptr = strstr(buf, "#");
		if (ptr)
			*ptr = '\0';
	}
	return ret;
}

bool EmptyLine(char *buf)
{
	char *start;
	if (!buf || strlen(buf) == 0)
		return true;
	for (start=buf; *start; start++) {
		if (*start == ' ' || *start == '\t')
			continue;
		else if (*start == '\n')
			return true;
		else
			return false;
	}
	return false;
}

const char *GetOperatorString(operator_t op)
{
	switch (op) {
		case GREATER_THAN: return ">";
		case GREATER_THAN_OR_EQUAL: return ">=";
		case EQUAL: return "=";
		case NOT_EQUAL: return "!=";
		case LESS_THAN: return "<";
		case LESS_THAN_OR_EQUAL: return "<=";
		default: return "?";
	}
}

void PrintRestrictions(struct parse_data *data)
{
	if (data->v1.version)
		printf("%s %s", GetOperatorString(data->v1.op), data->v1.version);
	if (data->v2.version)
		printf(", %s %s", GetOperatorString(data->v2.op), data->v2.version);
}

bool ParseName(struct parse_data *data)
{
	data->depname = strtok_r(data->workbuf, " \t", &data->saveptr);
	return data->depname ? true : false;
}

bool ParseVersion(struct parse_data *data, struct version *v)
{
	char *ptr = strtok_r(NULL, " \t", &data->saveptr);
	if (! ptr)
		return false;
	if (ptr[strlen(ptr)-1] == ',') {
		ptr[strlen(ptr)-1] = '\0';
		data->hascomma = true;
	}
	v->version = ptr;
	return true;
}

bool ParseOperand(struct parse_data *data, struct version *v)
{
	char *ptr = strtok_r(NULL, " \t", &data->saveptr);
	if (! ptr) {
		// Only a program name without a version was supplied. Create symlinks for the View based on 'Current'.
		return GetCurrentVersion(data);
	}
	if (! strcmp(ptr, ">")) {
		v->op = GREATER_THAN;
	} else if (! strcmp(ptr, ">=")) {
		v->op = GREATER_THAN_OR_EQUAL;
	} else if (! strcmp(ptr, "==")) {
		v->op = EQUAL;
	} else if (! strcmp(ptr, "=")) {
		v->op = EQUAL;
	} else if (! strcmp(ptr, "!=")) {
		v->op = NOT_EQUAL;
	} else if (! strcmp(ptr, "<")) {
		v->op = LESS_THAN;
	} else if (! strcmp(ptr, "<=")) {
		v->op = LESS_THAN_OR_EQUAL;
	} else {
		// ptr holds the version alone. Consider operator as GREATER_THAN_OR_EQUAL (XXX)
		v->op = GREATER_THAN_OR_EQUAL;
		v->version = ptr;
		return true;
	}
	return ParseVersion(data, v);
}

bool ParseComma(struct parse_data *data)
{
	char *ptr;
	if (data->hascomma)
		return true;
	ptr = strtok_r(NULL, " \t", &data->saveptr);
	if (! ptr)
		return false;
	return strcmp(ptr, ",") == 0 ? true : false;
}

struct list_head *ParseDependencies(char *file)
{
	int line = 0;
	FILE *fp = fopen(file, "r");
	if (! fp) {
		fprintf(stderr, "%s: %s\n", file, strerror(errno));
		return NULL;
	}
	struct list_head *head = (struct list_head *) malloc(sizeof(struct list_head));
	if (! head) {
		perror("malloc");
		return NULL;
	}
	INIT_LIST_HEAD(head);

	while (!feof(fp)) {
		char buf[LINE_MAX];

		line++;
		if (! ReadLine(buf, sizeof(buf), fp))
			break;
		else if (EmptyLine(buf))
			continue;

		struct parse_data *data = (struct parse_data*) calloc(1, sizeof(struct parse_data));
		if (! data) {
			perror("malloc");
			free(head);
			return NULL;
		}
		data->workbuf = buf;
		//printf("[%s]\n", buf);

		if (! ParseName(data))
			continue;
		
		if (! ParseOperand(data, &data->v1))
			continue;

		if (! ParseComma(data)) {
			if (GetBestVersion(data))
				ListAppend(head, data);
			continue;
		}

		if (! ParseOperand(data, &data->v2)) {
			fprintf(stderr, "%s:%d: syntax error, ignoring dependency %s.\n", file, line, data->depname);
			continue;
		}

		if (GetBestVersion(data)) {
		//	printf("Adding %s with restrictions ", data->depname);
		//	PrintRestrictions(data);
		//	printf("\n");
			ListAppend(head, data);
		}
	}

	fclose(fp);
	return head;
}

#ifdef DEBUG
int main(int argc, char **argv)
{
	struct list_head *head;
	struct list_data *data;

	if (argc != 2) {
		printf("Syntax: %s <Dependencies file>\n", argv[0]);
		return 1;
	}

	head = ParseDependencies(argv[1]);
	if (! head)
		return 1;
	list_for_each_entry(data, head, list)
		printf("Mounting %s\n", data->path);

	return 0;
}
#endif
