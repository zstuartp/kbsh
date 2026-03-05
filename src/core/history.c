/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include "core/history.h"

#define KBSH_HISTORY_MAX 200
#define KBSH_HISTORY_LINE_MAX 512

static char s_history_entries[KBSH_HISTORY_MAX][KBSH_HISTORY_LINE_MAX];
static int s_history_count;

void kbsh_history_add(const char *line)
{
	int slot;

	if (!line || !*line)
		return;
	slot = s_history_count % KBSH_HISTORY_MAX;
	strncpy(s_history_entries[slot], line, KBSH_HISTORY_LINE_MAX - 1);
	s_history_entries[slot][KBSH_HISTORY_LINE_MAX - 1] = '\0';
	s_history_count++;
}

const char *kbsh_history_get(int index)
{
	int available;
	int oldest;
	int slot;

	available = s_history_count < KBSH_HISTORY_MAX ? s_history_count
						       : KBSH_HISTORY_MAX;
	if (index < 0 || index >= available)
		return NULL;
	oldest = s_history_count - available;
	slot = (oldest + index) % KBSH_HISTORY_MAX;
	return s_history_entries[slot];
}

int kbsh_history_count(void)
{
	return s_history_count < KBSH_HISTORY_MAX ? s_history_count
						  : KBSH_HISTORY_MAX;
}

void kbsh_history_load(const char *path)
{
	FILE *fp;
	char buf[KBSH_HISTORY_LINE_MAX];
	size_t len;

	fp = fopen(path, "r");
	if (!fp)
		return;
	while (fgets(buf, (int)sizeof(buf), fp)) {
		len = strlen(buf);
		if (len > 0 && buf[len - 1] == '\n')
			buf[--len] = '\0';
		if (len > 0 && buf[len - 1] == '\r')
			buf[--len] = '\0';
		if (buf[0] != '\0')
			kbsh_history_add(buf);
	}
	fclose(fp);
}

void kbsh_history_save(const char *path)
{
	FILE *fp;
	int i;
	int available;
	const char *entry;

	fp = fopen(path, "w");
	if (!fp)
		return;
	available = kbsh_history_count();
	for (i = 0; i < available; i++) {
		entry = kbsh_history_get(i);
		if (entry) {
			fputs(entry, fp);
			fputc('\n', fp);
		}
	}
	fclose(fp);
}
