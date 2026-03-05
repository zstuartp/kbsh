/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "localize.h"

#include "core/arena.h"
#include "core/buffer.h"
#include "core/env.h"
#include "core/kbsh.h"
#include "core/parse.h"

struct kbsh_parse_state {
	struct Buffer *buffer;
	struct kbsh_arena *arena;
	struct {
		int checkme;
		enum kbsh_parse_result type;
	} syntax_err;
	int last_status;
	size_t argno;
	size_t bfind;
	size_t bpind;
	size_t hmind;
	int loop;
	int ignore_next;
	int in_arg;
	int in_quote;
	int in_squote;
	int in_dquote;
	int need_more;
};

static const char *kbsh_get_syntax_err_msg(const struct kbsh_parse_state *ps);
static void kbsh_parse_tok(struct kbsh_parse_state *ps);

static void parse_newline(struct kbsh_parse_state *ps)
{
	if (ps->ignore_next || ps->in_quote) {
		/* Newline at end of accumulated buffer: need more input */
		if (ps->buffer->full[ps->bfind + 1] == '\0') {
			ps->need_more = 1;
			ps->loop = 0;
			return;
		}
		/* Newline in the middle of an accumulated buffer while quoted:
		 * preserve it as a literal character in the output. */
		if (!ps->ignore_next && ps->in_quote) {
			ps->buffer->pars[ps->bpind] = ps->buffer->full[ps->bfind];
			ps->bpind++;
			if (!ps->in_arg) {
				ps->in_arg = 1;
				ps->argno++;
			}
		}
	}
	ps->ignore_next = 0;
}

static void parse_space(struct kbsh_parse_state *ps)
{
	if (ps->ignore_next || ps->in_quote) {
		ps->ignore_next = 0;
		if (!ps->in_arg) {
			ps->in_arg = 1;
			ps->argno++;
		}
		ps->buffer->pars[ps->bpind] = ps->buffer->full[ps->bfind];
	} else {
		ps->in_arg = 0;
		/* Separate arguments */
		ps->buffer->pars[ps->bpind] = 0x1d;
	}
	ps->bpind++;
}

static void parse_dquote(struct kbsh_parse_state *ps)
{
	if (ps->ignore_next || ps->in_squote) {
		ps->ignore_next = 0;
		ps->buffer->pars[ps->bpind] = ps->buffer->full[ps->bfind];
		ps->bpind++;
		if (!ps->in_arg) {
			ps->in_arg = 1;
			ps->argno++;
		}
	} else if (ps->in_dquote) {
		ps->syntax_err.checkme = 0;
		ps->in_dquote = 0;
		ps->in_quote = 0;
	} else {
		ps->syntax_err.checkme = 1;
		ps->syntax_err.type = KBSH_PARSE_ERROR_MISSING_DQUOTE;
		ps->in_dquote = 1;
		ps->in_quote = 1;
	}
}

static void parse_squote(struct kbsh_parse_state *ps)
{
	if (ps->ignore_next || ps->in_dquote) {
		ps->ignore_next = 0;
		ps->buffer->pars[ps->bpind] = ps->buffer->full[ps->bfind];
		ps->bpind++;
		if (!ps->in_arg) {
			ps->in_arg = 1;
			ps->argno++;
		}
	} else if (ps->in_squote) {
		ps->syntax_err.checkme = 0;
		ps->in_squote = 0;
		ps->in_quote = 0;
	} else {
		ps->syntax_err.checkme = 1;
		ps->syntax_err.type = KBSH_PARSE_ERROR_MISSING_SQUOTE;
		ps->in_squote = 1;
		ps->in_quote = 1;
	}
}

static void parse_bslash(struct kbsh_parse_state *ps)
{
	if (ps->ignore_next) {
		ps->ignore_next = 0;
		ps->buffer->pars[ps->bpind] = ps->buffer->full[ps->bfind];
		ps->bpind++;
		if (!ps->in_arg) {
			ps->in_arg = 1;
			ps->argno++;
		}
	} else if (ps->in_squote) {
		ps->buffer->pars[ps->bpind] = ps->buffer->full[ps->bfind];
		ps->bpind++;
	} else {
		ps->ignore_next = 1;
	}
}

static void parse_tilde(struct kbsh_parse_state *ps)
{
	if (ps->ignore_next || ps->in_quote || ps->in_arg) {
		ps->ignore_next = 0;
		ps->buffer->pars[ps->bpind] = ps->buffer->full[ps->bfind];
	} else if (env.home) {
		while (env.home[ps->hmind] != '\0')
			ps->buffer->pars[ps->bpind++] = env.home[ps->hmind++];

		ps->bpind--;
		ps->hmind = 0;
	}
	if (!ps->in_arg) {
		ps->in_arg = 1;
		ps->argno++;
	}
	ps->bpind++;
}

static void parse_dollar(struct kbsh_parse_state *ps)
{
	char name[64];
	char status_str[12];
	const char *value;
	size_t name_len;
	size_t k;
	char nc;
	char c;

	if (ps->ignore_next || ps->in_squote) {
		ps->ignore_next = 0;
		if (!ps->in_arg) {
			ps->in_arg = 1;
			ps->argno++;
		}
		ps->buffer->pars[ps->bpind++] = '$';
		return;
	}

	value = NULL;
	name_len = 0;
	nc = ps->buffer->full[ps->bfind + 1];

	if (nc == '?') {
		sprintf(status_str, "%d", ps->last_status);
		value = status_str;
		ps->bfind++;
	} else if (nc == '#') {
		sprintf(status_str, "%d", kbsh_positional_param_count);
		value = status_str;
		ps->bfind++;
	} else if (nc >= '0' && nc <= '9') {
		if (nc == '0') {
			value = program_name ? program_name : "";
		} else {
			int idx = nc - '1';
			if (idx < kbsh_positional_param_count)
				value = kbsh_positional_params[idx];
			else
				value = "";
		}
		ps->bfind++;
	} else if (nc == '@' || nc == '*') {
		/* expand all positional params space-joined */
		int pi;
		ps->bfind++;
		if (kbsh_positional_param_count > 0 && !ps->in_arg) {
			ps->in_arg = 1;
			ps->argno++;
		}
		for (pi = 0; pi < kbsh_positional_param_count; pi++) {
			const char *p = kbsh_positional_params[pi];
			if (pi > 0)
				ps->buffer->pars[ps->bpind++] = ' ';
			while (*p)
				ps->buffer->pars[ps->bpind++] = *p++;
		}
		return;
	} else if (nc == '{') {
		ps->bfind += 2;
		while ((c = ps->buffer->full[ps->bfind]) != '}' && c != '\0'
		       && name_len < sizeof(name) - 1) {
			name[name_len++] = ps->buffer->full[ps->bfind++];
		}
		name[name_len] = '\0';
		if (ps->buffer->full[ps->bfind] != '}') {
			ps->syntax_err.checkme = 1;
			ps->syntax_err.type = KBSH_PARSE_ERROR_MISSING_RBRACE;
			ps->loop = 0;
			return;
		}
		value = getenv(name);
		if (!value)
			value = "";
	} else if (isalpha((unsigned char)nc) || nc == '_') {
		ps->bfind++;
		while ((c = ps->buffer->full[ps->bfind]) != '\0'
		       && (isalnum((unsigned char)c) || c == '_')
		       && name_len < sizeof(name) - 1) {
			name[name_len++] = ps->buffer->full[ps->bfind++];
		}
		ps->bfind--;
		name[name_len] = '\0';
		value = getenv(name);
		if (!value)
			value = "";
	} else {
		if (!ps->in_arg) {
			ps->in_arg = 1;
			ps->argno++;
		}
		ps->buffer->pars[ps->bpind++] = '$';
		return;
	}

	if (value && *value && !ps->in_arg) {
		ps->in_arg = 1;
		ps->argno++;
	}
	if (value) {
		for (k = 0; value[k] != '\0'; k++)
			ps->buffer->pars[ps->bpind++] = value[k];
	}
}

static void parse_default(struct kbsh_parse_state *ps)
{
	if (!ps->in_arg) {
		ps->in_arg = 1;
		ps->argno++;
	}
	ps->ignore_next = 0;
	ps->buffer->pars[ps->bpind] = ps->buffer->full[ps->bfind];
	ps->bpind++;
}

enum kbsh_parse_result kbsh_parse(struct Buffer *b,
				  struct kbsh_arena *arena,
				  int last_status)
{
	struct kbsh_parse_state ps;

	if (!b)
		kbsh_exit(EINVAL);

	memset(&ps, 0, sizeof(ps));
	ps.buffer = b;
	ps.arena = arena;
	ps.last_status = last_status;
	ps.loop = 1;

	ps.buffer->full_size = strlen(ps.buffer->full);
	ps.buffer->pars_size = ps.buffer->full_size + 1;

	if (env.home) {
		size_t home_len;
		size_t tilde_count;
		size_t i;

		home_len = strlen(env.home);
		tilde_count = 0;
		for (i = 0; ps.buffer->full[i] != '\0'; i++) {
			if (ps.buffer->full[i] == '~')
				tilde_count++;
		}
		ps.buffer->pars_size += tilde_count * home_len;
	}

	/* Pre-scan: account for variable expansion size.
	 * Tracks in_squote state since $ is literal inside '...'. */
	{
		size_t i;
		int sq;
		int ign;
		char c;
		char nc;
		char name[64];
		size_t nl;
		const char *val;
		char sbuf[12];

		sq = 0;
		ign = 0;
		for (i = 0; (c = ps.buffer->full[i]) != '\0'; i++) {
			if (ign) { ign = 0; continue; }
			if (c == '\\' && !sq) { ign = 1; continue; }
			if (c == '\'') { sq = !sq; continue; }
			if (c != '$' || sq) continue;

			i++;
			nc = ps.buffer->full[i];

			if (nc == '?') {
				sprintf(sbuf, "%d", last_status);
				ps.buffer->pars_size += strlen(sbuf);
			} else if (nc == '#') {
				sprintf(sbuf, "%d", kbsh_positional_param_count);
				ps.buffer->pars_size += strlen(sbuf);
			} else if (nc >= '0' && nc <= '9') {
				if (nc == '0') {
					if (program_name)
						ps.buffer->pars_size += strlen(program_name);
				} else {
					int idx = nc - '1';
					if (idx < kbsh_positional_param_count)
						ps.buffer->pars_size +=
						    strlen(kbsh_positional_params[idx]);
				}
			} else if (nc == '@' || nc == '*') {
				int pi;
				for (pi = 0; pi < kbsh_positional_param_count; pi++) {
					ps.buffer->pars_size +=
					    strlen(kbsh_positional_params[pi]);
				}
				if (kbsh_positional_param_count > 1)
					ps.buffer->pars_size +=
					    (size_t)(kbsh_positional_param_count - 1);
			} else if (nc == '{') {
				i++;
				nl = 0;
				while ((c = ps.buffer->full[i]) != '}' && c != '\0'
				       && nl < sizeof(name) - 1)
					name[nl++] = ps.buffer->full[i++];
				name[nl] = '\0';
				val = getenv(name);
				if (val)
					ps.buffer->pars_size += strlen(val);
			} else if (isalpha((unsigned char)nc) || nc == '_') {
				nl = 0;
				while ((c = ps.buffer->full[i]) != '\0'
				       && (isalnum((unsigned char)c) || c == '_')
				       && nl < sizeof(name) - 1)
					name[nl++] = ps.buffer->full[i++];
				i--;
				name[nl] = '\0';
				val = getenv(name);
				if (val)
					ps.buffer->pars_size += strlen(val);
			}
		}
	}

	{
		unsigned char *out = NULL;

		if (kbsh_arena_alloc(arena, ps.buffer->pars_size,
				     KBSH_ARENA_DEFAULT_ALIGN,
				     &out) != KBSH_ARENA_SUCCESS)
			kbsh_exit(ENOMEM);
		ps.buffer->pars = (char *)out;
	}

	while (1) {
		switch (ps.buffer->full[ps.bfind]) {
		case '\0':
			ps.loop = 0;
			break;

		case '\n':
			parse_newline(&ps);
			break;

		case '\t':
		case ' ':
			parse_space(&ps);
			break;

		case '\"':
			parse_dquote(&ps);
			break;

		case '\'':
			parse_squote(&ps);
			break;

		case '\\':
			parse_bslash(&ps);
			break;

		case '~':
			parse_tilde(&ps);
			break;

		case '$':
			parse_dollar(&ps);
			break;

		default:
			parse_default(&ps);
			break;
		}
		if (ps.loop)
			ps.bfind++;
		else
			break;
	}

	if (ps.need_more)
		return KBSH_PARSE_NEED_MORE;

	ps.buffer->pars[ps.bpind] = '\0';

	if (ps.syntax_err.checkme) {
		fprintf(stderr, "%s: ", program_name);
		fprintf(stderr, _("syntax error: "));
		fprintf(stderr, "%s\n", kbsh_get_syntax_err_msg(&ps));
		return ps.syntax_err.type;
	}

	kbsh_parse_tok(&ps);
	return KBSH_PARSE_OK;
}

static const char *kbsh_get_syntax_err_msg(const struct kbsh_parse_state *ps)
{
	switch (ps->syntax_err.type) {
	case KBSH_PARSE_ERROR_MISSING_SQUOTE:
		return _("missing '");
		break;
	case KBSH_PARSE_ERROR_MISSING_DQUOTE:
		return _("missing \"");
		break;
	case KBSH_PARSE_ERROR_MISSING_RBRACE:
		return _("missing }");
		break;
	case KBSH_PARSE_ERROR_UNEXPECTED_EOF:
		return _("unexpected EOF");
	default:
		return _("unknown error");
		break;
	}
	return NULL;
}

static void kbsh_parse_tok(struct kbsh_parse_state *ps)
{
	size_t ind;
	char *temp;
	size_t word_alloc;

	ind = 0;
	temp = NULL;

	ps->buffer->word_size = ++ps->argno;
	word_alloc = sizeof(*ps->buffer->word) * (ps->buffer->word_size + 1);

	{
		unsigned char *out = NULL;

		if (kbsh_arena_alloc(ps->arena, word_alloc,
				     KBSH_ARENA_DEFAULT_ALIGN,
				     &out) != KBSH_ARENA_SUCCESS)
			kbsh_exit(ENOMEM);
		ps->buffer->word = (char **)out;
	}

	temp = strtok(ps->buffer->pars, "\x1d");
	while (temp != NULL) {
		ps->buffer->word[ind] = temp;
		temp = strtok(NULL, "\x1d");
		ind++;
	}
	ps->buffer->word[ind] = NULL;
	ps->buffer->word_used = ind;
}
