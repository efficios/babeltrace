# SPDX-License-Identifier: MIT
#
# Copyright (C) 2019 Philippe Proulx <pproulx@efficios.com>
#

# pyright: strict, reportTypeCommentUsage=false,  reportMissingTypeStubs=false

import os
import re
import socket
import struct
import logging
import os.path
import argparse
import tempfile
from typing import Dict, Union, Iterable, Optional, Sequence, overload

import tjson

# isort: off
from typing import Any, Callable  # noqa: F401

# isort: on


# An entry within the index of an LTTng data stream.
class _LttngDataStreamIndexEntry:
    def __init__(
        self,
        offset_bytes: int,
        total_size_bits: int,
        content_size_bits: int,
        timestamp_begin: int,
        timestamp_end: int,
        events_discarded: int,
        stream_class_id: int,
    ):
        self._offset_bytes = offset_bytes
        self._total_size_bits = total_size_bits
        self._content_size_bits = content_size_bits
        self._timestamp_begin = timestamp_begin
        self._timestamp_end = timestamp_end
        self._events_discarded = events_discarded
        self._stream_class_id = stream_class_id

    @property
    def offset_bytes(self):
        return self._offset_bytes

    @property
    def total_size_bits(self):
        return self._total_size_bits

    @property
    def total_size_bytes(self):
        return self._total_size_bits // 8

    @property
    def content_size_bits(self):
        return self._content_size_bits

    @property
    def content_size_bytes(self):
        return self._content_size_bits // 8

    @property
    def timestamp_begin(self):
        return self._timestamp_begin

    @property
    def timestamp_end(self):
        return self._timestamp_end

    @property
    def events_discarded(self):
        return self._events_discarded

    @property
    def stream_class_id(self):
        return self._stream_class_id


# An entry within the index of an LTTng data stream. While a stream beacon entry
# is conceptually unrelated to an index, it is sent as a reply to a
# LttngLiveViewerGetNextDataStreamIndexEntryCommand
class _LttngDataStreamBeaconIndexEntry:
    def __init__(self, stream_class_id: int, timestamp: int):
        self._stream_class_id = stream_class_id
        self._timestamp = timestamp

    @property
    def timestamp(self):
        return self._timestamp

    @property
    def stream_class_id(self):
        return self._stream_class_id


_LttngIndexEntryT = Union[_LttngDataStreamIndexEntry, _LttngDataStreamBeaconIndexEntry]


class _LttngLiveViewerCommand:
    def __init__(self, version: int):
        self._version = version

    @property
    def version(self):
        return self._version


class _LttngLiveViewerConnectCommand(_LttngLiveViewerCommand):
    def __init__(self, version: int, viewer_session_id: int, major: int, minor: int):
        super().__init__(version)
        self._viewer_session_id = viewer_session_id
        self._major = major
        self._minor = minor

    @property
    def viewer_session_id(self):
        return self._viewer_session_id

    @property
    def major(self):
        return self._major

    @property
    def minor(self):
        return self._minor


class _LttngLiveViewerReply:
    pass


class _LttngLiveViewerConnectReply(_LttngLiveViewerReply):
    def __init__(self, viewer_session_id: int, major: int, minor: int):
        self._viewer_session_id = viewer_session_id
        self._major = major
        self._minor = minor

    @property
    def viewer_session_id(self):
        return self._viewer_session_id

    @property
    def major(self):
        return self._major

    @property
    def minor(self):
        return self._minor


class _LttngLiveViewerGetTracingSessionInfosCommand(_LttngLiveViewerCommand):
    pass


class _LttngLiveViewerTracingSessionInfo:
    def __init__(
        self,
        tracing_session_id: int,
        live_timer_freq: int,
        client_count: int,
        stream_count: int,
        hostname: str,
        name: str,
    ):
        self._tracing_session_id = tracing_session_id
        self._live_timer_freq = live_timer_freq
        self._client_count = client_count
        self._stream_count = stream_count
        self._hostname = hostname
        self._name = name

    @property
    def tracing_session_id(self):
        return self._tracing_session_id

    @property
    def live_timer_freq(self):
        return self._live_timer_freq

    @property
    def client_count(self):
        return self._client_count

    @property
    def stream_count(self):
        return self._stream_count

    @property
    def hostname(self):
        return self._hostname

    @property
    def name(self):
        return self._name


class _LttngLiveViewerGetTracingSessionInfosReply(_LttngLiveViewerReply):
    def __init__(
        self, tracing_session_infos: Sequence[_LttngLiveViewerTracingSessionInfo]
    ):
        self._tracing_session_infos = tracing_session_infos

    @property
    def tracing_session_infos(self):
        return self._tracing_session_infos


class _LttngLiveViewerAttachToTracingSessionCommand(_LttngLiveViewerCommand):
    class SeekType:
        BEGINNING = 1
        LAST = 2

    def __init__(
        self, version: int, tracing_session_id: int, offset: int, seek_type: int
    ):
        super().__init__(version)
        self._tracing_session_id = tracing_session_id
        self._offset = offset
        self._seek_type = seek_type

    @property
    def tracing_session_id(self):
        return self._tracing_session_id

    @property
    def offset(self):
        return self._offset

    @property
    def seek_type(self):
        return self._seek_type


class _LttngLiveViewerStreamInfo:
    def __init__(
        self, id: int, trace_id: int, is_metadata: bool, path: str, channel_name: str
    ):
        self._id = id
        self._trace_id = trace_id
        self._is_metadata = is_metadata
        self._path = path
        self._channel_name = channel_name

    @property
    def id(self):
        return self._id

    @property
    def trace_id(self):
        return self._trace_id

    @property
    def is_metadata(self):
        return self._is_metadata

    @property
    def path(self):
        return self._path

    @property
    def channel_name(self):
        return self._channel_name


class _LttngLiveViewerAttachToTracingSessionReply(_LttngLiveViewerReply):
    class Status:
        OK = 1
        ALREADY = 2
        UNKNOWN = 3
        NOT_LIVE = 4
        SEEK_ERROR = 5
        NO_SESSION = 6

    def __init__(self, status: int, stream_infos: Sequence[_LttngLiveViewerStreamInfo]):
        self._status = status
        self._stream_infos = stream_infos

    @property
    def status(self):
        return self._status

    @property
    def stream_infos(self):
        return self._stream_infos


class _LttngLiveViewerGetNextDataStreamIndexEntryCommand(_LttngLiveViewerCommand):
    def __init__(self, version: int, stream_id: int):
        super().__init__(version)
        self._stream_id = stream_id

    @property
    def stream_id(self):
        return self._stream_id


class _LttngLiveViewerGetNextDataStreamIndexEntryReply(_LttngLiveViewerReply):
    class Status:
        OK = 1
        RETRY = 2
        HUP = 3
        ERROR = 4
        INACTIVE = 5
        EOF = 6

    def __init__(
        self,
        status: int,
        index_entry: _LttngIndexEntryT,
        has_new_metadata: bool,
        has_new_data_stream: bool,
    ):
        self._status = status
        self._index_entry = index_entry
        self._has_new_metadata = has_new_metadata
        self._has_new_data_stream = has_new_data_stream

    @property
    def status(self):
        return self._status

    @property
    def index_entry(self):
        return self._index_entry

    @property
    def has_new_metadata(self):
        return self._has_new_metadata

    @property
    def has_new_data_stream(self):
        return self._has_new_data_stream


class _LttngLiveViewerGetDataStreamPacketDataCommand(_LttngLiveViewerCommand):
    def __init__(self, version: int, stream_id: int, offset: int, req_length: int):
        super().__init__(version)
        self._stream_id = stream_id
        self._offset = offset
        self._req_length = req_length

    @property
    def stream_id(self):
        return self._stream_id

    @property
    def offset(self):
        return self._offset

    @property
    def req_length(self):
        return self._req_length


class _LttngLiveViewerGetDataStreamPacketDataReply(_LttngLiveViewerReply):
    class Status:
        OK = 1
        RETRY = 2
        ERROR = 3
        EOF = 4

    def __init__(
        self,
        status: int,
        data: bytes,
        has_new_metadata: bool,
        has_new_data_stream: bool,
    ):
        self._status = status
        self._data = data
        self._has_new_metadata = has_new_metadata
        self._has_new_data_stream = has_new_data_stream

    @property
    def status(self):
        return self._status

    @property
    def data(self):
        return self._data

    @property
    def has_new_metadata(self):
        return self._has_new_metadata

    @property
    def has_new_data_stream(self):
        return self._has_new_data_stream


class _LttngLiveViewerGetMetadataStreamDataCommand(_LttngLiveViewerCommand):
    def __init__(self, version: int, stream_id: int):
        super().__init__(version)
        self._stream_id = stream_id

    @property
    def stream_id(self):
        return self._stream_id


class _LttngLiveViewerGetMetadataStreamDataContentReply(_LttngLiveViewerReply):
    class Status:
        OK = 1
        NO_NEW = 2
        ERROR = 3

    def __init__(self, status: int, data: bytes):
        self._status = status
        self._data = data

    @property
    def status(self):
        return self._status

    @property
    def data(self):
        return self._data


class _LttngLiveViewerGetNewStreamInfosCommand(_LttngLiveViewerCommand):
    def __init__(self, version: int, tracing_session_id: int):
        super().__init__(version)
        self._tracing_session_id = tracing_session_id

    @property
    def tracing_session_id(self):
        return self._tracing_session_id


class _LttngLiveViewerGetNewStreamInfosReply(_LttngLiveViewerReply):
    class Status:
        OK = 1
        NO_NEW = 2
        ERROR = 3
        HUP = 4

    def __init__(self, status: int, stream_infos: Sequence[_LttngLiveViewerStreamInfo]):
        self._status = status
        self._stream_infos = stream_infos

    @property
    def status(self):
        return self._status

    @property
    def stream_infos(self):
        return self._stream_infos


class _LttngLiveViewerCreateViewerSessionCommand(_LttngLiveViewerCommand):
    pass


class _LttngLiveViewerCreateViewerSessionReply(_LttngLiveViewerReply):
    class Status:
        OK = 1
        ERROR = 2

    def __init__(self, status: int):
        self._status = status

    @property
    def status(self):
        return self._status


class _LttngLiveViewerDetachFromTracingSessionCommand(_LttngLiveViewerCommand):
    def __init__(self, version: int, tracing_session_id: int):
        super().__init__(version)
        self._tracing_session_id = tracing_session_id

    @property
    def tracing_session_id(self):
        return self._tracing_session_id


class _LttngLiveViewerDetachFromTracingSessionReply(_LttngLiveViewerReply):
    class Status:
        OK = 1
        UNKNOWN = 2
        ERROR = 3

    def __init__(self, status: int):
        self._status = status

    @property
    def status(self):
        return self._status


# An LTTng live protocol codec can convert bytes to command objects and
# reply objects to bytes.
class _LttngLiveViewerProtocolCodec:
    _COMMAND_HEADER_STRUCT_FMT = "QII"
    _COMMAND_HEADER_SIZE_BYTES = struct.calcsize(_COMMAND_HEADER_STRUCT_FMT)

    def __init__(self):
        pass

    def _unpack(self, fmt: str, data: bytes, offset: int = 0):
        fmt = "!" + fmt
        return struct.unpack_from(fmt, data, offset)

    def _unpack_payload(self, fmt: str, data: bytes):
        return self._unpack(
            fmt, data, _LttngLiveViewerProtocolCodec._COMMAND_HEADER_SIZE_BYTES
        )

    def decode(self, data: bytes):
        if len(data) < self._COMMAND_HEADER_SIZE_BYTES:
            # Not enough data to read the command header
            return

        payload_size, cmd_type, version = self._unpack(
            self._COMMAND_HEADER_STRUCT_FMT, data
        )
        logging.info(
            "Decoded command header: payload-size={}, cmd-type={}, version={}".format(
                payload_size, cmd_type, version
            )
        )

        if len(data) < self._COMMAND_HEADER_SIZE_BYTES + payload_size:
            # Not enough data to read the whole command
            return

        if cmd_type == 1:
            viewer_session_id, major, minor, _ = self._unpack_payload("QIII", data)
            return _LttngLiveViewerConnectCommand(
                version, viewer_session_id, major, minor
            )
        elif cmd_type == 2:
            return _LttngLiveViewerGetTracingSessionInfosCommand(version)
        elif cmd_type == 3:
            tracing_session_id, offset, seek_type = self._unpack_payload("QQI", data)
            return _LttngLiveViewerAttachToTracingSessionCommand(
                version, tracing_session_id, offset, seek_type
            )
        elif cmd_type == 4:
            (stream_id,) = self._unpack_payload("Q", data)
            return _LttngLiveViewerGetNextDataStreamIndexEntryCommand(
                version, stream_id
            )
        elif cmd_type == 5:
            stream_id, offset, req_length = self._unpack_payload("QQI", data)
            return _LttngLiveViewerGetDataStreamPacketDataCommand(
                version, stream_id, offset, req_length
            )
        elif cmd_type == 6:
            (stream_id,) = self._unpack_payload("Q", data)
            return _LttngLiveViewerGetMetadataStreamDataCommand(version, stream_id)
        elif cmd_type == 7:
            (tracing_session_id,) = self._unpack_payload("Q", data)
            return _LttngLiveViewerGetNewStreamInfosCommand(version, tracing_session_id)
        elif cmd_type == 8:
            return _LttngLiveViewerCreateViewerSessionCommand(version)
        elif cmd_type == 9:
            (tracing_session_id,) = self._unpack_payload("Q", data)
            return _LttngLiveViewerDetachFromTracingSessionCommand(
                version, tracing_session_id
            )
        else:
            raise RuntimeError("Unknown command type {}".format(cmd_type))

    def _pack(self, fmt: str, *args: Any):
        # Force network byte order
        return struct.pack("!" + fmt, *args)

    def _encode_zero_padded_str(self, string: str, length: int):
        data = string.encode()
        return data.ljust(length, b"\x00")

    def _encode_stream_info(self, info: _LttngLiveViewerStreamInfo):
        data = self._pack("QQI", info.id, info.trace_id, int(info.is_metadata))
        data += self._encode_zero_padded_str(info.path, 4096)
        data += self._encode_zero_padded_str(info.channel_name, 255)
        return data

    def _get_has_new_stuff_flags(
        self, has_new_metadata: bool, has_new_data_streams: bool
    ):
        flags = 0

        if has_new_metadata:
            flags |= 1

        if has_new_data_streams:
            flags |= 2

        return flags

    def encode(
        self,
        reply: _LttngLiveViewerReply,
    ) -> bytes:
        if type(reply) is _LttngLiveViewerConnectReply:
            data = self._pack(
                "QIII", reply.viewer_session_id, reply.major, reply.minor, 2
            )
        elif type(reply) is _LttngLiveViewerGetTracingSessionInfosReply:
            data = self._pack("I", len(reply.tracing_session_infos))

            for info in reply.tracing_session_infos:
                data += self._pack(
                    "QIII",
                    info.tracing_session_id,
                    info.live_timer_freq,
                    info.client_count,
                    info.stream_count,
                )
                data += self._encode_zero_padded_str(info.hostname, 64)
                data += self._encode_zero_padded_str(info.name, 255)
        elif type(reply) is _LttngLiveViewerAttachToTracingSessionReply:
            data = self._pack("II", reply.status, len(reply.stream_infos))

            for info in reply.stream_infos:
                data += self._encode_stream_info(info)
        elif type(reply) is _LttngLiveViewerGetNextDataStreamIndexEntryReply:
            index_format = "QQQQQQQII"
            entry = reply.index_entry
            flags = self._get_has_new_stuff_flags(
                reply.has_new_metadata, reply.has_new_data_stream
            )

            if isinstance(entry, _LttngDataStreamIndexEntry):
                data = self._pack(
                    index_format,
                    entry.offset_bytes,
                    entry.total_size_bits,
                    entry.content_size_bits,
                    entry.timestamp_begin,
                    entry.timestamp_end,
                    entry.events_discarded,
                    entry.stream_class_id,
                    reply.status,
                    flags,
                )
            else:
                data = self._pack(
                    index_format,
                    0,
                    0,
                    0,
                    0,
                    entry.timestamp,
                    0,
                    entry.stream_class_id,
                    reply.status,
                    flags,
                )
        elif type(reply) is _LttngLiveViewerGetDataStreamPacketDataReply:
            flags = self._get_has_new_stuff_flags(
                reply.has_new_metadata, reply.has_new_data_stream
            )
            data = self._pack("III", reply.status, len(reply.data), flags)
            data += reply.data
        elif type(reply) is _LttngLiveViewerGetMetadataStreamDataContentReply:
            data = self._pack("QI", len(reply.data), reply.status)
            data += reply.data
        elif type(reply) is _LttngLiveViewerGetNewStreamInfosReply:
            data = self._pack("II", reply.status, len(reply.stream_infos))

            for info in reply.stream_infos:
                data += self._encode_stream_info(info)
        elif type(reply) is _LttngLiveViewerCreateViewerSessionReply:
            data = self._pack("I", reply.status)
        elif type(reply) is _LttngLiveViewerDetachFromTracingSessionReply:
            data = self._pack("I", reply.status)
        else:
            raise ValueError(
                "Unknown reply object with class `{}`".format(reply.__class__.__name__)
            )

        return data


def _get_entry_timestamp_begin(
    entry: _LttngIndexEntryT,
):
    if isinstance(entry, _LttngDataStreamBeaconIndexEntry):
        return entry.timestamp
    else:
        return entry.timestamp_begin


# The index of an LTTng data stream, a sequence of index entries.
class _LttngDataStreamIndex(Sequence[_LttngIndexEntryT]):
    def __init__(self, path: str, beacons: Optional[tjson.ArrayVal]):
        self._path = path
        self._build()

        if beacons:
            stream_class_id = self._entries[0].stream_class_id

            beacons_list = []  # type: list[_LttngDataStreamBeaconIndexEntry]
            for ts in beacons.iter(tjson.IntVal):
                beacons_list.append(
                    _LttngDataStreamBeaconIndexEntry(stream_class_id, ts.val)
                )

            self._add_beacons(beacons_list)

        logging.info(
            'Built data stream index entries: path="{}", count={}'.format(
                path, len(self._entries)
            )
        )

    def _build(self):
        self._entries = []  # type: list[_LttngIndexEntryT]

        with open(self._path, "rb") as f:
            # Read header first
            fmt = ">IIII"
            size = struct.calcsize(fmt)
            data = f.read(size)
            assert len(data) == size
            magic, _, _, index_entry_length = struct.unpack(fmt, data)
            assert magic == 0xC1F1DCC1

            # Read index entries
            fmt = ">QQQQQQQ"
            size = struct.calcsize(fmt)

            while True:
                logging.debug(
                    'Decoding data stream index entry: path="{}", offset={}'.format(
                        self._path, f.tell()
                    )
                )
                data = f.read(size)

                if not data:
                    # Done
                    break

                assert len(data) == size
                (
                    offset_bytes,
                    total_size_bits,
                    content_size_bits,
                    timestamp_begin,
                    timestamp_end,
                    events_discarded,
                    stream_class_id,
                ) = struct.unpack(fmt, data)

                self._entries.append(
                    _LttngDataStreamIndexEntry(
                        offset_bytes,
                        total_size_bits,
                        content_size_bits,
                        timestamp_begin,
                        timestamp_end,
                        events_discarded,
                        stream_class_id,
                    )
                )

                # Skip anything else before the next entry
                f.seek(index_entry_length - size, os.SEEK_CUR)

    def _add_beacons(self, beacons: Iterable[_LttngDataStreamBeaconIndexEntry]):
        # Assumes entries[n + 1].timestamp_end >= entries[n].timestamp_begin
        def sort_key(
            entry: Union[_LttngDataStreamIndexEntry, _LttngDataStreamBeaconIndexEntry],
        ) -> int:
            if isinstance(entry, _LttngDataStreamBeaconIndexEntry):
                return entry.timestamp
            else:
                return entry.timestamp_end

        self._entries += beacons
        self._entries.sort(key=sort_key)

    @overload
    def __getitem__(self, index: int) -> _LttngIndexEntryT:
        ...

    @overload
    def __getitem__(self, index: slice) -> Sequence[_LttngIndexEntryT]:  # noqa: F811
        ...

    def __getitem__(  # noqa: F811
        self, index: Union[int, slice]
    ) -> Union[_LttngIndexEntryT, Sequence[_LttngIndexEntryT],]:
        return self._entries[index]

    def __len__(self):
        return len(self._entries)

    @property
    def path(self):
        return self._path


# An LTTng data stream.
class _LttngDataStream:
    def __init__(self, path: str, beacons_json: Optional[tjson.ArrayVal]):
        self._path = path
        filename = os.path.basename(path)
        match = re.match(r"(.*)_\d+", filename)
        if not match:
            raise RuntimeError(
                "Unexpected data stream file name pattern: {}".format(filename)
            )

        self._channel_name = match.group(1)
        trace_dir = os.path.dirname(path)
        index_path = os.path.join(trace_dir, "index", filename + ".idx")
        self._index = _LttngDataStreamIndex(index_path, beacons_json)
        assert os.path.isfile(path)
        self._file = open(path, "rb")
        logging.info(
            'Built data stream: path="{}", channel-name="{}"'.format(
                path, self._channel_name
            )
        )

    @property
    def path(self):
        return self._path

    @property
    def channel_name(self):
        return self._channel_name

    @property
    def index(self):
        return self._index

    def get_data(self, offset_bytes: int, len_bytes: int):
        self._file.seek(offset_bytes)
        return self._file.read(len_bytes)


class _LttngMetadataStreamSection:
    def __init__(self, timestamp: int, data: Optional[bytes]):
        self._timestamp = timestamp
        if data is None:
            self._data = bytes()
        else:
            self._data = data
        logging.info(
            "Built metadata stream section: ts={}, data-len={}".format(
                self._timestamp, len(self._data)
            )
        )

    @property
    def timestamp(self):
        return self._timestamp

    @property
    def data(self):
        return self._data


# An LTTng metadata stream.
class _LttngMetadataStream:
    def __init__(
        self,
        metadata_file_path: str,
        config_sections: Sequence[_LttngMetadataStreamSection],
    ):
        self._path = metadata_file_path
        self._sections = config_sections
        logging.info(
            "Built metadata stream: path={}, section-len={}".format(
                self._path, len(self._sections)
            )
        )

    @property
    def path(self):
        return self._path

    @property
    def sections(self):
        return self._sections


class LttngMetadataConfigSection:
    def __init__(self, line: int, timestamp: int, is_empty: bool):
        self._line = line
        self._timestamp = timestamp
        self._is_empty = is_empty

    @property
    def line(self):
        return self._line

    @property
    def timestamp(self):
        return self._timestamp

    @property
    def is_empty(self):
        return self._is_empty


def _parse_metadata_sections_config(metadata_sections_json: tjson.ArrayVal):
    metadata_sections = []  # type: list[LttngMetadataConfigSection]
    append_empty_section = False
    last_timestamp = 0
    last_line = 0

    for section in metadata_sections_json:
        if isinstance(section, tjson.StrVal):
            if section.val == "empty":
                # Found a empty section marker. Actually append the section at the
                # timestamp of the next concrete section.
                append_empty_section = True
            else:
                raise ValueError("Invalid string value at {}.".format(section.path))
        elif isinstance(section, tjson.ObjVal):
            line = section.at("line", tjson.IntVal).val
            ts = section.at("timestamp", tjson.IntVal).val

            # Sections' timestamps and lines must both be increasing.
            assert ts > last_timestamp
            last_timestamp = ts

            assert line > last_line
            last_line = line

            if append_empty_section:
                metadata_sections.append(LttngMetadataConfigSection(line, ts, True))
                append_empty_section = False

            metadata_sections.append(LttngMetadataConfigSection(line, ts, False))
        else:
            raise TypeError(
                "`{}`: expecting a string or object value".format(section.path)
            )

    return metadata_sections


def _split_metadata_sections(
    metadata_file_path: str, metadata_sections_json: tjson.ArrayVal
):
    metadata_sections = _parse_metadata_sections_config(metadata_sections_json)

    sections = []  # type: list[_LttngMetadataStreamSection]
    with open(metadata_file_path, "r") as metadata_file:
        metadata_lines = [line for line in metadata_file]

    metadata_section_idx = 0
    curr_metadata_section = bytearray()

    for idx, line_content in enumerate(metadata_lines):
        # Add one to the index to convert from the zero-indexing of the
        # enumerate() function to the one-indexing used by humans when
        # viewing a text file.
        curr_line_number = idx + 1

        # If there are no more sections, simply append the line.
        if metadata_section_idx + 1 >= len(metadata_sections):
            curr_metadata_section += bytearray(line_content, "utf8")
            continue

        next_section_line_number = metadata_sections[metadata_section_idx + 1].line

        # If the next section begins at the current line, create a
        # section with the metadata we gathered so far.
        if curr_line_number >= next_section_line_number:
            # Flushing the metadata of the current section.
            sections.append(
                _LttngMetadataStreamSection(
                    metadata_sections[metadata_section_idx].timestamp,
                    bytes(curr_metadata_section),
                )
            )

            # Move to the next section.
            metadata_section_idx += 1

            # Clear old content and append current line for the next section.
            curr_metadata_section.clear()
            curr_metadata_section += bytearray(line_content, "utf8")

            # Append any empty sections.
            while metadata_sections[metadata_section_idx].is_empty:
                sections.append(
                    _LttngMetadataStreamSection(
                        metadata_sections[metadata_section_idx].timestamp, None
                    )
                )
                metadata_section_idx += 1
        else:
            # Append line_content to the current metadata section.
            curr_metadata_section += bytearray(line_content, "utf8")

    # We iterated over all the lines of the metadata file. Close the current section.
    sections.append(
        _LttngMetadataStreamSection(
            metadata_sections[metadata_section_idx].timestamp,
            bytes(curr_metadata_section),
        )
    )

    return sections


_StreamBeaconsT = Dict[str, Iterable[int]]


# An LTTng trace, a sequence of LTTng data streams.
class LttngTrace(Sequence[_LttngDataStream]):
    def __init__(
        self,
        trace_dir: str,
        metadata_sections_json: Optional[tjson.ArrayVal],
        beacons_json: Optional[tjson.ObjVal],
    ):
        self._path = trace_dir
        self._create_metadata_stream(trace_dir, metadata_sections_json)
        self._create_data_streams(trace_dir, beacons_json)
        logging.info('Built trace: path="{}"'.format(trace_dir))

    def _create_data_streams(
        self, trace_dir: str, beacons_json: Optional[tjson.ObjVal]
    ):
        data_stream_paths = []  # type: list[str]

        for filename in os.listdir(trace_dir):
            path = os.path.join(trace_dir, filename)

            if not os.path.isfile(path):
                continue

            if filename.startswith("."):
                continue

            if filename == "metadata":
                continue

            data_stream_paths.append(path)

        data_stream_paths.sort()
        self._data_streams = []  # type: list[_LttngDataStream]

        for data_stream_path in data_stream_paths:
            stream_name = os.path.basename(data_stream_path)
            this_beacons_json = None
            if beacons_json is not None and stream_name in beacons_json:
                this_beacons_json = beacons_json.at(stream_name, tjson.ArrayVal)

            self._data_streams.append(
                _LttngDataStream(data_stream_path, this_beacons_json)
            )

    def _create_metadata_stream(
        self, trace_dir: str, metadata_sections_json: Optional[tjson.ArrayVal]
    ):
        metadata_path = os.path.join(trace_dir, "metadata")
        metadata_sections = []  # type: list[_LttngMetadataStreamSection]

        if metadata_sections_json is None:
            with open(metadata_path, "rb") as metadata_file:
                metadata_sections.append(
                    _LttngMetadataStreamSection(0, metadata_file.read())
                )
        else:
            metadata_sections = _split_metadata_sections(
                metadata_path, metadata_sections_json
            )

        self._metadata_stream = _LttngMetadataStream(metadata_path, metadata_sections)

    @property
    def path(self):
        return self._path

    @property
    def metadata_stream(self):
        return self._metadata_stream

    @overload
    def __getitem__(self, index: int) -> _LttngDataStream:
        ...

    @overload
    def __getitem__(self, index: slice) -> Sequence[_LttngDataStream]:  # noqa: F811
        ...

    def __getitem__(  # noqa: F811
        self, index: Union[int, slice]
    ) -> Union[_LttngDataStream, Sequence[_LttngDataStream]]:
        return self._data_streams[index]

    def __len__(self):
        return len(self._data_streams)


# The state of a single data stream.
class _LttngLiveViewerSessionDataStreamState:
    def __init__(
        self,
        ts_state: "_LttngLiveViewerSessionTracingSessionState",
        info: _LttngLiveViewerStreamInfo,
        data_stream: _LttngDataStream,
        metadata_stream_id: int,
    ):
        self._ts_state = ts_state
        self._info = info
        self._data_stream = data_stream
        self._metadata_stream_id = metadata_stream_id
        self._cur_index_entry_index = 0
        fmt = 'Built data stream state: id={}, ts-id={}, ts-name="{}", path="{}"'
        logging.info(
            fmt.format(
                info.id,
                ts_state.tracing_session_descriptor.info.tracing_session_id,
                ts_state.tracing_session_descriptor.info.name,
                data_stream.path,
            )
        )

    @property
    def tracing_session_state(self):
        return self._ts_state

    @property
    def info(self):
        return self._info

    @property
    def data_stream(self):
        return self._data_stream

    @property
    def cur_index_entry(self):
        if self._cur_index_entry_index == len(self._data_stream.index):
            return

        return self._data_stream.index[self._cur_index_entry_index]

    @property
    def metadata_stream_id(self):
        return self._metadata_stream_id

    def goto_next_index_entry(self):
        self._cur_index_entry_index += 1


# The state of a single metadata stream.
class _LttngLiveViewerSessionMetadataStreamState:
    def __init__(
        self,
        ts_state: "_LttngLiveViewerSessionTracingSessionState",
        info: _LttngLiveViewerStreamInfo,
        metadata_stream: _LttngMetadataStream,
    ):
        self._ts_state = ts_state
        self._info = info
        self._metadata_stream = metadata_stream
        self._cur_metadata_stream_section_index = 0
        if len(metadata_stream.sections) > 1:
            self._next_metadata_stream_section_timestamp = metadata_stream.sections[
                1
            ].timestamp
        else:
            self._next_metadata_stream_section_timestamp = None

        self._is_sent = False
        fmt = 'Built metadata stream state: id={}, ts-id={}, ts-name="{}", path="{}"'
        logging.info(
            fmt.format(
                info.id,
                ts_state.tracing_session_descriptor.info.tracing_session_id,
                ts_state.tracing_session_descriptor.info.name,
                metadata_stream.path,
            )
        )

    @property
    def info(self):
        return self._info

    @property
    def metadata_stream(self):
        return self._metadata_stream

    @property
    def is_sent(self):
        return self._is_sent

    @is_sent.setter
    def is_sent(self, value: bool):
        self._is_sent = value

    @property
    def cur_section(self):
        fmt = "Get current metadata section: section-idx={}"
        logging.info(fmt.format(self._cur_metadata_stream_section_index))
        if self._cur_metadata_stream_section_index == len(
            self._metadata_stream.sections
        ):
            return

        return self._metadata_stream.sections[self._cur_metadata_stream_section_index]

    def goto_next_section(self):
        self._cur_metadata_stream_section_index += 1
        if self.cur_section:
            self._next_metadata_stream_section_timestamp = self.cur_section.timestamp
        else:
            self._next_metadata_stream_section_timestamp = None

    @property
    def next_section_timestamp(self):
        return self._next_metadata_stream_section_timestamp


# A tracing session descriptor.
#
# In the constructor, `traces` is a list of LTTng traces (`LttngTrace`
# objects).
class LttngTracingSessionDescriptor:
    def __init__(
        self,
        name: str,
        tracing_session_id: int,
        hostname: str,
        live_timer_freq: int,
        client_count: int,
        traces: Iterable[LttngTrace],
    ):
        for trace in traces:
            if name not in trace.path:
                fmt = "Tracing session name must be part of every trace path (`{}` not found in `{}`)"
                raise ValueError(fmt.format(name, trace.path))

        self._traces = traces
        stream_count = sum([len(t) + 1 for t in traces])
        self._info = _LttngLiveViewerTracingSessionInfo(
            tracing_session_id,
            live_timer_freq,
            client_count,
            stream_count,
            hostname,
            name,
        )

    @property
    def traces(self):
        return self._traces

    @property
    def info(self):
        return self._info


# The state of a tracing session.
class _LttngLiveViewerSessionTracingSessionState:
    def __init__(self, tc_descr: LttngTracingSessionDescriptor, base_stream_id: int):
        self._tc_descr = tc_descr
        self._stream_infos = []  # type: list[_LttngLiveViewerStreamInfo]
        self._ds_states = {}  # type: dict[int, _LttngLiveViewerSessionDataStreamState]
        self._ms_states = (
            {}
        )  # type: dict[int, _LttngLiveViewerSessionMetadataStreamState]
        stream_id = base_stream_id

        for trace in tc_descr.traces:
            trace_id = stream_id * 1000

            # Metadata stream -> stream info and metadata stream state
            info = _LttngLiveViewerStreamInfo(
                stream_id, trace_id, True, trace.metadata_stream.path, "metadata"
            )
            self._stream_infos.append(info)
            self._ms_states[stream_id] = _LttngLiveViewerSessionMetadataStreamState(
                self, info, trace.metadata_stream
            )
            metadata_stream_id = stream_id
            stream_id += 1

            # Data streams -> stream infos and data stream states
            for data_stream in trace:
                info = _LttngLiveViewerStreamInfo(
                    stream_id,
                    trace_id,
                    False,
                    data_stream.path,
                    data_stream.channel_name,
                )
                self._stream_infos.append(info)
                self._ds_states[stream_id] = _LttngLiveViewerSessionDataStreamState(
                    self, info, data_stream, metadata_stream_id
                )
                stream_id += 1

        self._is_attached = False
        fmt = 'Built tracing session state: id={}, name="{}"'
        logging.info(fmt.format(tc_descr.info.tracing_session_id, tc_descr.info.name))

    @property
    def tracing_session_descriptor(self):
        return self._tc_descr

    @property
    def data_stream_states(self):
        return self._ds_states

    @property
    def metadata_stream_states(self):
        return self._ms_states

    @property
    def stream_infos(self):
        return self._stream_infos

    @property
    def has_new_metadata(self):
        return any([not ms.is_sent for ms in self._ms_states.values()])

    @property
    def is_attached(self):
        return self._is_attached

    @is_attached.setter
    def is_attached(self, value: bool):
        self._is_attached = value


def needs_new_metadata_section(
    metadata_stream_state: _LttngLiveViewerSessionMetadataStreamState,
    latest_timestamp: int,
):
    if metadata_stream_state.next_section_timestamp is None:
        return False

    if latest_timestamp >= metadata_stream_state.next_section_timestamp:
        return True
    else:
        return False


# An LTTng live viewer session manages a view on tracing sessions
# and replies to commands accordingly.
class _LttngLiveViewerSession:
    def __init__(
        self,
        viewer_session_id: int,
        tracing_session_descriptors: Iterable[LttngTracingSessionDescriptor],
        max_query_data_response_size: Optional[int],
    ):
        self._viewer_session_id = viewer_session_id
        self._ts_states = (
            {}
        )  # type:  dict[int, _LttngLiveViewerSessionTracingSessionState]
        self._stream_states = (
            {}
        )  # type: dict[int, _LttngLiveViewerSessionDataStreamState | _LttngLiveViewerSessionMetadataStreamState]
        self._max_query_data_response_size = max_query_data_response_size
        total_stream_infos = 0

        for ts_descr in tracing_session_descriptors:
            ts_state = _LttngLiveViewerSessionTracingSessionState(
                ts_descr, total_stream_infos
            )
            ts_id = ts_state.tracing_session_descriptor.info.tracing_session_id
            self._ts_states[ts_id] = ts_state
            total_stream_infos += len(ts_state.stream_infos)

            # Update session's stream states to have the new states
            self._stream_states.update(ts_state.data_stream_states)
            self._stream_states.update(ts_state.metadata_stream_states)

        self._command_handlers = {
            _LttngLiveViewerAttachToTracingSessionCommand: self._handle_attach_to_tracing_session_command,
            _LttngLiveViewerCreateViewerSessionCommand: self._handle_create_viewer_session_command,
            _LttngLiveViewerDetachFromTracingSessionCommand: self._handle_detach_from_tracing_session_command,
            _LttngLiveViewerGetDataStreamPacketDataCommand: self._handle_get_data_stream_packet_data_command,
            _LttngLiveViewerGetMetadataStreamDataCommand: self._handle_get_metadata_stream_data_command,
            _LttngLiveViewerGetNewStreamInfosCommand: self._handle_get_new_stream_infos_command,
            _LttngLiveViewerGetNextDataStreamIndexEntryCommand: self._handle_get_next_data_stream_index_entry_command,
            _LttngLiveViewerGetTracingSessionInfosCommand: self._handle_get_tracing_session_infos_command,
        }  # type: dict[type[_LttngLiveViewerCommand], Callable[[Any], _LttngLiveViewerReply]]

    @property
    def viewer_session_id(self):
        return self._viewer_session_id

    def _get_tracing_session_state(self, tracing_session_id: int):
        if tracing_session_id not in self._ts_states:
            raise RuntimeError(
                "Unknown tracing session ID {}".format(tracing_session_id)
            )

        return self._ts_states[tracing_session_id]

    def _get_data_stream_state(self, stream_id: int):
        if stream_id not in self._stream_states:
            RuntimeError("Unknown stream ID {}".format(stream_id))

        stream = self._stream_states[stream_id]
        if type(stream) is not _LttngLiveViewerSessionDataStreamState:
            raise RuntimeError("Stream is not a data stream")

        return stream

    def _get_metadata_stream_state(self, stream_id: int):
        if stream_id not in self._stream_states:
            RuntimeError("Unknown stream ID {}".format(stream_id))

        stream = self._stream_states[stream_id]
        if type(stream) is not _LttngLiveViewerSessionMetadataStreamState:
            raise RuntimeError("Stream is not a metadata stream")

        return stream

    def handle_command(self, cmd: _LttngLiveViewerCommand):
        logging.info(
            "Handling command in viewer session: cmd-cls-name={}".format(
                cmd.__class__.__name__
            )
        )
        cmd_type = type(cmd)

        if cmd_type not in self._command_handlers:
            raise RuntimeError(
                "Unexpected command: cmd-cls-name={}".format(cmd.__class__.__name__)
            )

        return self._command_handlers[cmd_type](cmd)

    def _handle_attach_to_tracing_session_command(
        self, cmd: _LttngLiveViewerAttachToTracingSessionCommand
    ):
        fmt = 'Handling "attach to tracing session" command: ts-id={}, offset={}, seek-type={}'
        logging.info(fmt.format(cmd.tracing_session_id, cmd.offset, cmd.seek_type))
        ts_state = self._get_tracing_session_state(cmd.tracing_session_id)
        info = ts_state.tracing_session_descriptor.info

        if ts_state.is_attached:
            raise RuntimeError(
                "Cannot attach to tracing session `{}`: viewer is already attached".format(
                    info.name
                )
            )

        ts_state.is_attached = True
        status = _LttngLiveViewerAttachToTracingSessionReply.Status.OK
        return _LttngLiveViewerAttachToTracingSessionReply(
            status, ts_state.stream_infos
        )

    def _handle_detach_from_tracing_session_command(
        self, cmd: _LttngLiveViewerDetachFromTracingSessionCommand
    ):
        fmt = 'Handling "detach from tracing session" command: ts-id={}'
        logging.info(fmt.format(cmd.tracing_session_id))
        ts_state = self._get_tracing_session_state(cmd.tracing_session_id)
        info = ts_state.tracing_session_descriptor.info

        if not ts_state.is_attached:
            raise RuntimeError(
                "Cannot detach to tracing session `{}`: viewer is not attached".format(
                    info.name
                )
            )

        ts_state.is_attached = False
        status = _LttngLiveViewerDetachFromTracingSessionReply.Status.OK
        return _LttngLiveViewerDetachFromTracingSessionReply(status)

    def _handle_get_next_data_stream_index_entry_command(
        self, cmd: _LttngLiveViewerGetNextDataStreamIndexEntryCommand
    ):
        fmt = 'Handling "get next data stream index entry" command: stream-id={}'
        logging.info(fmt.format(cmd.stream_id))
        stream_state = self._get_data_stream_state(cmd.stream_id)
        metadata_stream_state = self._get_metadata_stream_state(
            stream_state.metadata_stream_id
        )

        if stream_state.cur_index_entry is None:
            # The viewer is done reading this stream
            status = _LttngLiveViewerGetNextDataStreamIndexEntryReply.Status.HUP

            # Dummy data stream index entry to use with the `HUP` status
            # (the reply needs one, but the viewer ignores it)
            index_entry = _LttngDataStreamIndexEntry(0, 0, 0, 0, 0, 0, 0)

            return _LttngLiveViewerGetNextDataStreamIndexEntryReply(
                status, index_entry, False, False
            )

        timestamp_begin = _get_entry_timestamp_begin(stream_state.cur_index_entry)

        if needs_new_metadata_section(metadata_stream_state, timestamp_begin):
            metadata_stream_state.is_sent = False
            metadata_stream_state.goto_next_section()

        # The viewer only checks the `has_new_metadata` flag if the
        # reply's status is `OK`, so we need to provide an index here
        has_new_metadata = stream_state.tracing_session_state.has_new_metadata
        if isinstance(stream_state.cur_index_entry, _LttngDataStreamIndexEntry):
            status = _LttngLiveViewerGetNextDataStreamIndexEntryReply.Status.OK
        else:
            status = _LttngLiveViewerGetNextDataStreamIndexEntryReply.Status.INACTIVE

        reply = _LttngLiveViewerGetNextDataStreamIndexEntryReply(
            status, stream_state.cur_index_entry, has_new_metadata, False
        )
        stream_state.goto_next_index_entry()
        return reply

    def _handle_get_data_stream_packet_data_command(
        self, cmd: _LttngLiveViewerGetDataStreamPacketDataCommand
    ):
        fmt = 'Handling "get data stream packet data" command: stream-id={}, offset={}, req-length={}'
        logging.info(fmt.format(cmd.stream_id, cmd.offset, cmd.req_length))
        stream_state = self._get_data_stream_state(cmd.stream_id)
        data_response_length = cmd.req_length

        if stream_state.tracing_session_state.has_new_metadata:
            status = _LttngLiveViewerGetDataStreamPacketDataReply.Status.ERROR
            return _LttngLiveViewerGetDataStreamPacketDataReply(
                status, bytes(), True, False
            )

        if self._max_query_data_response_size:
            # Enforce a server side limit on the query requested length.
            # To ensure that the transaction terminate take the minimum of both
            # value.
            data_response_length = min(
                cmd.req_length, self._max_query_data_response_size
            )
            fmt = 'Limiting "get data stream packet data" command: req-length={} actual response size={}'
            logging.info(fmt.format(cmd.req_length, data_response_length))

        data = stream_state.data_stream.get_data(cmd.offset, data_response_length)
        status = _LttngLiveViewerGetDataStreamPacketDataReply.Status.OK
        return _LttngLiveViewerGetDataStreamPacketDataReply(status, data, False, False)

    def _handle_get_metadata_stream_data_command(
        self, cmd: _LttngLiveViewerGetMetadataStreamDataCommand
    ):
        fmt = 'Handling "get metadata stream data" command: stream-id={}'
        logging.info(fmt.format(cmd.stream_id))
        metadata_stream_state = self._get_metadata_stream_state(cmd.stream_id)

        if metadata_stream_state.is_sent:
            status = _LttngLiveViewerGetMetadataStreamDataContentReply.Status.NO_NEW
            return _LttngLiveViewerGetMetadataStreamDataContentReply(status, bytes())

        metadata_stream_state.is_sent = True
        status = _LttngLiveViewerGetMetadataStreamDataContentReply.Status.OK
        metadata_section = metadata_stream_state.cur_section
        assert metadata_section is not None

        # If we are sending an empty section, ready the next one right away.
        if len(metadata_section.data) == 0:
            metadata_stream_state.is_sent = False
            metadata_stream_state.goto_next_section()

        fmt = 'Replying to "get metadata stream data" command: metadata-size={}'
        logging.info(fmt.format(len(metadata_section.data)))
        return _LttngLiveViewerGetMetadataStreamDataContentReply(
            status, metadata_section.data
        )

    def _handle_get_new_stream_infos_command(
        self, cmd: _LttngLiveViewerGetNewStreamInfosCommand
    ):
        fmt = 'Handling "get new stream infos" command: ts-id={}'
        logging.info(fmt.format(cmd.tracing_session_id))

        # As of this version, all the tracing session's stream infos are
        # always given to the viewer when sending the "attach to tracing
        # session" reply, so there's nothing new here. Return the `HUP`
        # status as, if we're handling this command, the viewer consumed
        # all the existing data streams.
        status = _LttngLiveViewerGetNewStreamInfosReply.Status.HUP
        return _LttngLiveViewerGetNewStreamInfosReply(status, [])

    def _handle_get_tracing_session_infos_command(
        self, cmd: _LttngLiveViewerGetTracingSessionInfosCommand
    ):
        logging.info('Handling "get tracing session infos" command.')
        infos = [
            tss.tracing_session_descriptor.info for tss in self._ts_states.values()
        ]
        infos.sort(key=lambda info: info.name)
        return _LttngLiveViewerGetTracingSessionInfosReply(infos)

    def _handle_create_viewer_session_command(
        self, cmd: _LttngLiveViewerCreateViewerSessionCommand
    ):
        logging.info('Handling "create viewer session" command.')
        status = _LttngLiveViewerCreateViewerSessionReply.Status.OK

        # This does nothing here. In the LTTng relay daemon, it
        # allocates the viewer session's state.
        return _LttngLiveViewerCreateViewerSessionReply(status)


# An LTTng live TCP server.
#
# On creation, it binds to `localhost` on the TCP port `port` if not `None`, or
# on an OS-assigned TCP port otherwise. It writes the decimal TCP port number
# to a temporary port file.  It renames the temporary port file to
# `port_filename`.
#
# `tracing_session_descriptors` is a list of tracing session descriptors
# (`LttngTracingSessionDescriptor`) to serve.
#
# This server accepts a single viewer (client).
#
# When the viewer closes the connection, the server's constructor
# returns.
class LttngLiveServer:
    def __init__(
        self,
        port: Optional[int],
        port_filename: Optional[str],
        tracing_session_descriptors: Iterable[LttngTracingSessionDescriptor],
        max_query_data_response_size: Optional[int],
    ):
        logging.info("Server configuration:")

        logging.info("  Port file name: `{}`".format(port_filename))

        if max_query_data_response_size is not None:
            logging.info(
                "  Maximum response data query size: `{}`".format(
                    max_query_data_response_size
                )
            )

        for ts_descr in tracing_session_descriptors:
            info = ts_descr.info
            fmt = '  TS descriptor: name="{}", id={}, hostname="{}", live-timer-freq={}, client-count={}, stream-count={}:'
            logging.info(
                fmt.format(
                    info.name,
                    info.tracing_session_id,
                    info.hostname,
                    info.live_timer_freq,
                    info.client_count,
                    info.stream_count,
                )
            )

            for trace in ts_descr.traces:
                logging.info('    Trace: path="{}"'.format(trace.path))

        self._ts_descriptors = tracing_session_descriptors
        self._max_query_data_response_size = max_query_data_response_size
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._codec = _LttngLiveViewerProtocolCodec()

        # Port 0: OS assigns an unused port
        serv_addr = ("localhost", port if port is not None else 0)
        self._sock.bind(serv_addr)

        if port_filename is not None:
            self._write_port_to_file(port_filename)

        print("Listening on port {}".format(self._server_port))

        for ts_descr in tracing_session_descriptors:
            info = ts_descr.info
            print(
                "net://localhost:{}/host/{}/{}".format(
                    self._server_port, info.hostname, info.name
                )
            )

        try:
            self._listen()
        finally:
            self._sock.close()
            logging.info("Closed connection and socket.")

    @property
    def _server_port(self):
        return self._sock.getsockname()[1]

    def _recv_command(self):
        data = bytes()

        while True:
            logging.info("Waiting for viewer command.")
            buf = self._conn.recv(128)

            if not buf:
                logging.info("Client closed connection.")

                if data:
                    raise RuntimeError(
                        "Client closed connection after having sent {} command bytes.".format(
                            len(data)
                        )
                    )

                return

            logging.info("Received data from viewer: length={}".format(len(buf)))

            data += buf

            try:
                cmd = self._codec.decode(data)
            except struct.error as exc:
                raise RuntimeError("Malformed command: {}".format(exc)) from exc

            if cmd is not None:
                logging.info(
                    "Received command from viewer: cmd-cls-name={}".format(
                        cmd.__class__.__name__
                    )
                )
                return cmd

    def _send_reply(self, reply: _LttngLiveViewerReply):
        data = self._codec.encode(reply)
        logging.info(
            "Sending reply to viewer: reply-cls-name={}, length={}".format(
                reply.__class__.__name__, len(data)
            )
        )
        self._conn.sendall(data)

    def _handle_connection(self):
        # First command must be "connect"
        cmd = self._recv_command()

        if type(cmd) is not _LttngLiveViewerConnectCommand:
            raise RuntimeError(
                'First command is not "connect": cmd-cls-name={}'.format(
                    cmd.__class__.__name__
                )
            )

        # Create viewer session (arbitrary ID 23)
        logging.info(
            "LTTng live viewer connected: version={}.{}".format(cmd.major, cmd.minor)
        )
        viewer_session = _LttngLiveViewerSession(
            23, self._ts_descriptors, self._max_query_data_response_size
        )

        # Send "connect" reply
        self._send_reply(
            _LttngLiveViewerConnectReply(viewer_session.viewer_session_id, 2, 10)
        )

        # Make the viewer session handle the remaining commands
        while True:
            cmd = self._recv_command()

            if cmd is None:
                # Connection closed (at an expected location within the
                # conversation)
                return

            self._send_reply(viewer_session.handle_command(cmd))

    def _listen(self):
        logging.info("Listening: port={}".format(self._server_port))
        # Backlog must be present for Python version < 3.5.
        # 128 is an arbitrary number since we expect only 1 connection anyway.
        self._sock.listen(128)
        self._conn, viewer_addr = self._sock.accept()
        logging.info(
            "Accepted viewer: addr={}:{}".format(viewer_addr[0], viewer_addr[1])
        )

        try:
            self._handle_connection()
        finally:
            self._conn.close()

    def _write_port_to_file(self, port_filename: str):
        # Write the port number to a temporary file.
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as tmp_port_file:
            print(self._server_port, end="", file=tmp_port_file)

        # Rename temporary file to real file
        os.replace(tmp_port_file.name, port_filename)
        logging.info(
            'Renamed port file: src-path="{}", dst-path="{}"'.format(
                tmp_port_file.name, port_filename
            )
        )


def _session_descriptors_from_path(
    sessions_filename: str, trace_path_prefix: Optional[str]
):
    # File format is:
    #
    #     [
    #         {
    #             "name": "my-session",
    #             "id": 17,
    #             "hostname": "myhost",
    #             "live-timer-freq": 1000000,
    #             "client-count": 23,
    #             "traces": [
    #                 {
    #                     "path": "lol"
    #                 },
    #                 {
    #                     "path": "meow/mix",
    #                     "beacons": {
    #                         "my_stream": [ 5235787, 728375283 ]
    #                     },
    #                     "metadata-sections": [
    #                           {
    #                                "line": 1,
    #                                "timestamp": 0
    #                           }
    #                      ]
    #                 }
    #             ]
    #         }
    #     ]
    with open(sessions_filename, "r") as sessions_file:
        sessions_json = tjson.load(sessions_file, tjson.ArrayVal)

    sessions = []  # type: list[LttngTracingSessionDescriptor]

    for session_json in sessions_json.iter(tjson.ObjVal):
        name = session_json.at("name", tjson.StrVal).val
        tracing_session_id = session_json.at("id", tjson.IntVal).val
        hostname = session_json.at("hostname", tjson.StrVal).val
        live_timer_freq = session_json.at("live-timer-freq", tjson.IntVal).val
        client_count = session_json.at("client-count", tjson.IntVal).val
        traces_json = session_json.at("traces", tjson.ArrayVal)

        traces = []  # type: list[LttngTrace]

        for trace_json in traces_json.iter(tjson.ObjVal):
            metadata_sections = (
                trace_json.at("metadata-sections", tjson.ArrayVal)
                if "metadata-sections" in trace_json
                else None
            )
            beacons = (
                trace_json.at("beacons", tjson.ObjVal)
                if "beacons" in trace_json
                else None
            )
            path = trace_json.at("path", tjson.StrVal).val

            if not os.path.isabs(path) and trace_path_prefix:
                path = os.path.join(trace_path_prefix, path)

            traces.append(
                LttngTrace(
                    path,
                    metadata_sections,
                    beacons,
                )
            )

        sessions.append(
            LttngTracingSessionDescriptor(
                name,
                tracing_session_id,
                hostname,
                live_timer_freq,
                client_count,
                traces,
            )
        )

    return sessions


def _loglevel_parser(string: str):
    loglevels = {"info": logging.INFO, "warning": logging.WARNING}
    if string not in loglevels:
        msg = "{} is not a valid loglevel".format(string)
        raise argparse.ArgumentTypeError(msg)
    return loglevels[string]


if __name__ == "__main__":
    logging.basicConfig(format="# %(asctime)-25s%(message)s")
    parser = argparse.ArgumentParser(
        description="LTTng-live protocol mocker", add_help=False
    )
    parser.add_argument(
        "--log-level",
        default="warning",
        choices=["info", "warning"],
        help="The loglevel to be used.",
    )

    loglevel_namespace, remaining_args = parser.parse_known_args()
    logging.getLogger().setLevel(_loglevel_parser(loglevel_namespace.log_level))

    parser.add_argument(
        "--port",
        help="The port to bind to. If missing, use an OS-assigned port..",
        type=int,
    )
    parser.add_argument(
        "--port-filename",
        help="The final port file. This file is present when the server is ready to receive connection.",
    )
    parser.add_argument(
        "--max-query-data-response-size",
        type=int,
        help="The maximum size of control data response in bytes",
    )
    parser.add_argument(
        "--trace-path-prefix",
        type=str,
        help="Prefix to prepend to the trace paths of session configurations",
    )
    parser.add_argument(
        "sessions_filename",
        type=str,
        help="Path to a session configuration file",
        metavar="sessions-filename",
    )
    parser.add_argument(
        "-h",
        "--help",
        action="help",
        default=argparse.SUPPRESS,
        help="Show this help message and exit.",
    )

    args = parser.parse_args(args=remaining_args)
    sessions_filename = args.sessions_filename  # type: str
    trace_path_prefix = args.trace_path_prefix  # type: str | None
    sessions = _session_descriptors_from_path(
        sessions_filename,
        trace_path_prefix,
    )

    port = args.port  # type: int | None
    port_filename = args.port_filename  # type: str | None
    max_query_data_response_size = args.max_query_data_response_size  # type: int | None
    LttngLiveServer(port, port_filename, sessions, max_query_data_response_size)
