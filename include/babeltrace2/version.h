#ifndef BABELTRACE2_VERSION_H
#define BABELTRACE2_VERSION_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-version Library version

@brief
    Library version getters.

This module contains four functions to information about the library's
version:

<dl>
  <dt>Major version</dt>
  <dd>bt_version_get_major()</dd>

  <dt>Minor version</dt>
  <dd>bt_version_get_minor()</dd>

  <dt>Patch version</dt>
  <dd>bt_version_get_patch()</dd>

  <dt>\bt_dt_opt Development stage</dt>
  <dd>bt_version_get_development_stage()</dd>

  <dt>\bt_dt_opt Version control system revision's description</dt>
  <dd>bt_version_get_vcs_revision_description()</dd>

  <dt>\bt_dt_opt Release name</dt>
  <dd>bt_version_get_name()</dd>

  <dt>\bt_dt_opt Release name's description</dt>
  <dd>bt_version_get_name_description()</dd>
</dl>
*/

/*! @{ */

/*!
@brief
    Returns the major version of libbabeltrace2.

@returns
    Major version of the library.
*/
extern unsigned int bt_version_get_major(void);

/*!
@brief
    Returns the minor version of libbabeltrace2.

@returns
    Minor version of the library.
*/
extern unsigned int bt_version_get_minor(void);

/*!
@brief
    Returns the patch version of libbabeltrace2.

@returns
    Patch version of the library.
*/
extern unsigned int bt_version_get_patch(void);

/*!
@brief
    Returns the development stage of libbabeltrace2's version.

The development stage \em can contain a version suffix such as
<code>-pre5</code> or <code>-rc1</code>.

@returns
    Development stage of the library's version, or \c NULL if none.
*/
extern const char *bt_version_get_development_stage(void);

/*!
@brief
    Returns the version control system (VCS) revision's description of
    libbabeltrace2's version.

The VCS revision description is only available for a non-release build
of the library.

@returns
    Version control system revision's description of the library's
    version, or \c NULL if none.
*/
extern const char *bt_version_get_vcs_revision_description(void);

/*!
@brief
    Returns libbabeltrace2's release name.

If the release name is not available, which can be the case for a
development build, this function returns \c NULL.

@returns
    Library's release name, or \c NULL if not available.

@sa bt_version_get_name_description() &mdash;
    Returns the description of libbabeltrace2's release name.
*/
extern const char *bt_version_get_name(void);

/*!
@brief
    Returns libbabeltrace2's release name's description.

If the release name's description is not available, which can be the
case for a development build, this function returns \c NULL.

@returns
    Library's release name's description, or \c NULL if not available.

@sa bt_version_get_name() &mdash;
    Returns libbabeltrace2's release name.
*/
extern const char *bt_version_get_name_description(void);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_VERSION_H */
