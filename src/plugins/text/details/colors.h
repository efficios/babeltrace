#ifndef BABELTRACE_PLUGINS_TEXT_DETAILS_COLORS_H
#define BABELTRACE_PLUGINS_TEXT_DETAILS_COLORS_H

/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "common/common.h"

#include "write.h"

static inline
const char *color_reset(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_RESET;
	}

	return code;
}

static inline
const char *color_bold(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_BOLD;
	}

	return code;
}

static inline
const char *color_fg_default(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_FG_DEFAULT;
	}

	return code;
}

static inline
const char *color_fg_red(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_FG_RED;
	}

	return code;
}

static inline
const char *color_fg_green(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_FG_GREEN;
	}

	return code;
}

static inline
const char *color_fg_yellow(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_FG_YELLOW;
	}

	return code;
}

static inline
const char *color_fg_blue(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_FG_BLUE;
	}

	return code;
}

static inline
const char *color_fg_magenta(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_FG_MAGENTA;
	}

	return code;
}

static inline
const char *color_fg_cyan(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_FG_CYAN;
	}

	return code;
}

static inline
const char *color_fg_light_gray(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_FG_LIGHT_GRAY;
	}

	return code;
}

static inline
const char *color_bg_default(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_BG_DEFAULT;
	}

	return code;
}

static inline
const char *color_bg_red(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_BG_RED;
	}

	return code;
}

static inline
const char *color_bg_green(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_BG_GREEN;
	}

	return code;
}

static inline
const char *color_bg_yellow(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_BG_YELLOW;
	}

	return code;
}

static inline
const char *color_bg_blue(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_BG_BLUE;
	}

	return code;
}

static inline
const char *color_bg_magenta(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_BG_MAGENTA;
	}

	return code;
}

static inline
const char *color_bg_cyan(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_BG_CYAN;
	}

	return code;
}

static inline
const char *color_bg_light_gray(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = BT_COMMON_COLOR_BG_LIGHT_GRAY;
	}

	return code;
}

#endif /* BABELTRACE_PLUGINS_TEXT_DETAILS_COLORS_H */
