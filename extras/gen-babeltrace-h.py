import re


class _Section:
    def __init__(self, title, filenames):
        self.title = title
        self.filenames = filenames


def _get_sections(file):
    sections = []
    cur_title = None
    cur_filenames = []

    for line in file:
        m = re.match(r'^# (.+API.*)$', line)

        if m:
            if cur_filenames:
                sections.append(_Section(cur_title, cur_filenames))
                cur_title = None
                cur_filenames = []

            cur_title = m.group(1)
            continue

        m = re.match(r'^\s+(babeltrace/.+\.h).*$', line)

        if m:
            if m.group(1) != 'babeltrace/babeltrace.h':
                cur_filenames.append(m.group(1))

            continue

        if re.match(r'^noinst_HEADERS.*', line):
            break

    if cur_filenames:
        sections.append(_Section(cur_title, cur_filenames))

    return sections


def _c_includes_from_sections(sections):
    src = ''

    for section in sections:
        src += '/* {} */\n'.format(section.title)

        for filename in sorted(section.filenames):
            src += '#include <{}>\n'.format(filename)

        src += '\n'

    return src[:-1]


def _main():
    with open('include/Makefile.am') as f:
        sections = _get_sections(f)

    print('''#ifndef BABELTRACE_BABELTRACE_H
#define BABELTRACE_BABELTRACE_H

/*
 * Babeltrace API
 *
 * Copyright 2010-2018 EfficiOS Inc. <http://www.efficios.com/>
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
''')
    print(_c_includes_from_sections(sections))
    print('#endif /* BABELTRACE_BABELTRACE_H */')

if __name__ == '__main__':
    _main()
