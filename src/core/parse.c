/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "localize.h"

#include "core/arena.h"
#include "core/buffer.h"
#include "core/env.h"
#include "core/kbsh.h"
#include "core/parse.h"
#include "core/pipeline.h"

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
static void kbsh_build_pipeline(struct kbsh_parse_state *ps,
				struct kbsh_pipeline *pl);

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
			ps->buffer->pars[ps->bpind] =
			    ps->buffer->full[ps->bfind];
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
		while ((c = ps->buffer->full[ps->bfind]) != '}' && c != '\0' &&
		       name_len < sizeof(name) - 1) {
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
		while ((c = ps->buffer->full[ps->bfind]) != '\0' &&
		       (isalnum((unsigned char)c) || c == '_') &&
		       name_len < sizeof(name) - 1) {
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

enum kbsh_parse_result kbsh_parse(struct kbsh_pipeline *pl,
				  struct Buffer *staging,
				  struct kbsh_arena *arena,
				  int last_status)
{
	struct kbsh_parse_state ps;

	if (!pl || !staging)
		kbsh_exit(EINVAL);

	memset(&ps, 0, sizeof(ps));
	ps.buffer = staging;
	ps.arena = arena;
	ps.last_status = last_status;
	ps.loop = 1;

	ps.buffer->full_size = strlen(ps.buffer->full);

	/*
	 * Conservative pars allocation — no pre-scanning needed.
	 *
	 * 2× full_size handles moderate variable expansion (values up to
	 * the same length as the surrounding command text).  home_len covers
	 * tilde expansion.  The 256-byte pad handles small constants, status
	 * strings, and positional params.  For pathological inputs (a single
	 * $VAR that expands to several times full_size) the arena will
	 * reject the allocation and kbsh_exit fires — the same outcome as
	 * the old exact-size approach when the arena ran out.
	 */
	{
		size_t home_len = env.home ? strlen(env.home) : 0;
		size_t out_cap = ps.buffer->full_size * 2 + home_len + 256;
		unsigned char *out = NULL;

		if (kbsh_arena_alloc(
			arena, out_cap, KBSH_ARENA_DEFAULT_ALIGN, &out) !=
		    KBSH_ARENA_SUCCESS)
			kbsh_exit(ENOMEM);
		ps.buffer->pars = (char *)out;
		ps.buffer->pars_size = out_cap;
	}

	while (1) {
		/* Run-length: bulk-copy unambiguous plain chars via SIMD
		 * strcspn. Fires when not inside any quote and no pending
		 * escape — i.e. the common case for unquoted arguments that
		 * happen to contain $. Reduces dispatch iterations from
		 * O(chars) to O(special-chars). */
		if (!ps.ignore_next && !ps.in_squote && !ps.in_dquote) {
			const char *src = ps.buffer->full + ps.bfind;
			size_t run = strcspn(src, "\n\t \"'\\~$|><");

			if (run > 0) {
				if (!ps.in_arg) {
					ps.in_arg = 1;
					ps.argno++;
				}
				memcpy(ps.buffer->pars + ps.bpind, src, run);
				ps.bfind += run;
				ps.bpind += run;
				if (ps.buffer->full[ps.bfind] == '\0')
					break;
				continue;
			}
		}

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

		case '|':
			if (!ps.in_quote && !ps.ignore_next) {
				if (ps.in_arg) {
					ps.buffer->pars[ps.bpind++] = 0x1d;
					ps.in_arg = 0;
				}
				ps.buffer->pars[ps.bpind++] = '|';
				ps.buffer->pars[ps.bpind++] = 0x1d;
			} else {
				parse_default(&ps);
			}
			break;

		case '>':
			if (!ps.in_quote && !ps.ignore_next) {
				if (ps.in_arg) {
					ps.buffer->pars[ps.bpind++] = 0x1d;
					ps.in_arg = 0;
				}
				if (ps.buffer->full[ps.bfind + 1] == '>') {
					ps.buffer->pars[ps.bpind++] = '>';
					ps.buffer->pars[ps.bpind++] = '>';
					ps.bfind++;
				} else {
					ps.buffer->pars[ps.bpind++] = '>';
				}
				ps.buffer->pars[ps.bpind++] = 0x1d;
			} else {
				parse_default(&ps);
			}
			break;

		case '<':
			if (!ps.in_quote && !ps.ignore_next) {
				if (ps.in_arg) {
					ps.buffer->pars[ps.bpind++] = 0x1d;
					ps.in_arg = 0;
				}
				ps.buffer->pars[ps.bpind++] = '<';
				ps.buffer->pars[ps.bpind++] = 0x1d;
			} else {
				parse_default(&ps);
			}
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

	kbsh_build_pipeline(&ps, pl);
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

/* commit_cmd_argv: arena-allocate argv for one pipeline stage. */
static void commit_cmd_argv(struct kbsh_cmd *cmd,
			    char **words,
			    size_t nwords,
			    struct kbsh_arena *arena)
{
	unsigned char *out;

	if (kbsh_arena_alloc(arena,
			     sizeof(char *) * (nwords + 1),
			     sizeof(void *),
			     &out) != KBSH_ARENA_SUCCESS)
		kbsh_exit(ENOMEM);
	cmd->argv = (char **)out;
	if (nwords)
		memcpy(cmd->argv, words, sizeof(char *) * nwords);
	cmd->argv[nwords] = NULL;
	cmd->argc = (int)nwords;
}

/* kbsh_build_pipeline: one forward pass over pars[0..bpind-1].
 *
 * Tokens are delimited by 0x1d bytes.  Metacharacter tokens —
 * '|', '>', '>>', '<' — are recognised and split the flat token
 * stream into pipeline stages and per-stage redirect descriptors.
 * All other tokens are appended to the current stage's argv.
 */
static void kbsh_build_pipeline(struct kbsh_parse_state *ps,
				struct kbsh_pipeline *pl)
{
	char *tmp_words[512];
	size_t nwords = 0;
	char *p = ps->buffer->pars;
	char *end = p + ps->bpind;
	int pending_redir = -1;
	struct kbsh_cmd *cmd;

	memset(pl, 0, sizeof(*pl));
	pl->ncmds = 1;
	cmd = &pl->cmds[0];

	while (p < end) {
		char *word;
		size_t wlen;

		/* null-terminate and skip separator bytes */
		while (p < end && (unsigned char)*p == 0x1d)
			*p++ = '\0';
		if (p >= end)
			break;

		/* find end of this token */
		word = p;
		while (p < end && (unsigned char)*p != 0x1d)
			p++;
		wlen = (size_t)(p - word);

		if (wlen == 1 && word[0] == '|') {
			/* Pipe: commit current stage, advance */
			commit_cmd_argv(cmd, tmp_words, nwords, ps->arena);
			nwords = 0;
			pending_redir = -1;
			if (pl->ncmds < KBSH_PIPELINE_MAX)
				cmd = &pl->cmds[pl->ncmds++];
		} else if (wlen == 1 && word[0] == '>') {
			pending_redir = KBSH_REDIR_OUT;
		} else if (wlen == 2 && word[0] == '>' && word[1] == '>') {
			pending_redir = KBSH_REDIR_APPEND;
		} else if (wlen == 1 && word[0] == '<') {
			pending_redir = KBSH_REDIR_IN;
		} else if (pending_redir >= 0) {
			/* This token is the redirect target filename */
			if (cmd->nredirs < KBSH_CMD_REDIR_MAX) {
				cmd->redirs[cmd->nredirs].type =
				    (enum kbsh_redir_type)pending_redir;
				cmd->redirs[cmd->nredirs].target = word;
				cmd->nredirs++;
			}
			pending_redir = -1;
		} else {
			/* Regular argv word */
			if (nwords < 512)
				tmp_words[nwords++] = word;
		}
	}

	/* Commit the final pipeline stage */
	commit_cmd_argv(cmd, tmp_words, nwords, ps->arena);
}
