# SPDX-License-Identifier: MIT
#
# Copyright (C) 2019 Philippe Proulx <pproulx@efficios.com>
#

# pyright: strict, reportMissingTypeStubs=false

import os
import re
import enum
import time
import socket
import struct
import logging
import os.path
import argparse
import tempfile
import multiprocessing
from abc import ABC, abstractmethod
from typing import (
    Dict,
    List,
    Type,
    Tuple,
    Union,
    Iterable,
    Optional,
    Sequence,
    overload,
)

import tjson

# isort: off
from typing import Any, Callable  # noqa: F401

# isort: on

_logger = logging.getLogger("LTTng live server")


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


# Trace format of a tracing session.
class _LttngLiveTraceFormat(enum.Enum):
    CTF_V1_8 = "ctf-1.8"
    CTF_V2_0 = "ctf-2.0"


class _LttngLiveViewerTracingSessionInfo:
    def __init__(
        self,
        tracing_session_id: int,
        live_timer_freq: int,
        client_count: int,
        trace_format: _LttngLiveTraceFormat,
        stream_count: int,
        hostname: str,
        name: str,
    ):
        self._tracing_session_id = tracing_session_id
        self._live_timer_freq = live_timer_freq
        self._client_count = client_count
        self._trace_format = trace_format
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
    def trace_format(self):
        return self._trace_format

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

    # When `override_hostname_len` (resp. `override_session_name_len`)
    # is set, override the configured session hostname (resp. session
    # name) on the wire (v2.15 protocol) with that many filler bytes,
    # the announced length being their count, to emulate a relay
    # sending a bogus (zero or oversized) length.
    #
    # When `override_trace_format` is set, announce that trace format
    # value on the wire (v2.15 protocol) instead of the one of the
    # session, to emulate a relay sending an unknown trace format.
    def __init__(
        self,
        override_hostname_len: Optional[int] = None,
        override_session_name_len: Optional[int] = None,
        override_trace_format: Optional[int] = None,
    ):
        self._server_minor_version: Optional[int] = None
        self._override_hostname_len = override_hostname_len
        self._override_session_name_len = override_session_name_len
        self._override_trace_format = override_trace_format

    def _unpack(self, fmt: str, data: bytes, offset: int = 0):
        fmt = f"!{fmt}"
        return struct.unpack_from(fmt, data, offset)

    def _unpack_payload(self, fmt: str, data: bytes):
        return self._unpack(
            fmt, data, _LttngLiveViewerProtocolCodec._COMMAND_HEADER_SIZE_BYTES
        )

    @property
    def server_minor_version(self):
        return self._server_minor_version

    @server_minor_version.setter
    def server_minor_version(self, server_minor_version: int):
        self._server_minor_version = server_minor_version

    def decode(self, data: bytes):
        if len(data) < self._COMMAND_HEADER_SIZE_BYTES:
            # Not enough data to read the command header
            return

        payload_size, cmd_type, version = self._unpack(
            self._COMMAND_HEADER_STRUCT_FMT, data
        )
        _logger.info(
            f"Decoded command header: payload-size={payload_size}, cmd-type={cmd_type}, version={version}"
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
            raise RuntimeError(f"Unknown command type {cmd_type}")

    def _pack(self, fmt: str, *args: Any):
        # Force network byte order
        return struct.pack(f"!{fmt}", *args)

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
                # Common part
                data += self._pack(
                    "QIII",
                    info.tracing_session_id,
                    info.live_timer_freq,
                    info.client_count,
                    info.stream_count,
                )

                # Version-specific part
                assert self._server_minor_version is not None

                if self._server_minor_version < 15:
                    # v2.4 protocol: fixed-length strings
                    data += self._encode_zero_padded_str(info.hostname, 64)
                    data += self._encode_zero_padded_str(info.name, 255)
                else:
                    # v2.15 protocol: variable-length strings + trace format
                    hostname_bytes = info.hostname.encode()
                    name_bytes = info.name.encode()

                    if self._override_hostname_len is not None:
                        hostname_bytes = b"A" * self._override_hostname_len

                    if self._override_session_name_len is not None:
                        name_bytes = b"A" * self._override_session_name_len

                    if self._override_trace_format is not None:
                        trace_format = self._override_trace_format
                    elif info.trace_format == _LttngLiveTraceFormat.CTF_V1_8:
                        trace_format = 0
                    else:
                        trace_format = 1

                    data += self._pack(
                        "III",
                        len(hostname_bytes),
                        len(name_bytes),
                        trace_format,
                    )
                    data += hostname_bytes
                    data += name_bytes
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
                f"Unknown reply object with class `{reply.__class__.__name__}`"
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

            beacons_list: List[_LttngDataStreamBeaconIndexEntry] = []
            for ts in beacons.iter(tjson.IntVal):
                beacons_list.append(
                    _LttngDataStreamBeaconIndexEntry(stream_class_id, ts.val)
                )

            self._add_beacons(beacons_list)

        _logger.info(
            f'Built data stream index entries: path="{path}", count={len(self._entries)}'
        )

    def _build(self):
        self._entries: List[_LttngIndexEntryT] = []

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
                _logger.debug(
                    f'Decoding data stream index entry: path="{self._path}", offset={f.tell()}'
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
    def __getitem__(self, index: int) -> _LttngIndexEntryT: ...

    @overload
    def __getitem__(self, index: slice) -> Sequence[_LttngIndexEntryT]: ...

    def __getitem__(self, index: Union[int, slice]) -> Union[
        _LttngIndexEntryT,
        Sequence[_LttngIndexEntryT],
    ]:
        return self._entries[index]

    def __len__(self):
        return len(self._entries)

    @property
    def path(self):
        return self._path


# Any LTTng stream (metadata or data).
class _LttngStream(ABC):
    @abstractmethod
    def __init__(self, creation_timestamp: int):
        self._creation_timestamp = creation_timestamp

    @property
    def creation_timestamp(self):
        return self._creation_timestamp


# An LTTng data stream.
class _LttngDataStream(_LttngStream):
    def __init__(
        self, path: str, beacons_json: Optional[tjson.ArrayVal], creation_timestamp: int
    ):
        super().__init__(creation_timestamp)
        self._path = path
        filename = os.path.basename(path)
        match = re.match(r"(.*)_\d+", filename)
        if not match:
            raise RuntimeError(f"Unexpected data stream file name pattern: {filename}")

        self._channel_name = match.group(1)
        trace_dir = os.path.dirname(path)
        index_path = os.path.join(trace_dir, "index", f"{filename}.idx")
        self._index = _LttngDataStreamIndex(index_path, beacons_json)
        assert os.path.isfile(path)
        self._file = open(path, "rb")
        _logger.info(
            f'Built data stream: path="{path}", channel-name="{self._channel_name}"'
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
    def __init__(self, timestamp: int, data: bytes):
        self._timestamp = timestamp
        self._data = data
        _logger.info(
            f"Built metadata stream section: ts={self._timestamp}, data-len={len(self._data)}"
        )

    @property
    def timestamp(self):
        return self._timestamp

    @property
    def data(self):
        return self._data


# An LTTng metadata stream.
class _LttngMetadataStream(_LttngStream):
    def __init__(
        self,
        metadata_file_path: str,
        config_sections: Sequence[_LttngMetadataStreamSection],
        creation_timestamp: int,
    ):
        super().__init__(creation_timestamp)
        self._path = metadata_file_path
        self._sections = config_sections
        _logger.info(
            f"Built metadata stream: path={self._path}, section-len={len(self._sections)}"
        )

    @property
    def path(self):
        return self._path

    @property
    def sections(self):
        return self._sections

    @property
    def trace_format(self):
        with open(self._path, "rb") as f:
            return (
                _LttngLiveTraceFormat.CTF_V2_0
                if f.read(1)[0] == 30
                else _LttngLiveTraceFormat.CTF_V1_8
            )


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
    metadata_sections: List[LttngMetadataConfigSection] = []
    append_empty_section = False
    last_timestamp = 0
    last_line = 0

    for section in metadata_sections_json:
        if isinstance(section, tjson.StrVal):
            if section.val == "empty":
                # Found an empty section marker. Actually append the
                # section at the timestamp of the next concrete section.
                append_empty_section = True
            else:
                raise ValueError(f"Invalid string value at {section.path}.")
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
            raise TypeError(f"`{section.path}`: expecting a string or object value")

    return metadata_sections


def _split_metadata_sections(
    metadata_file_path: str, metadata_sections_json: tjson.ArrayVal
):
    metadata_sections = _parse_metadata_sections_config(metadata_sections_json)

    sections: List[_LttngMetadataStreamSection] = []
    with open(metadata_file_path, "r", encoding="utf-8") as metadata_file:
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
                        metadata_sections[metadata_section_idx].timestamp, bytes()
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
        creation_timestamp: int,
    ):
        self._path = trace_dir
        self._creation_timestamp = creation_timestamp
        self._create_metadata_stream(trace_dir, metadata_sections_json)
        self._create_data_streams(trace_dir, beacons_json)
        _logger.info(f'Built trace: path="{trace_dir}"')

    def _create_data_streams(
        self, trace_dir: str, beacons_json: Optional[tjson.ObjVal]
    ):
        data_stream_paths: List[str] = []

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
        self._data_streams: List[_LttngDataStream] = []

        for data_stream_path in data_stream_paths:
            stream_name = os.path.basename(data_stream_path)
            this_beacons_json = None
            if beacons_json is not None and stream_name in beacons_json:
                this_beacons_json = beacons_json.at(stream_name, tjson.ArrayVal)

            self._data_streams.append(
                _LttngDataStream(
                    data_stream_path, this_beacons_json, self._creation_timestamp
                )
            )

    def _create_metadata_stream(
        self, trace_dir: str, metadata_sections_json: Optional[tjson.ArrayVal]
    ):
        metadata_path = os.path.join(trace_dir, "metadata")
        metadata_sections: List[_LttngMetadataStreamSection] = []

        if metadata_sections_json is None:
            with open(metadata_path, "rb") as metadata_file:
                metadata_sections.append(
                    _LttngMetadataStreamSection(0, metadata_file.read())
                )
        else:
            metadata_sections = _split_metadata_sections(
                metadata_path, metadata_sections_json
            )

        self._metadata_stream = _LttngMetadataStream(
            metadata_path, metadata_sections, self.creation_timestamp
        )

    @property
    def path(self):
        return self._path

    @property
    def metadata_stream(self):
        return self._metadata_stream

    @property
    def creation_timestamp(self):
        return self._creation_timestamp

    @overload
    def __getitem__(self, index: int) -> _LttngDataStream: ...

    @overload
    def __getitem__(self, index: slice) -> Sequence[_LttngDataStream]: ...

    def __getitem__(
        self, index: Union[int, slice]
    ) -> Union[_LttngDataStream, Sequence[_LttngDataStream]]:
        return self._data_streams[index]

    def __len__(self):
        return len(self._data_streams)


# Stream (metadata or data) state specific to the LTTng live protocol.
class _LttngLiveViewerSessionStreamState:
    @abstractmethod
    def __init__(self):
        # A stream is considered "announced" when it has been returned
        # to the LTTng live client in response to a "get new stream
        # infos" (`_LttngLiveViewerGetNewStreamInfosCommand`) command.
        self._announced: bool = False
        pass

    @property
    def is_announced(self):
        return self._announced

    def mark_as_announced(self):
        self._announced = True

    @property
    @abstractmethod
    def stream(self) -> _LttngStream:
        pass


# The state of a single data stream.
class _LttngLiveViewerSessionDataStreamState(_LttngLiveViewerSessionStreamState):
    def __init__(
        self,
        ts_state: "_LttngLiveViewerSessionTracingSessionState",
        info: _LttngLiveViewerStreamInfo,
        data_stream: _LttngDataStream,
        metadata_stream_id: int,
    ):
        super().__init__()
        self._ts_state = ts_state
        self._info = info
        self._data_stream = data_stream
        self._metadata_stream_id = metadata_stream_id
        self._cur_index_entry_index = 0
        fmt = 'Built data stream state: id={}, ts-id={}, ts-name="{}", path="{}"'
        _logger.info(
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
    def stream(self):
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
class _LttngLiveViewerSessionMetadataStreamState(_LttngLiveViewerSessionStreamState):
    def __init__(
        self,
        ts_state: "_LttngLiveViewerSessionTracingSessionState",
        info: _LttngLiveViewerStreamInfo,
        metadata_stream: _LttngMetadataStream,
    ):
        super().__init__()
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

        self._all_data_is_sent = False
        fmt = 'Built metadata stream state: id={}, ts-id={}, ts-name="{}", path="{}"'
        _logger.info(
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
    def stream(self):
        return self._metadata_stream

    @property
    def all_data_is_sent(self):
        return self._all_data_is_sent

    @all_data_is_sent.setter
    def all_data_is_sent(self, value: bool):
        self._all_data_is_sent = value

    @property
    def cur_section(self):
        fmt = "Get current metadata section: section-idx={}"
        _logger.info(fmt.format(self._cur_metadata_stream_section_index))
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
        trace_format: _LttngLiveTraceFormat,
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
            trace_format,
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
        self._client_visible_stream_infos: List[_LttngLiveViewerStreamInfo] = []
        self._ds_states: Dict[int, _LttngLiveViewerSessionDataStreamState] = {}
        self._ms_states: Dict[int, _LttngLiveViewerSessionMetadataStreamState] = {}
        self._last_delivered_index_timestamp = 0
        self._last_allocated_stream_id = base_stream_id

        for trace in tc_descr.traces:
            trace_stream_infos: List[_LttngLiveViewerStreamInfo] = []
            trace_id = self._last_allocated_stream_id * 1000

            # Metadata stream -> stream info and metadata stream state
            info = _LttngLiveViewerStreamInfo(
                self._last_allocated_stream_id,
                trace_id,
                True,
                trace.metadata_stream.path,
                "metadata",
            )

            trace_stream_infos.append(info)
            self._ms_states[self._last_allocated_stream_id] = (
                _LttngLiveViewerSessionMetadataStreamState(
                    self, info, trace.metadata_stream
                )
            )
            metadata_stream_id = self._last_allocated_stream_id
            self._last_allocated_stream_id += 1

            # Data streams -> stream infos and data stream states
            for data_stream in trace:
                info = _LttngLiveViewerStreamInfo(
                    self._last_allocated_stream_id,
                    trace_id,
                    False,
                    data_stream.path,
                    data_stream.channel_name,
                )
                trace_stream_infos.append(info)
                self._ds_states[self._last_allocated_stream_id] = (
                    _LttngLiveViewerSessionDataStreamState(
                        self, info, data_stream, metadata_stream_id
                    )
                )
                self._last_allocated_stream_id += 1

            if trace.creation_timestamp == 0:
                # Only announce streams for traces that are created at
                # the origin.
                #
                # The rest of the streams will be discovered by the
                # client as indexes are received with a "has new data
                # streams" flag set in the reply.
                self._client_visible_stream_infos.extend(trace_stream_infos)

        self._is_attached = False
        fmt = 'Built tracing session state: id={}, name="{}"'
        _logger.info(fmt.format(tc_descr.info.tracing_session_id, tc_descr.info.name))

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
    def client_visible_stream_infos(self):
        return self._client_visible_stream_infos

    @property
    def has_new_metadata(self):
        return any(
            [
                ms.is_announced and not ms.all_data_is_sent
                for ms in self._ms_states.values()
            ]
        )

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
        extra_data_response_len: Optional[int],
    ):
        self._viewer_session_id = viewer_session_id
        self._ts_states: Dict[int, _LttngLiveViewerSessionTracingSessionState] = {}
        self._stream_states: Dict[
            int,
            Union[
                _LttngLiveViewerSessionDataStreamState,
                _LttngLiveViewerSessionMetadataStreamState,
            ],
        ] = {}
        self._max_query_data_response_size = max_query_data_response_size
        self._extra_data_response_len = extra_data_response_len
        total_stream_infos = 0

        for ts_descr in tracing_session_descriptors:
            ts_state = _LttngLiveViewerSessionTracingSessionState(
                ts_descr, total_stream_infos
            )
            ts_id = ts_state.tracing_session_descriptor.info.tracing_session_id
            self._ts_states[ts_id] = ts_state
            total_stream_infos += len(ts_state.client_visible_stream_infos)

            # Update session's stream states to have the new states
            self._stream_states.update(ts_state.data_stream_states)
            self._stream_states.update(ts_state.metadata_stream_states)

        self._command_handlers: Dict[
            Type[_LttngLiveViewerCommand],
            Callable[[Any], _LttngLiveViewerReply],
        ] = {
            _LttngLiveViewerAttachToTracingSessionCommand: self._handle_attach_to_tracing_session_command,
            _LttngLiveViewerCreateViewerSessionCommand: self._handle_create_viewer_session_command,
            _LttngLiveViewerDetachFromTracingSessionCommand: self._handle_detach_from_tracing_session_command,
            _LttngLiveViewerGetDataStreamPacketDataCommand: self._handle_get_data_stream_packet_data_command,
            _LttngLiveViewerGetMetadataStreamDataCommand: self._handle_get_metadata_stream_data_command,
            _LttngLiveViewerGetNewStreamInfosCommand: self._handle_get_new_stream_infos_command,
            _LttngLiveViewerGetNextDataStreamIndexEntryCommand: self._handle_get_next_data_stream_index_entry_command,
            _LttngLiveViewerGetTracingSessionInfosCommand: self._handle_get_tracing_session_infos_command,
        }

    @property
    def viewer_session_id(self):
        return self._viewer_session_id

    def _get_tracing_session_state(self, tracing_session_id: int):
        if tracing_session_id not in self._ts_states:
            raise RuntimeError(f"Unknown tracing session ID {tracing_session_id}")

        return self._ts_states[tracing_session_id]

    def _get_data_stream_state(self, stream_id: int):
        if stream_id not in self._stream_states:
            raise RuntimeError(f"Unknown stream ID {stream_id}")

        stream = self._stream_states[stream_id]
        if type(stream) is not _LttngLiveViewerSessionDataStreamState:
            raise RuntimeError("Stream is not a data stream")

        return stream

    def _get_metadata_stream_state(self, stream_id: int):
        if stream_id not in self._stream_states:
            raise RuntimeError(f"Unknown stream ID {stream_id}")

        stream = self._stream_states[stream_id]
        if type(stream) is not _LttngLiveViewerSessionMetadataStreamState:
            raise RuntimeError("Stream is not a metadata stream")

        return stream

    def handle_command(self, cmd: _LttngLiveViewerCommand):
        _logger.info(
            f"Handling command in viewer session: cmd-cls-name={cmd.__class__.__name__}"
        )
        cmd_type = type(cmd)

        if cmd_type not in self._command_handlers:
            raise RuntimeError(
                f"Unexpected command: cmd-cls-name={cmd.__class__.__name__}"
            )

        return self._command_handlers[cmd_type](cmd)

    def _handle_attach_to_tracing_session_command(
        self, cmd: _LttngLiveViewerAttachToTracingSessionCommand
    ):
        fmt = 'Handling "attach to tracing session" command: ts-id={}, offset={}, seek-type={}'
        _logger.info(fmt.format(cmd.tracing_session_id, cmd.offset, cmd.seek_type))
        ts_state = self._get_tracing_session_state(cmd.tracing_session_id)
        info = ts_state.tracing_session_descriptor.info

        if ts_state.is_attached:
            raise RuntimeError(
                f"Cannot attach to tracing session `{info.name}`: viewer is already attached"
            )

        ts_state.is_attached = True
        status = _LttngLiveViewerAttachToTracingSessionReply.Status.OK
        stream_infos_to_announce = ts_state.client_visible_stream_infos

        # Mark stream infos transmitted as part of the reply as
        # announced.
        for si in stream_infos_to_announce:
            if si.is_metadata:
                self._get_metadata_stream_state(si.id).mark_as_announced()
            else:
                self._get_data_stream_state(si.id).mark_as_announced()

        return _LttngLiveViewerAttachToTracingSessionReply(
            status, stream_infos_to_announce
        )

    def _handle_detach_from_tracing_session_command(
        self, cmd: _LttngLiveViewerDetachFromTracingSessionCommand
    ):
        fmt = 'Handling "detach from tracing session" command: ts-id={}'
        _logger.info(fmt.format(cmd.tracing_session_id))
        ts_state = self._get_tracing_session_state(cmd.tracing_session_id)
        info = ts_state.tracing_session_descriptor.info

        if not ts_state.is_attached:
            raise RuntimeError(
                f"Cannot detach to tracing session `{info.name}`: viewer is not attached"
            )

        ts_state.is_attached = False
        status = _LttngLiveViewerDetachFromTracingSessionReply.Status.OK
        return _LttngLiveViewerDetachFromTracingSessionReply(status)

    @staticmethod
    def _stream_is_ready(
        stream_state: _LttngLiveViewerSessionStreamState, creation_timestamp: int
    ):
        return (
            not stream_state.is_announced
            and stream_state.stream.creation_timestamp <= creation_timestamp
        )

    def _needs_new_streams(self, current_timestamp: int):
        return any(
            self._stream_is_ready(ss, current_timestamp)
            for ss in self._stream_states.values()
        )

    def _handle_get_next_data_stream_index_entry_command(
        self, cmd: _LttngLiveViewerGetNextDataStreamIndexEntryCommand
    ):
        fmt = 'Handling "get next data stream index entry" command: stream-id={}'
        _logger.info(fmt.format(cmd.stream_id))
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
            metadata_stream_state.all_data_is_sent = False
            metadata_stream_state.goto_next_section()

        # The viewer only checks the `has_new_metadata` flag if the
        # reply's status is `OK`, so we need to provide an index here
        has_new_metadata = stream_state.tracing_session_state.has_new_metadata
        if isinstance(stream_state.cur_index_entry, _LttngDataStreamIndexEntry):
            status = _LttngLiveViewerGetNextDataStreamIndexEntryReply.Status.OK
        else:
            status = _LttngLiveViewerGetNextDataStreamIndexEntryReply.Status.INACTIVE

        reply = _LttngLiveViewerGetNextDataStreamIndexEntryReply(
            status,
            stream_state.cur_index_entry,
            has_new_metadata,
            self._needs_new_streams(timestamp_begin),
        )
        self._last_delivered_index_timestamp_begin = timestamp_begin
        stream_state.goto_next_index_entry()
        return reply

    def _handle_get_data_stream_packet_data_command(
        self, cmd: _LttngLiveViewerGetDataStreamPacketDataCommand
    ):
        fmt = 'Handling "get data stream packet data" command: stream-id={}, offset={}, req-length={}'
        _logger.info(fmt.format(cmd.stream_id, cmd.offset, cmd.req_length))
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
            _logger.info(fmt.format(cmd.req_length, data_response_length))

        data = stream_state.stream.get_data(cmd.offset, data_response_length)

        if self._extra_data_response_len:
            # Emulate a relay daemon which announces and sends more data
            # than the client requested.
            data += b"A" * self._extra_data_response_len
            fmt = 'Over-delivering "get data stream packet data" command: req-length={} actual response size={}'
            _logger.info(fmt.format(cmd.req_length, len(data)))

        status = _LttngLiveViewerGetDataStreamPacketDataReply.Status.OK
        return _LttngLiveViewerGetDataStreamPacketDataReply(status, data, False, False)

    def _handle_get_metadata_stream_data_command(
        self, cmd: _LttngLiveViewerGetMetadataStreamDataCommand
    ):
        fmt = 'Handling "get metadata stream data" command: stream-id={}'
        _logger.info(fmt.format(cmd.stream_id))
        metadata_stream_state = self._get_metadata_stream_state(cmd.stream_id)

        if metadata_stream_state.all_data_is_sent:
            status = _LttngLiveViewerGetMetadataStreamDataContentReply.Status.NO_NEW
            return _LttngLiveViewerGetMetadataStreamDataContentReply(status, bytes())

        metadata_stream_state.all_data_is_sent = True
        status = _LttngLiveViewerGetMetadataStreamDataContentReply.Status.OK
        metadata_section = metadata_stream_state.cur_section
        assert metadata_section is not None

        # If we are sending an empty section, ready the next one right away.
        if len(metadata_section.data) == 0:
            metadata_stream_state.all_data_is_sent = False
            metadata_stream_state.goto_next_section()

        fmt = 'Replying to "get metadata stream data" command: metadata-size={}'
        _logger.info(fmt.format(len(metadata_section.data)))
        return _LttngLiveViewerGetMetadataStreamDataContentReply(
            status, metadata_section.data
        )

    def _get_stream_infos_ready_for_announcement(self):
        ready_stream_infos: List[_LttngLiveViewerStreamInfo] = []

        for ss in self._stream_states.values():
            if self._stream_is_ready(ss, ss.stream.creation_timestamp):
                ready_stream_infos.append(ss.info)

        return ready_stream_infos

    # A stream is considered finished if it has been announced and all
    # of its index entries have been provided to the client.
    def _all_streams_finished(self):
        return all(
            isinstance(stream_state, _LttngLiveViewerSessionMetadataStreamState)
            or (stream_state.cur_index_entry is None and stream_state.is_announced)
            for stream_state in self._stream_states.values()
        )

    def _handle_get_new_stream_infos_command(
        self, cmd: _LttngLiveViewerGetNewStreamInfosCommand
    ):
        fmt = 'Handling "get new stream infos" command: ts-id={}'
        _logger.info(fmt.format(cmd.tracing_session_id))
        newly_announced_stream_infos = self._get_stream_infos_ready_for_announcement()

        # Mark stream infos transmitted as part of the reply as
        # announced.
        for si in newly_announced_stream_infos:
            if si.is_metadata:
                self._get_metadata_stream_state(si.id).mark_as_announced()
            else:
                self._get_data_stream_state(si.id).mark_as_announced()

        status = _LttngLiveViewerGetNewStreamInfosReply.Status.OK

        if len(newly_announced_stream_infos) == 0:
            # If all streams have been transmitted and no new traces are
            # scheduled for creation, hang up to signal that the tracing
            # session is "done".
            status = (
                _LttngLiveViewerGetNewStreamInfosReply.Status.HUP
                if self._all_streams_finished()
                else _LttngLiveViewerGetNewStreamInfosReply.Status.NO_NEW
            )

        return _LttngLiveViewerGetNewStreamInfosReply(
            status, newly_announced_stream_infos
        )

    def _handle_get_tracing_session_infos_command(
        self, cmd: _LttngLiveViewerGetTracingSessionInfosCommand
    ):
        _logger.info('Handling "get tracing session infos" command.')
        infos = [
            tss.tracing_session_descriptor.info for tss in self._ts_states.values()
        ]
        infos.sort(key=lambda info: info.name)
        return _LttngLiveViewerGetTracingSessionInfosReply(infos)

    def _handle_create_viewer_session_command(
        self, cmd: _LttngLiveViewerCreateViewerSessionCommand
    ):
        _logger.info('Handling "create viewer session" command.')
        status = _LttngLiveViewerCreateViewerSessionReply.Status.OK

        # This does nothing here. In the LTTng relay daemon, it
        # allocates the viewer session's state.
        return _LttngLiveViewerCreateViewerSessionReply(status)


# An LTTng live TCP server.
#
# On creation, it binds to `localhost` on a given TCP port, or on an
# OS-assigned TCP port, and starts listening.
#
# This server accepts a single viewer (client).
#
# Call `serve()` to accept and handle a client connection (blocking).
# The `port` property is available immediately after construction.
class LttngLiveServer:
    def __init__(
        self,
        tracing_session_descriptors: Iterable[LttngTracingSessionDescriptor],
        max_query_data_response_size: Optional[int] = None,
        extra_data_response_len: Optional[int] = None,
        override_hostname_len: Optional[int] = None,
        override_session_name_len: Optional[int] = None,
        override_trace_format: Optional[int] = None,
        max_minor_version: int = 10,
        port: Optional[int] = None,
    ):
        _logger.info("Server configuration:")
        _logger.info(f"  Maximum minor version: {max_minor_version}")

        if max_query_data_response_size is not None:
            _logger.info(
                f"  Maximum response data query size: `{max_query_data_response_size}`"
            )

        if extra_data_response_len is not None:
            _logger.info(f"  Extra response data length: `{extra_data_response_len}`")

        if override_hostname_len is not None:
            _logger.info(
                f"  Session hostname override length: `{override_hostname_len}`"
            )

        if override_session_name_len is not None:
            _logger.info(
                f"  Session name override length: `{override_session_name_len}`"
            )

        if override_trace_format is not None:
            _logger.info(f"  Session trace format override: `{override_trace_format}`")

        for ts_descr in tracing_session_descriptors:
            info = ts_descr.info
            fmt = '  TS descriptor: name="{}", id={}, hostname="{}", live-timer-freq={}, client-count={}, stream-count={}, trace-format={}:'
            _logger.info(
                fmt.format(
                    info.name,
                    info.tracing_session_id,
                    info.hostname,
                    info.live_timer_freq,
                    info.client_count,
                    info.stream_count,
                    info.trace_format.name,
                )
            )

            for trace in ts_descr.traces:
                _logger.info(f'    Trace: path="{trace.path}"')

        self._ts_descriptors = list(tracing_session_descriptors)
        self._max_query_data_response_size = max_query_data_response_size
        self._extra_data_response_len = extra_data_response_len
        self._max_minor_version = max_minor_version
        self._codec = _LttngLiveViewerProtocolCodec(
            override_hostname_len, override_session_name_len, override_trace_format
        )

        # Bind and listen now so port is immediately available and
        # connections can be accepted as soon as serve() is called.
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind(("localhost", port if port is not None else 0))

        # Backlog must be present for Python version < 3.5.
        #
        # 128 is an arbitrary number since we expect only one
        # connection anyway.
        #
        # Call listen() here in the constructor, not in serve(), to
        # avoid a race condition when used with
        # `LttngLiveServerProcess`: the child process creates this
        # server and then signals the port to the parent process before
        # calling serve(). If we don't call listen() before signaling,
        # the parent (or its client) could attempt to connect before
        # serve() calls listen(), causing the connection to be refused.
        self._sock.listen(128)
        _logger.info(f"LttngLiveServer: listening on port {self.port}")

    @property
    def port(self) -> int:
        return self._sock.getsockname()[1]

    # URL for listing sessions.
    @property
    def base_url(self) -> str:
        return f"net://localhost:{self.port}"

    # Dictionary mapping session name to full session URL.
    @property
    def session_urls(self) -> Dict[str, str]:
        urls: Dict[str, str] = {}

        for descr in self._ts_descriptors:
            info = descr.info
            urls[info.name] = (
                f"net://localhost:{self.port}/host/{info.hostname}/{info.name}"
            )

        return urls

    # Returns the URL for a specific session by name.
    def session_url(self, session_name: str) -> str:
        return self.session_urls[session_name]

    def _recv_command(self):
        data = bytes()

        while True:
            _logger.info("Waiting for viewer command.")
            buf = self._conn.recv(128)

            if not buf:
                _logger.info("Client closed connection.")

                if data:
                    raise RuntimeError(
                        f"Client closed connection after having sent {len(data)} command bytes."
                    )

                return

            _logger.info(f"Received data from viewer: length={len(buf)}")

            data += buf

            try:
                cmd = self._codec.decode(data)
            except struct.error as exc:
                raise RuntimeError(f"Malformed command: {exc}") from exc

            if cmd is not None:
                _logger.info(
                    f"Received command from viewer: cmd-cls-name={cmd.__class__.__name__}"
                )
                return cmd

    def _send_reply(self, reply: _LttngLiveViewerReply):
        data = self._codec.encode(reply)
        _logger.info(
            f"Sending reply to viewer: reply-cls-name={reply.__class__.__name__}, length={len(data)}"
        )
        self._conn.sendall(data)

    def _handle_connection(self):
        # First command must be "connect"
        cmd = self._recv_command()

        if type(cmd) is not _LttngLiveViewerConnectCommand:
            raise RuntimeError(
                f'First command is not "connect": cmd-cls-name={cmd.__class__.__name__}'
            )

        # Create viewer session (arbitrary ID 23)
        _logger.info(f"LTTng live viewer connected: version={cmd.major}.{cmd.minor}")
        viewer_session = _LttngLiveViewerSession(
            23,
            self._ts_descriptors,
            self._max_query_data_response_size,
            self._extra_data_response_len,
        )

        # Set our effective minor version
        self._minor_version = min([self._max_minor_version, cmd.minor])
        _logger.info(
            f"Effective server version is available: version={2}.{self._minor_version}"
        )
        self._codec.server_minor_version = self._minor_version

        # Send "connect" reply
        self._send_reply(
            _LttngLiveViewerConnectReply(
                viewer_session.viewer_session_id, 2, self._minor_version
            )
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
        _logger.info(f"Waiting for viewer connection: port={self.port}")
        self._conn, viewer_addr = self._sock.accept()
        _logger.info(f"Accepted viewer: addr={viewer_addr[0]}:{viewer_addr[1]}")

        try:
            self._handle_connection()
        finally:
            self._conn.close()

    # Accepts and handles a single client connection, blocking
    # until done.
    def serve(self):
        try:
            self._listen()
        finally:
            self.close()
            _logger.info("Server closed socket")

    # Closes the server socket.
    def close(self):
        _logger.info("Server closing socket")
        self._sock.close()

    # Creates a server from the JSON session configuration file
    # `sessions_filename`.
    #
    # This function prepends `trace_path_prefix` to relative trace
    # paths in the configuration.
    #
    # Other parameters are forwarded to LttngLiveServer.__init__().
    @classmethod
    def from_config_file(
        cls,
        sessions_filename: str,
        trace_path_prefix: Optional[str] = None,
        max_query_data_response_size: Optional[int] = None,
        extra_data_response_len: Optional[int] = None,
        override_hostname_len: Optional[int] = None,
        override_session_name_len: Optional[int] = None,
        override_trace_format: Optional[int] = None,
        max_minor_version: int = 10,
        port: Optional[int] = None,
    ) -> "LttngLiveServer":
        descriptors = _session_descriptors_from_path(
            sessions_filename, trace_path_prefix
        )
        return cls(
            descriptors,
            max_query_data_response_size=max_query_data_response_size,
            extra_data_response_len=extra_data_response_len,
            override_hostname_len=override_hostname_len,
            override_session_name_len=override_session_name_len,
            override_trace_format=override_trace_format,
            max_minor_version=max_minor_version,
            port=port,
        )


# Writes a port number to a file atomically.
#
# Uses a temporary file and rename for atomicity. Includes retry logic
# for Windows where the rename may fail with `PermissionError`.
def _write_port_to_file(port: int, port_filename: str):
    with tempfile.NamedTemporaryFile(
        mode="w", delete=False, dir=os.path.dirname(port_filename)
    ) as tmp_port_file:
        print(port, end="", file=tmp_port_file)

    retry_delay_s = 1

    for attempt in reversed(range(5)):
        try:
            os.replace(tmp_port_file.name, port_filename)
            _logger.info(
                f'Renamed port file: src-path="{tmp_port_file.name}", dst-path="{port_filename}"'
            )
            return
        except PermissionError:
            _logger.info(
                f'Permission error while attempting to rename port file; retrying in {retry_delay_s} s: src-path="{tmp_port_file.name}", dst-path="{port_filename}"'
            )

            if attempt == 0:
                raise

            time.sleep(retry_delay_s)


def _lttng_live_server_process_target(
    sessions_filename: str,
    trace_path_prefix: Optional[str],
    max_query_data_response_size: Optional[int],
    extra_data_response_len: Optional[int],
    override_hostname_len: Optional[int],
    override_session_name_len: Optional[int],
    override_trace_format: Optional[int],
    max_minor_version: int,
    info_queue: "multiprocessing.Queue[tuple[int, Dict[str, str]]]",
):
    # Create the server in the child process
    server = LttngLiveServer.from_config_file(
        sessions_filename,
        trace_path_prefix,
        max_query_data_response_size,
        extra_data_response_len,
        override_hostname_len,
        override_session_name_len,
        override_trace_format,
        max_minor_version,
    )

    # Send server info back to parent
    info_queue.put((server.port, server.session_urls))

    # Serve (blocks until client disconnects or socket is closed)
    server.serve()


# Background process version of `LttngLiveServer`.
#
# This class takes configuration parameters rather than wrapping an
# existing server instance because sockets cannot be "pickled" across
# process boundaries.
#
# Offers wait() to wait for the completion of the server (when the
# viewer client disconnects) and stop() to terminate the process.
#
# The `port`, `base_url`, and `session_urls` properties and the
# session_url() method are available once start() returns.
#
# Use as a context manager to automatically handle waiting and stopping.
class LttngLiveServerProcess:
    def __init__(
        self,
        sessions_filename: str,
        trace_path_prefix: Optional[str] = None,
        max_query_data_response_size: Optional[int] = None,
        extra_data_response_len: Optional[int] = None,
        override_hostname_len: Optional[int] = None,
        override_session_name_len: Optional[int] = None,
        override_trace_format: Optional[int] = None,
        max_minor_version: int = 10,
    ):
        self._sessions_filename = sessions_filename
        self._trace_path_prefix = trace_path_prefix
        self._max_query_data_response_size = max_query_data_response_size
        self._extra_data_response_len = extra_data_response_len
        self._override_hostname_len = override_hostname_len
        self._override_session_name_len = override_session_name_len
        self._override_trace_format = override_trace_format
        self._max_minor_version = max_minor_version
        self._process: Optional[multiprocessing.Process] = None
        self._server_port: Optional[int] = None
        self._session_urls: Optional[Dict[str, str]] = None

    @property
    def port(self) -> int:
        if self._server_port is None:
            raise RuntimeError("Server not started")

        return self._server_port

    @property
    def base_url(self) -> str:
        return f"net://localhost:{self.port}"

    @property
    def session_urls(self) -> Dict[str, str]:
        if self._session_urls is None:
            raise RuntimeError("Background server process not started")

        return self._session_urls

    def session_url(self, session_name: str) -> str:
        return self.session_urls[session_name]

    @property
    def is_alive(self) -> bool:
        return self._process is not None and self._process.is_alive()

    # Starts the server process.
    #
    # Blocks until the server is ready to accept connections (port is
    # available).
    def start(self):
        info_queue: "multiprocessing.Queue[Tuple[int, Dict[str, str]]]" = (
            multiprocessing.Queue()
        )

        self._process = multiprocessing.Process(
            target=_lttng_live_server_process_target,
            args=(
                self._sessions_filename,
                self._trace_path_prefix,
                self._max_query_data_response_size,
                self._extra_data_response_len,
                self._override_hostname_len,
                self._override_session_name_len,
                self._override_trace_format,
                self._max_minor_version,
                info_queue,
            ),
            daemon=True,
        )

        _logger.info("Starting background server process")
        self._process.start()

        # Wait for the server to send its info
        self._server_port, self._session_urls = info_queue.get()
        _logger.info(f"Background server ready on port {self._server_port}")

    # Waits for the server process to finish.
    def wait(self, timeout: Optional[float] = None):
        assert self._process is not None
        self._process.join(timeout=timeout)

        if timeout is not None and self._process.is_alive():
            _logger.warning(
                f"Background server process still running after {timeout} s timeout"
            )

    # Terminates the server process and waits for it to finish.
    def close(self):
        if self._process is not None and self._process.is_alive():
            self._process.terminate()

        self.wait()

    # Creates an `LttngLiveServerProcess` instance from a
    # configuration file.
    #
    # This is provided for API symmetry with `LttngLiveServer`.
    @classmethod
    def from_config_file(
        cls,
        sessions_filename: str,
        trace_path_prefix: Optional[str] = None,
        max_query_data_response_size: Optional[int] = None,
        extra_data_response_len: Optional[int] = None,
        override_hostname_len: Optional[int] = None,
        override_session_name_len: Optional[int] = None,
        override_trace_format: Optional[int] = None,
        max_minor_version: int = 10,
    ) -> "LttngLiveServerProcess":
        return cls(
            sessions_filename,
            trace_path_prefix,
            max_query_data_response_size,
            extra_data_response_len,
            override_hostname_len,
            override_session_name_len,
            override_trace_format,
            max_minor_version,
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
    #                     "path": "lol",
    #                     creation-timestamp: 12948
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
    with open(sessions_filename, "r", encoding="utf-8") as sessions_file:
        sessions_json = tjson.load(sessions_file, tjson.ArrayVal)

    sessions: List[LttngTracingSessionDescriptor] = []

    for session_json in sessions_json.iter(tjson.ObjVal):
        name = session_json.at("name", tjson.StrVal).val
        tracing_session_id = session_json.at("id", tjson.IntVal).val
        hostname = session_json.at("hostname", tjson.StrVal).val
        live_timer_freq = session_json.at("live-timer-freq", tjson.IntVal).val
        client_count = session_json.at("client-count", tjson.IntVal).val
        traces_json = session_json.at("traces", tjson.ArrayVal)

        traces: List[LttngTrace] = []

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
            creation_timestamp: int = (
                trace_json.at("creation-timestamp", tjson.IntVal).val
                if "creation-timestamp" in trace_json
                else 0
            )

            if not os.path.isabs(path) and trace_path_prefix:
                path = os.path.join(trace_path_prefix, path)

            traces.append(
                LttngTrace(path, metadata_sections, beacons, creation_timestamp)
            )

        assert len(traces) > 0

        sessions.append(
            LttngTracingSessionDescriptor(
                name,
                tracing_session_id,
                hostname,
                live_timer_freq,
                client_count,
                traces[0].metadata_stream.trace_format,
                traces,
            )
        )

    return sessions


def _loglevel_parser(string: str):
    loglevels = {"info": logging.INFO, "warning": logging.WARNING}
    if string not in loglevels:
        msg = f"{string} is not a valid loglevel"
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
        help="The port to bind to. If missing, use an OS-assigned port.",
        type=int,
    )
    parser.add_argument(
        "--port-filename",
        help="The final port file. This file is present when the server is ready to receive connection.",
    )
    parser.add_argument(
        "--max-query-data-response-size",
        type=int,
        help="The maximum size of control data response in bytes.",
    )
    parser.add_argument(
        "--extra-data-response-len",
        type=int,
        help="Number of extra filler bytes to append to each data packet response, "
        "to emulate a relay daemon sending more data than requested.",
    )
    parser.add_argument(
        "--override-hostname-len",
        type=int,
        help="Override the configured session hostname on the wire (2.15 protocol) "
        "with this many filler bytes, to emulate a relay daemon announcing a bogus "
        "length.",
    )
    parser.add_argument(
        "--override-session-name-len",
        type=int,
        help="Like `--override-hostname-len`, but for the session name.",
    )
    parser.add_argument(
        "--override-trace-format",
        type=int,
        help="Announce this trace format on the wire (2.15 protocol) instead of the "
        "one of the session, to emulate a relay daemon announcing an unknown trace "
        "format.",
    )
    parser.add_argument(
        "--trace-path-prefix",
        type=str,
        help="Prefix to prepend to the trace paths of session configurations.",
    )
    parser.add_argument(
        "--server-max-minor-version",
        type=int,
        default=10,
        help="Maximum minor version of the server instead of 10.",
    )
    parser.add_argument(
        "sessions_filename",
        type=str,
        help="Path to a session configuration file.",
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

    server = LttngLiveServer.from_config_file(
        args.sessions_filename,
        args.trace_path_prefix,
        args.max_query_data_response_size,
        args.extra_data_response_len,
        args.override_hostname_len,
        args.override_session_name_len,
        args.override_trace_format,
        args.server_max_minor_version,
        args.port,
    )

    if args.port_filename is not None:
        _write_port_to_file(server.port, args.port_filename)

    print(f"Listening on port: {server.port}")
    print(f"Listing URL: `{server.base_url}`")
    print("Available session URLs:")

    for url in server.session_urls.values():
        print(f"  - `{url}`")

    server.serve()
