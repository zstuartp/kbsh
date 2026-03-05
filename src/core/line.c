/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>

#if !defined(KBSH_PORTABLE_PROFILE)
#include <sys/select.h>
#endif

#include "core/history.h"
#include "core/line.h"

#define KBSH_LINE_MAX 4096
#define KBSH_LINE_SAVE_MAX 512

static char s_line_buffer[KBSH_LINE_MAX];
static size_t s_line_length;
static size_t s_cursor_position;
static struct termios s_saved_termios;
static int s_raw_mode_active;

static char s_saved_line[KBSH_LINE_SAVE_MAX]; /* saved on first up-arrow */
static int s_browse_pos; /* history index, or -1 when not browsing */

/* ------------------------------------------------------------------ */
/* Terminal raw mode                                                    */
/* ------------------------------------------------------------------ */

static void raw_mode_enter(void)
{
	struct termios t;

	t = s_saved_termios;
	t.c_lflag &= ~(unsigned)(ICANON | ECHO | ISIG | IEXTEN);
	t.c_iflag &= ~(unsigned)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	t.c_cflag |= CS8;
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
	s_raw_mode_active = 1;
}

static void raw_mode_exit(void)
{
	if (s_raw_mode_active) {
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_saved_termios);
		s_raw_mode_active = 0;
	}
}

/* ------------------------------------------------------------------ */
/* Timed single-byte read (50 ms used for ESC disambiguation)          */
/* ------------------------------------------------------------------ */

static int read_char_timed(unsigned char *out, int ms)
{
	fd_set fds;
	struct timeval tv;

	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = ms * 1000;
	if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) == 1)
		return (int)read(STDIN_FILENO, out, 1);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Display                                                              */
/* ------------------------------------------------------------------ */

static void redraw_line(const char *prompt)
{
	char esc[32];
	int esc_len;

	write(STDERR_FILENO, "\r\033[K", 4); /* CR + erase to end of line */
	write(STDERR_FILENO, prompt, strlen(prompt));
	if (s_line_length > 0)
		write(STDERR_FILENO, s_line_buffer, s_line_length);
	if (s_line_length > s_cursor_position) {
		esc_len =
		    sprintf(esc,
			    "\033[%luD",
			    (unsigned long)(s_line_length - s_cursor_position));
		write(STDERR_FILENO, esc, (size_t)esc_len);
	}
}

/* ------------------------------------------------------------------ */
/* History browsing helpers                                             */
/* ------------------------------------------------------------------ */

static void browse_load(int idx)
{
	const char *entry;

	entry = kbsh_history_get(idx);
	if (!entry)
		return;
	strncpy(s_line_buffer, entry, KBSH_LINE_MAX - 2);
	s_line_buffer[KBSH_LINE_MAX - 2] = '\0';
	s_line_length = strlen(s_line_buffer);
	s_cursor_position = s_line_length;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void kbsh_line_init(void)
{
	tcgetattr(STDIN_FILENO, &s_saved_termios);
	s_raw_mode_active = 0;
	s_browse_pos = -1;
}

void kbsh_line_exit(void) { raw_mode_exit(); }

/*
 * Read one edited line.  Returns a pointer to a static buffer that holds
 * the line followed by '\n' and '\0'.  Returns NULL on EOF / Ctrl-D on
 * an empty line.  Adds non-empty lines to history automatically.
 */
char *kbsh_line_read(const char *prompt)
{
	unsigned char ch;
	unsigned char seq1;
	unsigned char seq2;
	int avail;
	int n;

	s_line_length = 0;
	s_cursor_position = 0;
	s_browse_pos = -1;
	s_line_buffer[0] = '\0';

	raw_mode_enter();
	redraw_line(prompt);

	for (;;) {
		n = (int)read(STDIN_FILENO, &ch, 1);
		if (n <= 0) {
			raw_mode_exit();
			write(STDERR_FILENO, "\r\n", 2);
			return NULL;
		}

		/* ---- Submit ---- */
		if (ch == '\n' || ch == '\r') {
			write(STDERR_FILENO, "\r\n", 2);
			if (s_line_length > 0)
				kbsh_history_add(s_line_buffer);
			s_line_buffer[s_line_length] = '\n';
			s_line_buffer[s_line_length + 1] = '\0';
			raw_mode_exit();
			return s_line_buffer;
		}

		/* ---- Ctrl-C: clear line and re-prompt ---- */
		if (ch == 0x03) {
			s_line_length = 0;
			s_cursor_position = 0;
			s_browse_pos = -1;
			s_line_buffer[0] = '\0';
			write(STDERR_FILENO, "^C\r\n", 4);
			redraw_line(prompt);
			continue;
		}

		/* ---- Ctrl-D: EOF on empty, delete-char otherwise ---- */
		if (ch == 0x04) {
			if (s_line_length == 0) {
				raw_mode_exit();
				write(STDERR_FILENO, "\r\n", 2);
				return NULL;
			}
			if (s_cursor_position < s_line_length) {
				memmove(s_line_buffer + s_cursor_position,
					s_line_buffer + s_cursor_position + 1,
					s_line_length - s_cursor_position - 1);
				s_line_length--;
				s_line_buffer[s_line_length] = '\0';
				s_browse_pos = -1;
				redraw_line(prompt);
			}
			continue;
		}

		/* ---- Ctrl-A: start of line ---- */
		if (ch == 0x01) {
			s_cursor_position = 0;
			redraw_line(prompt);
			continue;
		}

		/* ---- Ctrl-E: end of line ---- */
		if (ch == 0x05) {
			s_cursor_position = s_line_length;
			redraw_line(prompt);
			continue;
		}

		/* ---- Ctrl-F: cursor right ---- */
		if (ch == 0x06) {
			if (s_cursor_position < s_line_length) {
				s_cursor_position++;
				redraw_line(prompt);
			}
			continue;
		}

		/* ---- Ctrl-B: cursor left ---- */
		if (ch == 0x02) {
			if (s_cursor_position > 0) {
				s_cursor_position--;
				redraw_line(prompt);
			}
			continue;
		}

		/* ---- Ctrl-P: history prev ---- */
		if (ch == 0x10) {
			avail = kbsh_history_count();
			if (avail == 0)
				continue;
			if (s_browse_pos == -1) {
				strncpy(s_saved_line,
					s_line_buffer,
					KBSH_LINE_SAVE_MAX - 1);
				s_saved_line[KBSH_LINE_SAVE_MAX - 1] = '\0';
				s_browse_pos = avail - 1;
			} else if (s_browse_pos > 0) {
				s_browse_pos--;
			}
			browse_load(s_browse_pos);
			redraw_line(prompt);
			continue;
		}

		/* ---- Ctrl-N: history next ---- */
		if (ch == 0x0e) {
			if (s_browse_pos == -1)
				continue;
			avail = kbsh_history_count();
			if (s_browse_pos < avail - 1) {
				s_browse_pos++;
				browse_load(s_browse_pos);
			} else {
				s_browse_pos = -1;
				strncpy(s_line_buffer,
					s_saved_line,
					KBSH_LINE_MAX - 2);
				s_line_buffer[KBSH_LINE_MAX - 2] = '\0';
				s_line_length = strlen(s_line_buffer);
				s_cursor_position = s_line_length;
			}
			redraw_line(prompt);
			continue;
		}

		/* ---- Ctrl-K: kill to end of line ---- */
		if (ch == 0x0b) {
			s_line_length = s_cursor_position;
			s_line_buffer[s_line_length] = '\0';
			s_browse_pos = -1;
			redraw_line(prompt);
			continue;
		}

		/* ---- Ctrl-U: kill entire line ---- */
		if (ch == 0x15) {
			s_line_length = 0;
			s_cursor_position = 0;
			s_browse_pos = -1;
			s_line_buffer[0] = '\0';
			redraw_line(prompt);
			continue;
		}

		/* ---- Backspace / Ctrl-H / DEL ---- */
		if (ch == 0x7f || ch == 0x08) {
			if (s_cursor_position > 0) {
				memmove(s_line_buffer + s_cursor_position - 1,
					s_line_buffer + s_cursor_position,
					s_line_length - s_cursor_position);
				s_cursor_position--;
				s_line_length--;
				s_line_buffer[s_line_length] = '\0';
				s_browse_pos = -1;
				redraw_line(prompt);
			}
			continue;
		}

		/* ---- ESC sequences ---- */
		if (ch == 0x1b) {
			if (read_char_timed(&seq1, 50) <= 0)
				continue; /* bare ESC */

			if (seq1 == '[') {
				if (read_char_timed(&seq2, 50) <= 0)
					continue;

				if (seq2 == 'A') {
					/* Up arrow: history prev */
					avail = kbsh_history_count();
					if (avail == 0)
						continue;
					if (s_browse_pos == -1) {
						strncpy(s_saved_line,
							s_line_buffer,
							KBSH_LINE_SAVE_MAX - 1);
						s_saved_line
						    [KBSH_LINE_SAVE_MAX - 1] =
							'\0';
						s_browse_pos = avail - 1;
					} else if (s_browse_pos > 0) {
						s_browse_pos--;
					}
					browse_load(s_browse_pos);
					redraw_line(prompt);
					continue;
				}

				if (seq2 == 'B') {
					/* Down arrow: history next */
					if (s_browse_pos == -1)
						continue;
					avail = kbsh_history_count();
					if (s_browse_pos < avail - 1) {
						s_browse_pos++;
						browse_load(s_browse_pos);
					} else {
						/* restore saved line */
						s_browse_pos = -1;
						strncpy(s_line_buffer,
							s_saved_line,
							KBSH_LINE_MAX - 2);
						s_line_buffer[KBSH_LINE_MAX -
							      2] = '\0';
						s_line_length =
						    strlen(s_line_buffer);
						s_cursor_position =
						    s_line_length;
					}
					redraw_line(prompt);
					continue;
				}

				if (seq2 == 'C') {
					/* Right arrow */
					if (s_cursor_position < s_line_length) {
						s_cursor_position++;
						redraw_line(prompt);
					}
					continue;
				}

				if (seq2 == 'D') {
					/* Left arrow */
					if (s_cursor_position > 0) {
						s_cursor_position--;
						redraw_line(prompt);
					}
					continue;
				}

				if (seq2 == 'H') {
					/* Home */
					s_cursor_position = 0;
					redraw_line(prompt);
					continue;
				}

				if (seq2 == 'F') {
					/* End */
					s_cursor_position = s_line_length;
					redraw_line(prompt);
					continue;
				}

				/* ESC [ 1 ~ (Home), ESC [ 3 ~ (Delete), ESC [ 4
				 * ~ (End) */
				if (seq2 == '1' || seq2 == '3' || seq2 == '4') {
					unsigned char tilde = 0;

					read_char_timed(&tilde,
							50); /* consume '~' */
					(void)tilde;
					if (seq2 == '1') {
						s_cursor_position = 0;
						redraw_line(prompt);
					} else if (seq2 == '3') {
						/* Delete key */
						if (s_cursor_position <
						    s_line_length) {
							memmove(
							    s_line_buffer +
								s_cursor_position,
							    s_line_buffer +
								s_cursor_position +
								1,
							    s_line_length -
								s_cursor_position -
								1);
							s_line_length--;
							s_line_buffer
							    [s_line_length] =
								'\0';
							s_browse_pos = -1;
							redraw_line(prompt);
						}
					} else {
						/* seq2 == '4': End */
						s_cursor_position =
						    s_line_length;
						redraw_line(prompt);
					}
					continue;
				}

				/* Unknown ESC [ sequence: ignore */
				continue;
			}

			if (seq1 == 'O') {
				/* SS3 sequences: ESC O H (Home), ESC O F (End)
				 */
				unsigned char ss3 = 0;

				if (read_char_timed(&ss3, 50) <= 0)
					continue;
				if (ss3 == 'H') {
					s_cursor_position = 0;
					redraw_line(prompt);
				} else if (ss3 == 'F') {
					s_cursor_position = s_line_length;
					redraw_line(prompt);
				}
				continue;
			}

			/* Unknown ESC sequence: ignore */
			continue;
		}

		/* ---- Regular printable ASCII ---- */
		if (ch >= 0x20 && ch < 0x7f) {
			if (s_line_length < (size_t)(KBSH_LINE_MAX - 2)) {
				if (s_cursor_position < s_line_length) {
					memmove(
					    s_line_buffer + s_cursor_position +
						1,
					    s_line_buffer + s_cursor_position,
					    s_line_length - s_cursor_position);
				}
				s_line_buffer[s_cursor_position] = (char)ch;
				s_cursor_position++;
				s_line_length++;
				s_line_buffer[s_line_length] = '\0';
				s_browse_pos = -1;
				redraw_line(prompt);
			}
		}
	}
}
