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
		code = bt_common_color_reset();
	}

	return code;
}

static inline
const char *color_bold(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_bold();
	}

	return code;
}

static inline
const char *color_fg_default(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_default();
	}

	return code;
}

static inline
const char *color_fg_red(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_red();
	}

	return code;
}

static inline
const char *color_fg_green(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_green();
	}

	return code;
}

static inline
const char *color_fg_yellow(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_yellow();
	}

	return code;
}

static inline
const char *color_fg_blue(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_blue();
	}

	return code;
}

static inline
const char *color_fg_magenta(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_magenta();
	}

	return code;
}

static inline
const char *color_fg_cyan(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_cyan();
	}

	return code;
}

static inline
const char *color_fg_light_gray(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_light_gray();
	}

	return code;
}

static inline
const char *color_fg_bright_red(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_bright_red();
	}

	return code;
}

static inline
const char *color_fg_bright_green(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_bright_green();
	}

	return code;
}

static inline
const char *color_fg_bright_yellow(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_bright_yellow();
	}

	return code;
}

static inline
const char *color_fg_bright_blue(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_bright_blue();
	}

	return code;
}

static inline
const char *color_fg_bright_magenta(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_bright_magenta();
	}

	return code;
}

static inline
const char *color_fg_bright_cyan(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_bright_cyan();
	}

	return code;
}

static inline
const char *color_fg_bright_light_gray(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_fg_bright_light_gray();
	}

	return code;
}

static inline
const char *color_bg_default(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_bg_default();
	}

	return code;
}

static inline
const char *color_bg_red(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_bg_red();
	}

	return code;
}

static inline
const char *color_bg_green(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_bg_green();
	}

	return code;
}

static inline
const char *color_bg_yellow(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_bg_yellow();
	}

	return code;
}

static inline
const char *color_bg_blue(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_bg_blue();
	}

	return code;
}

static inline
const char *color_bg_magenta(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_bg_magenta();
	}

	return code;
}

static inline
const char *color_bg_cyan(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_bg_cyan();
	}

	return code;
}

static inline
const char *color_bg_light_gray(struct details_write_ctx *ctx)
{
	const char *code = "";

	if (ctx->details_comp->cfg.with_color) {
		code = bt_common_color_bg_light_gray();
	}

	return code;
}

#endif /* BABELTRACE_PLUGINS_TEXT_DETAILS_COLORS_H */
