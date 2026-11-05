/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2016 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_CTF_FS_SRC_DATA_STREAM_FILE_HPP
#define BABELTRACE_PLUGINS_CTF_FS_SRC_DATA_STREAM_FILE_HPP

#include <memory>
#include <string>
#include <vector>

#include <glib.h>
#include <stdio.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/trace-ir.hpp"
#include "cpp-common/bt2c/data-len.hpp"
#include "cpp-common/bt2c/logging.hpp"

#include "../common/src/item-seq/medium.hpp"
#include "../common/src/metadata/ctf-ir.hpp"
#include "file.hpp"

struct ctf_fs_ds_file_info
{
    using UP = std::unique_ptr<ctf_fs_ds_file_info>;

    ctf_fs_ds_file_info(std::string path, const bt2c::Logger& parentLogger);

    const bt2c::Logger& logger() const noexcept
    {
        return _mLogger;
    }

    const std::string& path() const noexcept
    {
        return _mPath;
    }

    bt2c::DataLen size() const noexcept
    {
        return _mSize;
    }

private:
    bt2c::Logger _mLogger;
    std::string _mPath;
    bt2c::DataLen _mSize;
};

struct ctf_fs_ds_file
{
    using UP = std::unique_ptr<ctf_fs_ds_file>;

    explicit ctf_fs_ds_file(const bt2c::Logger& parentLogger, const size_t mmapMaxLenParam)
        : logger {parentLogger, "PLUGIN/SRC.CTF.FS/DS"},
          mmap_max_len {mmapMaxLenParam}
    {
    }

    ctf_fs_ds_file(const ctf_fs_ds_file&) = delete;
    ctf_fs_ds_file& operator=(const ctf_fs_ds_file&) = delete;
    ~ctf_fs_ds_file();

    bt2c::Logger logger;

    ctf_fs_file::UP file;

    void *mmap_addr = nullptr;

    /*
     * Max length of chunk to mmap() when updating the current mapping.
     * This value must be page-aligned.
     */
    size_t mmap_max_len = 0;

    /* Length of the current mapping. Never exceeds the file's length. */
    size_t mmap_len = 0;

    /* Offset in the file where the current mapping starts. */
    off_t mmap_offset_in_file = 0;
};

struct ctf_fs_ds_index_entry
{
    ctf_fs_ds_index_entry(const bt2c::CStringView pathParam, const bt2c::DataLen offsetInFileParam,
                          const bt2c::DataLen packetSizeParam)
        : path {pathParam},
          offsetInFile {offsetInFileParam},
          offsetInStream {offsetInFileParam},
          packetSize {packetSizeParam}
    {
        BT_ASSERT(path);
    }

    /* Weak, belongs to ctf_fs_ds_file_info. */
    const char *path;

    /* Position of the packet from the beginning of the file. */
    bt2c::DataLen offsetInFile;

    /*
     * Position of the packet from the beginning of the stream.  Starts equal
     * to `offsetInFile`, but can change when multiple data stream files
     * belonging to the same stream are merged.
     */
    bt2c::DataLen offsetInStream;

    /* Size of the packet. */
    bt2c::DataLen packetSize;

    /*
     * Extracted from the packet context, relative to the respective fields'
     * mapped clock classes (in cycles).
     */
    uint64_t timestamp_begin = 0, timestamp_end = 0;

    /*
     * Packet sequence number, or UINT64_MAX if not present in the index.
     */
    uint64_t packet_seq_num = UINT64_MAX;
};

struct ctf_fs_ds_index
{
    using EntriesT = std::vector<ctf_fs_ds_index_entry>;

    EntriesT entries;

    void updateOffsetsInStream();
};

struct ctf_fs_ds_file_group
{
    using UP = std::unique_ptr<ctf_fs_ds_file_group>;

    explicit ctf_fs_ds_file_group(struct ctf_fs_trace * const ctfFsTrace,
                                  const ctf::src::DataStreamCls& dataStreamClsParam,
                                  const uint64_t streamInstanceId,
                                  ctf_fs_ds_index indexParam) noexcept
        : dataStreamCls {&dataStreamClsParam},
          stream_id {streamInstanceId},
          ctf_fs_trace {ctfFsTrace},
          /* Don't use brace initialization, because of gcc 4.8. */
          index(std::move(indexParam))
    {
    }

    /*
     * Add ds_file_info to the list of ds_file_infos.
     */
    void add_ds_file_info(ctf_fs_ds_file_info::UP ds_file_info);

    /*
     * This is an array of data stream file infos which belong to this
     * group (a single stream instance).
     *
     * You can call ctf_fs_ds_file_create() with one of those paths
     * and the trace IR stream below.
     */
    std::vector<ctf_fs_ds_file_info::UP> ds_file_infos;

    const ctf::src::DataStreamCls *dataStreamCls;

    bt2::Stream::Shared stream;

    /* Stream (instance) ID; -1ULL means none */
    uint64_t stream_id = 0;

    /* Weak, belongs to component */
    struct ctf_fs_trace *ctf_fs_trace = nullptr;

    ctf_fs_ds_index index;
};

ctf_fs_ds_file::UP ctf_fs_ds_file_create(const char *path, const bt2c::Logger& parentLogger);

std::optional<ctf_fs_ds_index> ctf_fs_ds_file_build_index(const ctf_fs_ds_file_info& file_info,
                                                          const ctf::src::TraceCls& traceCls);

namespace ctf::src::fs {

struct Medium : public ctf::src::Medium
{
    explicit Medium(const ctf_fs_ds_index& index, const bt2c::Logger& parentLogger);

    ~Medium() = default;
    Medium(const Medium&) = delete;
    Medium& operator=(const Medium&) = delete;

    ctf::src::Buf buf(bt2c::DataLen offset, bt2c::DataLen minSize) override;

private:
    ctf_fs_ds_index::EntriesT::const_iterator
    _mFindIndexEntryForOffset(bt2c::DataLen offsetInStream) const;

    const ctf_fs_ds_index& _mIndex;
    bt2c::Logger _mLogger;
    ctf_fs_ds_file::UP _mCurrentDsFile;
};

} /* namespace ctf::src::fs */

#endif /* BABELTRACE_PLUGINS_CTF_FS_SRC_DATA_STREAM_FILE_HPP */
