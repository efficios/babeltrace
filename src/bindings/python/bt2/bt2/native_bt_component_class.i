/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

%include <babeltrace2/graph/component-class.h>
%include <babeltrace2/graph/component-class-const.h>
%include <babeltrace2/graph/component-class-source-const.h>
%include <babeltrace2/graph/component-class-source.h>
%include <babeltrace2/graph/component-class-filter-const.h>
%include <babeltrace2/graph/component-class-filter.h>
%include <babeltrace2/graph/component-class-sink-const.h>
%include <babeltrace2/graph/component-class-sink.h>
%include <babeltrace2/graph/self-component-class-source.h>
%include <babeltrace2/graph/self-component-class-filter.h>
%include <babeltrace2/graph/self-component-class-sink.h>

%{
#include "native_bt_component_class.i.h"
%}

struct bt_component_class_source *bt_bt2_component_class_source_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
struct bt_component_class_filter *bt_bt2_component_class_filter_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
struct bt_component_class_sink *bt_bt2_component_class_sink_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
