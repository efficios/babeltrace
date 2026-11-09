/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2015-2017 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Babeltrace CTF file system Reader Component
 */

#include <sstream>

#include <glib.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "common/common.h"
#include "cpp-common/bt2/message.hpp"
#include "cpp-common/bt2/private-query-executor.hpp"
#include "cpp-common/bt2/wrap.hpp"
#include "cpp-common/bt2c/file-utils.hpp"
#include "cpp-common/bt2c/glib-up.hpp"
#include "cpp-common/bt2c/logging.hpp"

#include "plugins/common/param-validation/param-validation.h"

#include "../common/src/metadata/ctf-ir.hpp"
#include "../common/src/metadata/metadata-stream-parser-utils.hpp"
#include "../common/src/metadata/tsdl/ctf-meta-configure-ir-trace.hpp"
#include "../common/src/msg-iter.hpp"
#include "../common/src/pkt-props.hpp"
#include "data-stream-file.hpp"
#include "file.hpp"
#include "fs.hpp"
#include "metadata.hpp"
#include "query.hpp"

using namespace bt2c::literals::datalen;
using namespace ctf::src;
using namespace ctf;

struct tracer_info
{
    const char *name;
    int64_t major;
    int64_t minor;
    int64_t patch;
};

bt_message_iterator_class_next_method_status
ctf_fs_iterator_next(bt_self_message_iterator *iterator, bt_message_array_const msgs,
                     uint64_t capacity, uint64_t *count)
{
    struct ctf_fs_msg_iter_data *msg_iter_data =
        (struct ctf_fs_msg_iter_data *) bt_self_message_iterator_get_data(iterator);
    uint64_t i = 0;

    if (G_UNLIKELY(msg_iter_data->next_saved_error)) {
        /*
         * Last time we were called, we hit an error but had some
         * messages to deliver, so we stashed the error here.  Return
         * it now.
         */
        BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(msg_iter_data->next_saved_error);
        return msg_iter_data->next_saved_status;
    }

    bt_message_iterator_class_next_method_status status =
        BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;

    do {
        try {
            if (auto msg = msg_iter_data->msgIter->next(); G_LIKELY(msg)) {
                msgs[i] = msg.release().libObjPtr();
                ++i;
            } else {
                status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
            }
        } catch (const bt2::Error&) {
            status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
            break;
        } catch (const std::bad_alloc&) {
            status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
            break;
        }
    } while (i < capacity && status == BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK);

    if (i > 0) {
        /*
         * Even if ctf_fs_iterator_next_one() returned something
         * else than BT_MESSAGE_ITERATOR_NEXT_METHOD_STATUS_OK, we
         * accumulated message objects in the output
         * message array, so we need to return
         * BT_MESSAGE_ITERATOR_NEXT_METHOD_STATUS_OK so that they are
         * transferred to downstream. This other status occurs
         * again the next time muxer_msg_iter_do_next() is
         * called, possibly without any accumulated
         * message, in which case we'll return it.
         */
        if (status < 0) {
            /*
             * Save this error for the next _next call.  Assume that
             * this component always appends error causes when
             * returning an error status code, which will cause the
             * current thread error to be non-NULL.
             */
            msg_iter_data->next_saved_error = bt_current_thread_take_error();
            BT_ASSERT(msg_iter_data->next_saved_error);
            msg_iter_data->next_saved_status = status;
        }

        *count = i;
        status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
    }

    return status;
}

static void instantiateMsgIter(ctf_fs_msg_iter_data *msg_iter_data)
{
    ctf_fs_ds_file_group *ds_file_group = msg_iter_data->port_data->ds_file_group;

    Medium::UP medium = std::make_unique<fs::Medium>(ds_file_group->index, msg_iter_data->logger);
    msg_iter_data->msgIter.emplace(msg_iter_data->selfMsgIter, *ds_file_group->ctf_fs_trace->cls(),
                                   ds_file_group->ctf_fs_trace->metadataStreamUuid(),
                                   *ds_file_group->stream, std::move(medium),
                                   msg_iter_data->port_data->ctf_fs->quirks, msg_iter_data->logger);
}

bt_message_iterator_class_seek_beginning_method_status
ctf_fs_iterator_seek_beginning(bt_self_message_iterator *it)
{
    try {
        struct ctf_fs_msg_iter_data *msg_iter_data =
            (struct ctf_fs_msg_iter_data *) bt_self_message_iterator_get_data(it);

        BT_ASSERT(msg_iter_data);

        instantiateMsgIter(msg_iter_data);

        return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2::Error&) {
        return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_ERROR;
    }
}

void ctf_fs_iterator_finalize(bt_self_message_iterator *it)
{
    ctf_fs_msg_iter_data::UP {
        static_cast<ctf_fs_msg_iter_data *>(bt_self_message_iterator_get_data(it))};
}

bt_message_iterator_class_initialize_method_status
ctf_fs_iterator_init(bt_self_message_iterator *self_msg_iter,
                     bt_self_message_iterator_configuration *config,
                     bt_self_component_port_output *self_port)
{
    try {
        ctf_fs_port_data *port_data = (struct ctf_fs_port_data *) bt_self_component_port_get_data(
            bt_self_component_port_output_as_self_component_port(self_port));
        BT_ASSERT(port_data);

        auto msg_iter_data = std::make_unique<ctf_fs_msg_iter_data>(bt2::wrap(self_msg_iter));
        msg_iter_data->port_data = port_data;

        instantiateMsgIter(msg_iter_data.get());

        /*
         * This iterator can seek forward if its stream class has a default
         * clock class.
         */
        if (msg_iter_data->port_data->ds_file_group->dataStreamCls->defClkCls()) {
            bt_self_message_iterator_configuration_set_can_seek_forward(config, true);
        }

        bt_self_message_iterator_set_data(self_msg_iter, msg_iter_data.release());

        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2::Error&) {
        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
    }
}

void ctf_fs_finalize(bt_self_component_source *component)
{
    ctf_fs_component::UP {static_cast<ctf_fs_component *>(
        bt_self_component_get_data(bt_self_component_source_as_self_component(component)))};
}

std::string ctf_fs_make_port_name(ctf_fs_ds_file_group *ds_file_group)
{
    std::stringstream name;

    /*
     * The unique port name is generated by concatenating unique identifiers
     * for:
     *
     *   - the trace
     *   - the stream class
     *   - the stream
     */

    /* For the trace, use the NS/name/UID if present, else the path. */
    if (ds_file_group->ctf_fs_trace->cls()->name() && ds_file_group->ctf_fs_trace->cls()->uid()) {
        name << '{';

        if (ds_file_group->ctf_fs_trace->cls()->ns()) {
            name << fmt::format("namespace: `{}`, ", *ds_file_group->ctf_fs_trace->cls()->ns());
        }

        name << fmt::format("name: `{}`, uid: `{}`}}", *ds_file_group->ctf_fs_trace->cls()->name(),
                            *ds_file_group->ctf_fs_trace->cls()->uid());
    } else {
        name << ds_file_group->ctf_fs_trace->path;
    }

    /*
     * For the stream class, use the id if present.  We can omit this field
     * otherwise, as there will only be a single stream class.
     */
    if (ds_file_group->dataStreamCls->id() != UINT64_C(-1)) {
        name << " | " << ds_file_group->dataStreamCls->id();
    }

    /* For the stream, use the id if present, else, use the path. */
    if (ds_file_group->stream_id != UINT64_C(-1)) {
        name << " | " << ds_file_group->stream_id;
    } else {
        BT_ASSERT(ds_file_group->ds_file_infos.size() == 1);
        const auto& ds_file_info = *ds_file_group->ds_file_infos[0];
        name << " | " << ds_file_info.path();
    }

    return name.str();
}

static int create_one_port_for_trace(struct ctf_fs_component *ctf_fs,
                                     struct ctf_fs_ds_file_group *ds_file_group,
                                     const bt2::SelfSourceComponent selfSrcComp)
{
    const auto port_name = ctf_fs_make_port_name(ds_file_group);
    auto port_data = std::make_unique<ctf_fs_port_data>();

    BT_CPPLOGI_SPEC(ctf_fs->logger, "Creating one port named `{}`", port_name);

    port_data->ctf_fs = ctf_fs;
    port_data->ds_file_group = ds_file_group;

    int ret = bt_self_component_source_add_output_port(selfSrcComp.libObjPtr(), port_name.c_str(),
                                                       port_data.get(), NULL);
    if (ret) {
        return ret;
    }

    ctf_fs->port_data.emplace_back(std::move(port_data));
    return 0;
}

static int create_ports_for_trace(struct ctf_fs_component *ctf_fs,
                                  struct ctf_fs_trace *ctf_fs_trace,
                                  const bt2::SelfSourceComponent selfSrcComp)
{
    /* Create one output port for each stream file group */
    for (const auto& ds_file_group : ctf_fs_trace->ds_file_groups) {
        if (const auto ret = create_one_port_for_trace(ctf_fs, ds_file_group.get(), selfSrcComp);
            ret) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(ctf_fs->logger, "Cannot create output port.");
            return ret;
        }
    }

    return 0;
}

static bool ds_index_entries_equal(const ctf_fs_ds_index_entry& left,
                                   const ctf_fs_ds_index_entry& right)
{
    if (left.packetSize != right.packetSize) {
        return false;
    }

    if (left.timestamp_begin != right.timestamp_begin) {
        return false;
    }

    if (left.timestamp_end != right.timestamp_end) {
        return false;
    }

    if (left.packet_seq_num != right.packet_seq_num) {
        return false;
    }

    return true;
}

/*
 * Insert `entry` into `index`, without duplication.
 *
 * The entry is inserted only if there isn't an identical entry already.
 */

static void ds_index_insert_ds_index_entry_sorted(ctf_fs_ds_index& index,
                                                  const ctf_fs_ds_index_entry& entry)
{
    /* Find the spot where to insert this index entry. */
    auto otherEntry = index.entries.begin();
    for (; otherEntry != index.entries.end(); ++otherEntry) {
        if (entry.timestamp_begin <= otherEntry->timestamp_begin) {
            break;
        }
    }

    /*
     * Insert the entry only if a duplicate doesn't already exist.
     *
     * There can be duplicate packets if reading multiple overlapping
     * snapshots of the same trace.  We then want the index to contain
     * a reference to only one copy of that packet.
     */
    if (otherEntry == index.entries.end() || !ds_index_entries_equal(entry, *otherEntry)) {
        index.entries.emplace(otherEntry, entry);
    }
}

static void merge_ctf_fs_ds_indexes(ctf_fs_ds_index& dest, const ctf_fs_ds_index& src)
{
    for (const auto& entry : src.entries) {
        ds_index_insert_ds_index_entry_sorted(dest, entry);
    }
}

static int add_ds_file_to_ds_file_group(struct ctf_fs_trace *ctf_fs_trace, const char *path,
                                        const bt2c::Logger& logger)
{
    auto ds_file_info = std::make_unique<ctf_fs_ds_file_info>(path, logger);
    const auto& traceCls = *ctf_fs_trace->cls();
    ctf_fs_ds_index tempIndex;
    ctf_fs_ds_index_entry tempIndexEntry {path, 0_bytes, ds_file_info->size()};

    tempIndex.entries.emplace_back(tempIndexEntry);

    const auto props =
        readPktProps(traceCls, std::make_unique<fs::Medium>(tempIndex, logger), 0_bytes, logger);
    const auto sc = props.dataStreamCls;

    BT_ASSERT(sc);

    std::optional<unsigned long long> stream_instance_id = props.dataStreamId;

    int64_t begin_ns = -1;
    if (props.snapshots.beginDefClk) {
        BT_ASSERT(sc->defClkCls());
        int ret = bt_util_clock_cycles_to_ns_from_origin(
            *props.snapshots.beginDefClk, sc->defClkCls()->freq(),
            sc->defClkCls()->offsetFromOrigin().seconds(),
            sc->defClkCls()->offsetFromOrigin().cycles(), &begin_ns);
        if (ret) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(
                logger, "Cannot convert clock cycles to nanoseconds from origin (`{}`).", path);
            return ret;
        }
    }

    auto index = ctf_fs_ds_file_build_index(*ds_file_info, traceCls);
    if (!index) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(logger, "Failed to index CTF stream file \'{}\'", path);
        return -1;
    }

    if (!stream_instance_id || begin_ns == -1) {
        /*
         * No stream instance ID or no beginning timestamp:
         * create a unique stream file group for this stream
         * file because, even if there's a stream instance ID,
         * there's no timestamp to order the file within its
         * group.
         */
        ctf_fs_trace->ds_file_groups.emplace_back(std::make_unique<ctf_fs_ds_file_group>(
            ctf_fs_trace, *sc, stream_instance_id ? *stream_instance_id : UINT64_C(-1),
            std::move(*index)));
        ctf_fs_trace->ds_file_groups.back()->add_ds_file_info(std::move(ds_file_info));
        return 0;
    }

    /* Find an existing stream file group with this ID */
    ctf_fs_ds_file_group *ds_file_group = NULL;
    for (const auto& candidate : ctf_fs_trace->ds_file_groups) {
        if (candidate->dataStreamCls == sc && candidate->stream_id == stream_instance_id) {
            ds_file_group = candidate.get();
            break;
        }
    }

    if (!ds_file_group) {
        ctf_fs_trace->ds_file_groups.emplace_back(std::make_unique<ctf_fs_ds_file_group>(
            ctf_fs_trace, *sc, static_cast<std::uint64_t>(*stream_instance_id), std::move(*index)));
        ds_file_group = ctf_fs_trace->ds_file_groups.back().get();
    } else {
        merge_ctf_fs_ds_indexes(ds_file_group->index, *index);
    }

    ds_file_group->add_ds_file_info(std::move(ds_file_info));

    return 0;
}

static int create_ds_file_groups(struct ctf_fs_trace *ctf_fs_trace, const bt2c::Logger& logger)
{
    /* Check each file in the path directory, except specific ones */
    GError *error = NULL;
    const bt2c::GDirUP dir {g_dir_open(ctf_fs_trace->path.c_str(), 0, &error)};
    if (!dir) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(logger, "Cannot open directory `{}`: {} (code {})",
                                     ctf_fs_trace->path, error->message, error->code);
        g_error_free(error);
        return -1;
    }

    while (const char *basename = g_dir_read_name(dir.get())) {
        if (strcmp(basename, CTF_FS_METADATA_FILENAME) == 0) {
            /* Ignore the metadata stream. */
            BT_CPPLOGI_SPEC(logger, "Ignoring metadata file `{}" G_DIR_SEPARATOR_S "{}`",
                            ctf_fs_trace->path, basename);
            continue;
        }

        if (basename[0] == '.') {
            BT_CPPLOGI_SPEC(logger, "Ignoring hidden file `{}" G_DIR_SEPARATOR_S "{}`",
                            ctf_fs_trace->path, basename);
            continue;
        }

        /* Create the file. */
        ctf_fs_file file {logger};

        /* Create full path string. */
        file.path = fmt::format("{}" G_DIR_SEPARATOR_S "{}", ctf_fs_trace->path, basename);

        if (!g_file_test(file.path.c_str(), G_FILE_TEST_IS_REGULAR)) {
            BT_CPPLOGI_SPEC(logger, "Ignoring non-regular file `{}`", file.path);
            continue;
        }

        int ret = ctf_fs_file_open(&file, "rb");
        if (ret) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(logger, "Cannot open stream file `{}`", file.path);
            return ret;
        }

        if (file.size == 0) {
            /* Skip empty stream. */
            BT_CPPLOGI_SPEC(logger, "Ignoring empty file `{}`", file.path);
            continue;
        }

        ret = add_ds_file_to_ds_file_group(ctf_fs_trace, file.path.c_str(), logger);
        if (ret) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(logger, "Cannot add stream file `{}` to stream file group",
                                         file.path);
            return ret;
        }
    }

    return 0;
}

static void set_trace_name(const bt2::Trace trace, const char *name_suffix)
{
    std::string name;

    /*
     * Check if we have a trace environment string value named `hostname`.
     * If so, use it as the trace name's prefix.
     */
    if (const auto val = trace.environmentEntry("hostname"); val && val->isString()) {
        name += val->asString().value();

        if (name_suffix) {
            name += G_DIR_SEPARATOR;
        }
    }

    if (name_suffix) {
        name += name_suffix;
    }

    trace.name(name);
}

static ctf_fs_trace::UP
ctf_fs_trace_create(const char *path, const char *name, const ctf::src::ClkClsCfg& clkClsCfg,
                    const bt2::OptionalBorrowedObject<bt2::SelfComponent> selfComp,
                    const bt2c::Logger& logger)
{
    auto ctf_fs_trace = std::make_unique<struct ctf_fs_trace>(clkClsCfg, selfComp, logger);
    const auto metadataPath = fmt::format("{}" G_DIR_SEPARATOR_S CTF_FS_METADATA_FILENAME, path);
    const auto metadataBuf = bt2c::dataFromFile(metadataPath, logger, true);

    if (metadataBuf.empty()) {
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(
            logger, bt2c::Error, "Metadata stream is empty: path=\"{}\"", metadataPath);
    }

    if (getMetadataStreamMajorVersion(metadataBuf) == MetadataStreamMajorVersion::V2 && selfComp &&
        selfComp->graphMipVersion() == 0) {
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(logger, bt2c::Error,
                                               "CTF 2 traces are not supported with MIP version 0; "
                                               "consider creating the graph with MIP version 1: "
                                               "path=\"{}\", name=\"{}\"",
                                               path, bt2c::maybeNull(name));
    }

    ctf_fs_trace->path = path;
    ctf_fs_trace->parseMetadata(metadataBuf);

    BT_ASSERT(ctf_fs_trace->cls());

    if (ctf_fs_trace->cls()->libCls()) {
        bt2::TraceClass traceCls = *ctf_fs_trace->cls()->libCls();
        ctf_fs_trace->trace = traceCls.instantiate();
        ctf_trace_class_configure_ir_trace(
            *ctf_fs_trace->cls(), *ctf_fs_trace->trace,
            bt_self_component_get_graph_mip_version(selfComp->libObjPtr()), logger);
        set_trace_name(*ctf_fs_trace->trace, name);
    }

    if (create_ds_file_groups(ctf_fs_trace.get(), logger)) {
        return nullptr;
    }

    return ctf_fs_trace;
}

static int path_is_ctf_trace(const char *path)
{
    return g_file_test(fmt::format("{}" G_DIR_SEPARATOR_S CTF_FS_METADATA_FILENAME, path).c_str(),
                       G_FILE_TEST_IS_REGULAR);
}

/* Helper for ctf_fs_component_create_ctf_fs_trace, to handle a single path. */

static int ctf_fs_component_create_ctf_fs_trace_one_path(
    struct ctf_fs_component *ctf_fs, const char *path_param, const char *trace_name,
    std::vector<ctf_fs_trace::UP>& traces,
    const bt2::OptionalBorrowedObject<bt2::SelfComponent> selfComp)
{
    bt2c::GStringUP norm_path {bt_common_normalize_path(path_param, NULL)};
    if (!norm_path) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(ctf_fs->logger, "Failed to normalize path: `{}`.", path_param);
        return -1;
    }

    if (const auto ret = path_is_ctf_trace(norm_path->str); ret < 0) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(
            ctf_fs->logger, "Failed to check if path is a CTF trace: path={}", norm_path->str);
        return ret;
    } else if (ret == 0) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(
            ctf_fs->logger, "Path is not a CTF trace (does not contain a metadata file): `{}`.",
            norm_path->str);
        return -1;
    }

    // FIXME: Remove or ifdef for __MINGW32__
    if (strcmp(norm_path->str, "/") == 0) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(ctf_fs->logger, "Opening a trace in `/` is not supported.");
        return -1;
    }

    ctf_fs_trace::UP ctf_fs_trace = ctf_fs_trace_create(
        norm_path->str, trace_name, ctf_fs->clkClsCfg, selfComp, ctf_fs->logger);
    if (!ctf_fs_trace) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(ctf_fs->logger, "Cannot create trace for `{}`.",
                                     norm_path->str);
        return -1;
    }

    traces.emplace_back(std::move(ctf_fs_trace));

    return 0;
}

/*
 * Count the number of stream and event classes defined by this trace's metadata.
 *
 * This is used to determine which metadata is the "latest", out of multiple
 * traces sharing the same UUID.  It is assumed that amongst all these metadatas,
 * a bigger metadata is a superset of a smaller metadata.  Therefore, it is
 * enough to just count the classes.
 */

static unsigned int metadata_count_stream_and_event_classes(struct ctf_fs_trace *trace)
{
    const TraceCls::DataStreamClsSet& dataStreamClasses = trace->cls()->dataStreamClasses();
    unsigned int num = dataStreamClasses.size();

    for (const DataStreamCls::UP& dsc : dataStreamClasses) {
        num += dsc->eventRecordClasses().size();
    }

    return num;
}

/*
 * Merge the src ds_file_group into dest.  This consists of merging their
 * ds_file_infos and indexes.
 */

static void merge_ctf_fs_ds_file_groups(struct ctf_fs_ds_file_group *dest,
                                        ctf_fs_ds_file_group::UP src)
{
    for (auto& ds_file_info : src->ds_file_infos) {
        dest->add_ds_file_info(std::move(ds_file_info));
    }

    /* Merge both indexes. */
    merge_ctf_fs_ds_indexes(dest->index, src->index);
}

/* Merge src_trace's data stream file groups into dest_trace's. */

static int merge_matching_ctf_fs_ds_file_groups(struct ctf_fs_trace *dest_trace,
                                                ctf_fs_trace::UP src_trace)
{
    std::vector<ctf_fs_ds_file_group::UP>& dest = dest_trace->ds_file_groups;
    std::vector<ctf_fs_ds_file_group::UP>& src = src_trace->ds_file_groups;

    /*
     * Save the initial length of dest: we only want to check against the
     * original elements in the inner loop.
     */
    size_t dest_len = dest.size();

    for (auto& src_group : src) {
        struct ctf_fs_ds_file_group *dest_group = NULL;

        /* A stream instance without ID can't match a stream in the other trace.  */
        if (src_group->stream_id != -1) {
            /* Let's search for a matching ds_file_group in the destination.  */
            for (size_t d_i = 0; d_i < dest_len; ++d_i) {
                ctf_fs_ds_file_group *candidate_dest = dest[d_i].get();

                /* Can't match a stream instance without ID.  */
                if (candidate_dest->stream_id == -1) {
                    continue;
                }

                /*
                 * If the two groups have the same stream instance id
                 * and belong to the same stream class (stream instance
                 * ids are per-stream class), they represent the same
                 * stream instance.
                 */
                if (candidate_dest->stream_id != src_group->stream_id ||
                    candidate_dest->dataStreamCls->id() != src_group->dataStreamCls->id()) {
                    continue;
                }

                dest_group = candidate_dest;
                break;
            }
        }

        /*
         * Didn't find a friend in dest to merge our src_group into?
         * Create a new empty one. This can happen if a stream was
         * active in the source trace chunk but not in the destination
         * trace chunk.
         */
        if (!dest_group) {
            const DataStreamCls *sc = (*dest_trace->cls())[src_group->dataStreamCls->id()];
            BT_ASSERT(sc);

            dest_trace->ds_file_groups.emplace_back(std::make_unique<ctf_fs_ds_file_group>(
                dest_trace, *sc, src_group->stream_id, ctf_fs_ds_index {}));
            dest_group = dest_trace->ds_file_groups.back().get();
        }

        BT_ASSERT(dest_group);
        merge_ctf_fs_ds_file_groups(dest_group, std::move(src_group));
    }

    return 0;
}

/*
 * Collapse the given traces, which must all share the same UUID, in a single
 * one.
 *
 * The trace with the most expansive metadata is chosen and all other traces
 * are merged into that one.  On return, the elements of `traces` are nullptr
 * and the merged trace is placed in `out_trace`.
 */

static int merge_ctf_fs_traces(std::vector<ctf_fs_trace::UP> traces, ctf_fs_trace::UP& out_trace)
{
    BT_ASSERT(traces.size() >= 2);

    unsigned int winner_count = metadata_count_stream_and_event_classes(traces[0].get());
    ctf_fs_trace *winner = traces[0].get();
    std::size_t winner_i = 0;

    /* Find the trace with the largest metadata. */
    for (std::size_t i = 1; i < traces.size(); i++) {
        ctf_fs_trace *candidate = traces[i].get();
        unsigned int candidate_count;

        candidate_count = metadata_count_stream_and_event_classes(candidate);

        if (candidate_count > winner_count) {
            winner_count = candidate_count;
            winner = candidate;
            winner_i = i;
        }
    }

    /* Merge all the other traces in the winning trace. */
    for (ctf_fs_trace::UP& trace : traces) {
        /* Don't merge the winner into itself. */
        if (trace.get() == winner) {
            continue;
        }

        /* Merge trace's data stream file groups into winner's. */
        if (const auto ret = merge_matching_ctf_fs_ds_file_groups(winner, std::move(trace))) {
            return ret;
        }
    }

    /*
     * Move the winner out of the array, into `*out_trace`.
     */
    out_trace = std::move(traces[winner_i]);

    return 0;
}

struct ClockSnapshotAfterEventItemVisitor : public ItemVisitor
{
    bool done() const
    {
        return _mDone;
    }

    std::optional<unsigned long long> result() const
    {
        return _mResult;
    }

protected:
    std::optional<unsigned long long> _mResult;
    bool _mDone = false;
};

struct ClockSnapshotAfterFirstEventItemVisitor : public ClockSnapshotAfterEventItemVisitor
{
    void visit(const EventRecordInfoItem& item) override
    {
        _mResult = item.defClkVal();
        _mDone = true;
    }
};

/*
 * Find the timestamp of the last event of the packet, if any, otherwise
 * find the timestamp of the beginning of the packet.
 */
struct ClockSnapshotAfterLastEventItemVisitor : public ClockSnapshotAfterEventItemVisitor
{
    void visit(const PktInfoItem& item) override
    {
        _mLastSeen = item.beginDefClkVal();
    }

    void visit(const EventRecordInfoItem& item) override
    {
        _mLastSeen = item.defClkVal();
    }

    void visit(const PktEndItem&) override
    {
        _mResult = _mLastSeen;
        _mDone = true;
    }

private:
    std::optional<unsigned long long> _mLastSeen;
};

static int decode_clock_snapshot_after_event(struct ctf_fs_trace *ctf_fs_trace,
                                             const ctf_fs_ds_index_entry& index_entry,
                                             ClockSnapshotAfterEventItemVisitor& visitor,
                                             const char *firstOrLast, const bt2c::Logger& logger,
                                             uint64_t *cs)
{
    BT_ASSERT(ctf_fs_trace);
    BT_ASSERT(ctf_fs_trace->cls());
    BT_ASSERT(index_entry.path);

    ctf_fs_ds_index tempIndex;

    tempIndex.entries.emplace_back(index_entry);

    ItemSeqIter itemSeqIter {std::make_unique<fs::Medium>(tempIndex, logger), *ctf_fs_trace->cls(),
                             index_entry.offsetInFile, logger};
    LoggingItemVisitor loggingVisitor {logger};

    while (!visitor.done()) {
        const Item *item = itemSeqIter.next();
        BT_ASSERT(item);

        if (logger.wouldLogT()) {
            item->accept(loggingVisitor);
        }

        item->accept(visitor);
    }

    if (!visitor.result()) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(logger, "Failed to get {} event clock snapshot.", firstOrLast);
        return -1;
    }

    *cs = *visitor.result();
    return 0;
}

static int decode_packet_first_event_timestamp(struct ctf_fs_trace *ctf_fs_trace,
                                               const ctf_fs_ds_index_entry& index_entry,
                                               const bt2c::Logger& logger, uint64_t *cs)
{
    ClockSnapshotAfterFirstEventItemVisitor visitor {};

    return decode_clock_snapshot_after_event(ctf_fs_trace, index_entry, visitor, "first", logger,
                                             cs);
}

static int decode_packet_last_event_timestamp(struct ctf_fs_trace *ctf_fs_trace,
                                              const ctf_fs_ds_index_entry& index_entry,
                                              const bt2c::Logger& logger, uint64_t *cs)
{
    ClockSnapshotAfterLastEventItemVisitor visitor {};

    return decode_clock_snapshot_after_event(ctf_fs_trace, index_entry, visitor, "last", logger,
                                             cs);
}

/*
 * Fix up packet index entries for lttng's "event-after-packet" bug.
 * Some buggy lttng tracer versions may emit events with a timestamp that is
 * larger (after) than the timestamp_end of the their packets.
 *
 * To fix up this erroneous data we do the following:
 *  1. If it's not the stream file's last packet: set the packet index entry's
 *     end time to the next packet's beginning time.
 *  2. If it's the stream file's last packet, set the packet index entry's end
 *     time to the packet's last event's time, if any, or to the packet's
 *     beginning time otherwise.
 *
 * Known buggy tracer versions:
 *  - before lttng-ust 2.11.0
 *  - before lttng-module 2.11.0
 *  - before lttng-module 2.10.10
 *  - before lttng-module 2.9.13
 */
static int fix_index_lttng_event_after_packet_bug(struct ctf_fs_trace *trace,
                                                  const bt2c::Logger& logger)
{
    for (const auto& ds_file_group : trace->ds_file_groups) {
        BT_ASSERT(ds_file_group);
        auto& index = ds_file_group->index;

        BT_ASSERT(!index.entries.empty());

        /*
         * Iterate over all entries but the last one. The last one is
         * fixed differently after.
         */
        for (size_t entry_i = 0; entry_i < index.entries.size() - 1; ++entry_i) {
            auto& curr_entry = index.entries[entry_i];
            const auto& next_entry = index.entries[entry_i + 1];

            /*
             * 1. Set the current index entry `end` timestamp to
             * the next index entry `begin` timestamp.
             */
            curr_entry.timestamp_end = next_entry.timestamp_begin;
        }

        /*
         * 2. Fix the last entry by decoding the last event of the last
         * packet.
         */
        auto& last_entry = index.entries.back();

        /*
         * Decode packet to read the timestamp of the last event of the
         * entry.
         */
        int ret = decode_packet_last_event_timestamp(trace, last_entry, logger,
                                                     &last_entry.timestamp_end);
        if (ret) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(
                logger,
                "Failed to decode stream's last packet to get its last event's clock snapshot.");
            return ret;
        }
    }

    return 0;
}

/*
 * Fix up packet index entries for barectf's "event-before-packet" bug.
 * Some buggy barectf tracer versions may emit events with a timestamp that is
 * less than the timestamp_begin of the their packets.
 *
 * To fix up this erroneous data we do the following:
 *  1. Starting at the second index entry, set the timestamp_begin of the
 *     current entry to the timestamp of the first event of the packet.
 *  2. Set the previous entry's timestamp_end to the timestamp_begin of the
 *     current packet.
 *
 * Known buggy tracer versions:
 *  - before barectf 2.3.1
 */
static int fix_index_barectf_event_before_packet_bug(struct ctf_fs_trace *trace,
                                                     const bt2c::Logger& logger)
{
    for (const auto& ds_file_group : trace->ds_file_groups) {
        auto& index = ds_file_group->index;

        BT_ASSERT(!index.entries.empty());

        /*
         * 1. Iterate over the index, starting from the second entry
         * (index = 1).
         */
        for (size_t entry_i = 1; entry_i < index.entries.size(); ++entry_i) {
            auto& prev_entry = index.entries[entry_i - 1];
            auto& curr_entry = index.entries[entry_i];
            /*
             * 2. Set the current entry `begin` timestamp to the
             * timestamp of the first event of the current packet.
             */
            int ret = decode_packet_first_event_timestamp(trace, curr_entry, logger,
                                                          &curr_entry.timestamp_begin);
            if (ret) {
                BT_CPPLOGE_APPEND_CAUSE_SPEC(logger,
                                             "Failed to decode first event's clock snapshot");
                return ret;
            }

            /*
             * 3. Set the previous entry `end` timestamp to the
             * timestamp of the first event of the current packet.
             */
            prev_entry.timestamp_end = curr_entry.timestamp_begin;
        }
    }

    return 0;
}

/*
 * When using the lttng-crash feature it's likely that the last packets of each
 * stream have their timestamp_end set to zero. This is caused by the fact that
 * the tracer crashed and was not able to properly close the packets.
 *
 * To fix up this erroneous data we do the following:
 * For each index entry, if the entry's timestamp_end is 0 and the
 * timestamp_begin is not 0:
 *  - If it's the stream file's last packet: set the packet index entry's end
 *    time to the packet's last event's time, if any, or to the packet's
 *    beginning time otherwise.
 *  - If it's not the stream file's last packet: set the packet index
 *    entry's end time to the next packet's beginning time.
 *
 * Affected versions:
 * - All current and future lttng-ust and lttng-modules versions.
 */
static int fix_index_lttng_crash_quirk(struct ctf_fs_trace *trace, const bt2c::Logger& logger)
{
    for (const auto& ds_file_group : trace->ds_file_groups) {
        BT_ASSERT(ds_file_group);
        auto& index = ds_file_group->index;

        BT_ASSERT(!index.entries.empty());

        /* 1. Fix the last entry first. */
        if (auto& last_entry = index.entries.back();
            last_entry.timestamp_end == 0 && last_entry.timestamp_begin != 0) {
            /*
             * Decode packet to read the timestamp of the
             * last event of the stream file.
             */
            int ret = decode_packet_last_event_timestamp(trace, last_entry, logger,
                                                         &last_entry.timestamp_end);
            if (ret) {
                BT_CPPLOGE_APPEND_CAUSE_SPEC(logger,
                                             "Failed to decode last event's clock snapshot");
                return ret;
            }
        }

        /* Iterate over all entries but the last one. */
        for (size_t entry_idx = 0; entry_idx < index.entries.size() - 1; ++entry_idx) {
            auto& curr_entry = index.entries[entry_idx];
            const auto& next_entry = index.entries[entry_idx + 1];

            if (curr_entry.timestamp_end == 0 && curr_entry.timestamp_begin != 0) {
                /*
                 * 2. Set the current index entry `end` timestamp to
                 * the next index entry `begin` timestamp.
                 */
                curr_entry.timestamp_end = next_entry.timestamp_begin;
            }
        }
    }

    return 0;
}

/*
 * Extract the tracer information necessary to compare versions.
 * Returns 0 on success, and -1 if the extraction is not successful because the
 * necessary fields are absents in the trace metadata.
 */
static int extract_tracer_info(struct ctf_fs_trace *trace, struct tracer_info *current_tracer_info)
{
    if (!trace->cls()->env()) {
        return -1;
    }

    bt2::ConstMapValue env = *trace->cls()->env();

    /* Clear the current_tracer_info struct */
    memset(current_tracer_info, 0, sizeof(*current_tracer_info));

    /*
     * To compare 2 tracer versions, at least the tracer name and it's
     * major version are needed. If one of these is missing, consider it an
     * extraction failure.
     */
    bt2::OptionalBorrowedObject<bt2::ConstValue> tracerName = env["tracer_name"];
    if (!tracerName || !tracerName->isString()) {
        return -1;
    }

    /* Set tracer name. */
    current_tracer_info->name = tracerName->asString().value();

    bt2::OptionalBorrowedObject<bt2::ConstValue> tracerMajor = env["tracer_major"];
    if (!tracerMajor || !tracerMajor->isInteger()) {
        return -1;
    }

    /* Set major version number. */
    current_tracer_info->major =
        tracerMajor->isSignedInteger() ?
            tracerMajor->asSignedInteger().value() :
            static_cast<std::int64_t>(tracerMajor->asUnsignedInteger().value());

    bt2::OptionalBorrowedObject<bt2::ConstValue> tracerMinor = env["tracer_minor"];
    if (!tracerMinor || !tracerMinor->isInteger()) {
        return 0;
    }

    /* Set minor version number. */
    current_tracer_info->minor =
        tracerMinor->isSignedInteger() ?
            tracerMinor->asSignedInteger().value() :
            static_cast<std::int64_t>(tracerMinor->asUnsignedInteger().value());

    /*
     * If `tracer_patch` doesn't exist `tracer_patchlevel` might.
     * For example, `lttng-modules` uses entry name `tracer_patchlevel`.
     */
    bt2::OptionalBorrowedObject<bt2::ConstValue> tracerPatch = env["tracer_patch"];
    if (!tracerPatch) {
        tracerPatch = env["tracer_patchlevel"];
    }

    if (!tracerPatch || !tracerPatch->isInteger()) {
        return 0;
    }

    /* Set patch version number. */
    current_tracer_info->patch =
        tracerPatch->isSignedInteger() ?
            tracerPatch->asSignedInteger().value() :
            static_cast<std::int64_t>(tracerPatch->asUnsignedInteger().value());

    return 0;
}

static bool is_tracer_affected_by_lttng_event_after_packet_bug(struct tracer_info *curr_tracer_info)
{
    bool is_affected = false;

    if (strcmp(curr_tracer_info->name, "lttng-ust") == 0) {
        if (curr_tracer_info->major < 2) {
            is_affected = true;
        } else if (curr_tracer_info->major == 2) {
            /* fixed in lttng-ust 2.11.0 */
            if (curr_tracer_info->minor < 11) {
                is_affected = true;
            }
        }
    } else if (strcmp(curr_tracer_info->name, "lttng-modules") == 0) {
        if (curr_tracer_info->major < 2) {
            is_affected = true;
        } else if (curr_tracer_info->major == 2) {
            /* fixed in lttng-modules 2.11.0 */
            if (curr_tracer_info->minor == 10) {
                /* fixed in lttng-modules 2.10.10 */
                if (curr_tracer_info->patch < 10) {
                    is_affected = true;
                }
            } else if (curr_tracer_info->minor == 9) {
                /* fixed in lttng-modules 2.9.13 */
                if (curr_tracer_info->patch < 13) {
                    is_affected = true;
                }
            } else if (curr_tracer_info->minor < 9) {
                is_affected = true;
            }
        }
    }

    return is_affected;
}

static bool
is_tracer_affected_by_barectf_event_before_packet_bug(struct tracer_info *curr_tracer_info)
{
    bool is_affected = false;

    if (strcmp(curr_tracer_info->name, "barectf") == 0) {
        if (curr_tracer_info->major < 2) {
            is_affected = true;
        } else if (curr_tracer_info->major == 2) {
            if (curr_tracer_info->minor < 3) {
                is_affected = true;
            } else if (curr_tracer_info->minor == 3) {
                /* fixed in barectf 2.3.1 */
                if (curr_tracer_info->patch < 1) {
                    is_affected = true;
                }
            }
        }
    }

    return is_affected;
}

static bool is_tracer_affected_by_lttng_crash_quirk(struct tracer_info *curr_tracer_info)
{
    bool is_affected = false;

    /* All LTTng tracer may be affected by this lttng crash quirk. */
    if (strcmp(curr_tracer_info->name, "lttng-ust") == 0) {
        is_affected = true;
    } else if (strcmp(curr_tracer_info->name, "lttng-modules") == 0) {
        is_affected = true;
    }

    return is_affected;
}

/*
 * Looks for trace produced by known buggy tracers and fix up the index
 * produced earlier.
 */
static int fix_packet_index_tracer_bugs(ctf_fs_component *ctf_fs)
{
    struct tracer_info current_tracer_info;

    int ret = extract_tracer_info(ctf_fs->trace.get(), &current_tracer_info);
    if (ret) {
        /*
         * A trace may not have all the necessary environment
         * entries to do the tracer version comparison.
         * At least, the tracer name and major version number
         * are needed. Failing to extract these entries is not
         * an error.
         */
        BT_CPPLOGI_SPEC(
            ctf_fs->logger,
            "Cannot extract tracer information necessary to compare with buggy versions.");
        return 0;
    }

    /* Check if the trace may be affected by old tracer bugs. */
    if (is_tracer_affected_by_lttng_event_after_packet_bug(&current_tracer_info)) {
        BT_CPPLOGI_SPEC(ctf_fs->logger,
                        "Trace may be affected by LTTng tracer packet timestamp bug. Fixing up.");
        ret = fix_index_lttng_event_after_packet_bug(ctf_fs->trace.get(), ctf_fs->logger);
        if (ret) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(ctf_fs->logger,
                                         "Failed to fix LTTng event-after-packet bug.");
            return ret;
        }
        ctf_fs->quirks.eventRecordDefClkValGtNextPktBeginDefClkVal = true;
    }

    if (is_tracer_affected_by_barectf_event_before_packet_bug(&current_tracer_info)) {
        BT_CPPLOGI_SPEC(ctf_fs->logger,
                        "Trace may be affected by barectf tracer packet timestamp bug. Fixing up.");
        ret = fix_index_barectf_event_before_packet_bug(ctf_fs->trace.get(), ctf_fs->logger);
        if (ret) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(ctf_fs->logger,
                                         "Failed to fix barectf event-before-packet bug.");
            return ret;
        }
        ctf_fs->quirks.eventRecordDefClkValLtPktBeginDefClkVal = true;
    }

    if (is_tracer_affected_by_lttng_crash_quirk(&current_tracer_info)) {
        ret = fix_index_lttng_crash_quirk(ctf_fs->trace.get(), ctf_fs->logger);
        if (ret) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(ctf_fs->logger,
                                         "Failed to fix lttng-crash timestamp quirks.");
            return ret;
        }
        ctf_fs->quirks.pktEndDefClkValZero = true;
    }

    return 0;
}

static bool compare_ds_file_groups_by_first_path(const ctf_fs_ds_file_group::UP& ds_file_group_a,
                                                 const ctf_fs_ds_file_group::UP& ds_file_group_b)
{
    BT_ASSERT(!ds_file_group_a->ds_file_infos.empty());
    BT_ASSERT(!ds_file_group_b->ds_file_infos.empty());

    const auto& first_ds_file_info_a = *ds_file_group_a->ds_file_infos[0];
    const auto& first_ds_file_info_b = *ds_file_group_b->ds_file_infos[0];

    return first_ds_file_info_a.path() < first_ds_file_info_b.path();
}

int ctf_fs_component_create_ctf_fs_trace(
    struct ctf_fs_component *ctf_fs, const bt2::ConstArrayValue pathsValue, const char *traceName,
    const bt2::OptionalBorrowedObject<bt2::SelfComponent> selfComp)
{
    std::vector<std::string> paths;

    BT_ASSERT(!pathsValue.isEmpty());

    /*
     * Create a sorted array of the paths, to make the execution of this
     * component deterministic.
     */
    for (const auto pathValue : pathsValue) {
        BT_ASSERT(pathValue.isString());
        paths.emplace_back(pathValue.asString().value().str());
    }

    std::sort(paths.begin(), paths.end());

    /* Create a separate ctf_fs_trace object for each path. */
    std::vector<ctf_fs_trace::UP> traces;
    for (const auto& path : paths) {
        int ret = ctf_fs_component_create_ctf_fs_trace_one_path(ctf_fs, path.c_str(), traceName,
                                                                traces, selfComp);
        if (ret) {
            return ret;
        }
    }

    if (traces.size() > 1) {
        const auto firstTrace = traces[0].get();
        const bt2::IdentityView firstTraceId {firstTrace->cls()->ns(), firstTrace->cls()->name(),
                                              firstTrace->cls()->uid()};

        /*
         * We have more than one trace, they must all share the same
         * identity, verify that.
         */
        for (const ctf_fs_trace::UP& thisTrace : traces) {
            const bt2::IdentityView thisTraceId {thisTrace->cls()->ns(), thisTrace->cls()->name(),
                                                 thisTrace->cls()->uid()};

            if (firstTraceId != thisTraceId) {
                BT_CPPLOGE_APPEND_CAUSE_SPEC(
                    ctf_fs->logger,
                    "Multiple traces given, but identities don't match: "
                    "first-trace-ns={}, first-trace-name={}, first-trace-uid={}, "
                    "trace-ns={}, trace-name={}, trace-uid={}, trace-path={}",
                    firstTraceId.nameSpace(), firstTraceId.name(), firstTraceId.uid(),
                    thisTraceId.nameSpace(), thisTraceId.name(), thisTraceId.uid(),
                    thisTrace->path);
                return -1;
            }
        }

        if (const auto ret = merge_ctf_fs_traces(std::move(traces), ctf_fs->trace)) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(ctf_fs->logger,
                                         "Failed to merge traces with the same UUID.");
            return ret;
        }
    } else {
        /* Just one trace, it may or may not have a UUID, both are fine. */
        BT_ASSERT(traces.size() == 1);
        ctf_fs->trace = std::move(traces[0]);
    }

    if (const auto ret = fix_packet_index_tracer_bugs(ctf_fs)) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(ctf_fs->logger, "Failed to fix packet index tracer bugs.");
        return ret;
    }

    /*
     * Sort data stream file groups by first data stream file info
     * path to get a deterministic order. This order influences the
     * order of the output ports. It also influences the order of
     * the automatic stream IDs if the trace's packet headers do not
     * contain a `stream_instance_id` field, in which case the data
     * stream file to stream ID association is always the same,
     * whatever the build and the system.
     *
     * Having a deterministic order here can help debugging and
     * testing.
     */
    std::sort(ctf_fs->trace->ds_file_groups.begin(), ctf_fs->trace->ds_file_groups.end(),
              compare_ds_file_groups_by_first_path);

    /*
     * Now that indexes are not going to change anymore, compute each entry's
     * offset in the logical data stream.
     */
    for (ctf_fs_ds_file_group::UP& group : ctf_fs->trace->ds_file_groups) {
        group->index.updateOffsetsInStream();
    }

    return 0;
}

static const std::string&
get_stream_instance_unique_name(struct ctf_fs_ds_file_group *ds_file_group)
{
    /*
     * The first (earliest) stream file's path is used as the stream's unique
     * name.
     */
    BT_ASSERT(!ds_file_group->ds_file_infos.empty());
    return ds_file_group->ds_file_infos[0]->path();
}

/* Create the IR stream objects for ctf_fs_trace. */

static void create_streams_for_trace(struct ctf_fs_trace *ctf_fs_trace)
{
    BT_ASSERT(ctf_fs_trace->trace);

    for (const auto& ds_file_group : ctf_fs_trace->ds_file_groups) {
        BT_ASSERT(ds_file_group->dataStreamCls->libCls());

        const std::string& name = get_stream_instance_unique_name(ds_file_group.get());
        const auto streamCls = *ds_file_group->dataStreamCls->libCls();

        if (ds_file_group->stream_id == UINT64_C(-1)) {
            /* No stream ID: use 0 */
            ds_file_group->stream =
                streamCls.instantiate(*ctf_fs_trace->trace, ctf_fs_trace->next_stream_id);
            ctf_fs_trace->next_stream_id++;
        } else {
            /* Specific stream ID */
            ds_file_group->stream =
                streamCls.instantiate(*ctf_fs_trace->trace, ds_file_group->stream_id);
        }

        ds_file_group->stream->name(name);
    }
}

static const bt_param_validation_value_descr inputs_elem_descr =
    bt_param_validation_value_descr::makeString();

static bt_param_validation_map_value_entry_descr fs_params_entries_descr[] = {
    {"inputs", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
     bt_param_validation_value_descr::makeArray(1, BT_PARAM_VALIDATION_INFINITE,
                                                inputs_elem_descr)},
    {"trace-name", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeString()},
    {"clock-class-offset-s", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeSignedInteger()},
    {"clock-class-offset-ns", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeSignedInteger()},
    {"force-clock-class-origin-unix-epoch", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END};

ctf::src::fs::Parameters read_src_fs_parameters(const bt2::ConstValue params,
                                                const bt2c::Logger& logger)
{
    gchar *error = NULL;
    bt_param_validation_status validate_value_status =
        bt_param_validation_validate(params.libObjPtr(), fs_params_entries_descr, &error);

    if (validate_value_status != BT_PARAM_VALIDATION_STATUS_OK) {
        bt2c::GCharUP errorFreer {error};
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(logger, bt2c::Error, "{}", error);
    }

    ctf::src::fs::Parameters parameters {params["inputs"]->asArray()};

    /* clock-class-offset-s parameter */
    if (const auto clockClassOffsetS = params["clock-class-offset-s"]) {
        parameters.clkClsCfg.offsetSec = clockClassOffsetS->asSignedInteger().value();
    }

    /* clock-class-offset-ns parameter */
    if (const auto clockClassOffsetNs = params["clock-class-offset-ns"]) {
        parameters.clkClsCfg.offsetNanoSec = clockClassOffsetNs->asSignedInteger().value();
    }

    /* force-clock-class-origin-unix-epoch parameter */
    if (const auto forceClockClassOriginUnixEpoch = params["force-clock-class-origin-unix-epoch"]) {
        parameters.clkClsCfg.forceOriginIsUnixEpoch =
            forceClockClassOriginUnixEpoch->asBool().value();
    }

    /* trace-name parameter */
    if (const auto traceName = params["trace-name"]) {
        parameters.traceName = traceName->asString().value().str();
    }

    return parameters;
}

static ctf_fs_component::UP ctf_fs_create(const bt2::ConstMapValue params,
                                          const bt2::SelfSourceComponent selfSrcComp)
{
    const bt2c::Logger logger {selfSrcComp, "PLUGIN/SRC.CTF.FS/COMP"};
    const auto parameters = read_src_fs_parameters(params, logger);
    auto ctf_fs = std::make_unique<ctf_fs_component>(parameters.clkClsCfg, logger);

    if (ctf_fs_component_create_ctf_fs_trace(ctf_fs.get(), parameters.inputs,
                                             parameters.traceName ? parameters.traceName->c_str() :
                                                                    nullptr,
                                             static_cast<bt2::SelfComponent>(selfSrcComp))) {
        return nullptr;
    }

    create_streams_for_trace(ctf_fs->trace.get());

    if (create_ports_for_trace(ctf_fs.get(), ctf_fs->trace.get(), selfSrcComp)) {
        return nullptr;
    }

    return ctf_fs;
}

bt_component_class_get_supported_mip_versions_method_status
ctf_fs_get_supported_mip_versions(bt_self_component_class_source *selfCompClsSrc,
                                  const bt_value *params, void *, bt_logging_level logLevel,
                                  bt_integer_range_set_unsigned *supportedVersionsRaw)
{
    try {
        const bt2c::Logger logger {bt2::wrap(selfCompClsSrc),
                                   static_cast<bt2::LoggingLevel>(logLevel),
                                   "PLUGIN/SRC.CTF.FS/COMP"};
        const auto parameters = read_src_fs_parameters(bt2::ConstMapValue {params}, logger);
        auto ctf_fs = std::make_unique<ctf_fs_component>(parameters.clkClsCfg, logger);

        if (ctf_fs_component_create_ctf_fs_trace(
                ctf_fs.get(), parameters.inputs,
                parameters.traceName ? parameters.traceName->c_str() : nullptr, {})) {
            throw bt2::Error {"Failed to build CTF trace"};
        }

        auto supportedVersions = bt2::wrap(supportedVersionsRaw);

        supportedVersions.addRange(1, 1);

        if (ctf_fs->trace->metadataVersion() == ctf::src::MetadataStreamMajorVersion::V1) {
            supportedVersions.addRange(0, 0);
        }

        return BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2::Error&) {
        return BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_ERROR;
    }
}

bt_component_class_initialize_method_status ctf_fs_init(bt_self_component_source *self_comp_src,
                                                        bt_self_component_source_configuration *,
                                                        const bt_value *params, void *)
{
    try {
        ctf_fs_component::UP ctf_fs =
            ctf_fs_create(bt2::ConstMapValue {params}, bt2::wrap(self_comp_src));
        if (!ctf_fs) {
            return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
        }

        bt_self_component_set_data(bt_self_component_source_as_self_component(self_comp_src),
                                   ctf_fs.release());
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2::Error&) {
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
    }
}

bt_component_class_query_method_status ctf_fs_query(bt_self_component_class_source *comp_class_src,
                                                    bt_private_query_executor *priv_query_exec,
                                                    const char *object, const bt_value *params,
                                                    __attribute__((unused)) void *method_data,
                                                    const bt_value **result)
{
    try {
        bt2c::Logger logger {bt2::SelfComponentClass {comp_class_src},
                             bt2::PrivateQueryExecutor {priv_query_exec},
                             "PLUGIN/SRC.CTF.FS/QUERY"};
        bt2::ConstValue paramsObj(params);
        bt2::Value::Shared resultObj;

        if (strcmp(object, "metadata-info") == 0) {
            resultObj = metadata_info_query(paramsObj, logger);
        } else if (strcmp(object, "babeltrace.trace-infos") == 0) {
            resultObj = trace_infos_query(paramsObj, logger);
        } else if (!strcmp(object, "babeltrace.support-info")) {
            resultObj = support_info_query(paramsObj, logger);
        } else {
            BT_CPPLOGE_SPEC(logger, "Unknown query object `{}`", object);
            return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_UNKNOWN_OBJECT;
        }

        *result = resultObj.release().libObjPtr();

        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2::Error&) {
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
    }
}
