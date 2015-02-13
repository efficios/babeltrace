class CTFStringEncoding:
    """
    CTF string encodings.
    """

    #: None
    NONE = 0

    #: UTF-8
    UTF8 = 1

    #: ASCII
    ASCII = 2

    #: Unknown
    UNKNOWN = 3


# Based on the enum in ctf-writer/writer.h
class ByteOrder:
    """
    Byte orders.
    """

    #: Native byte order
    BYTE_ORDER_NATIVE = 0

    #: Little-endian
    BYTE_ORDER_LITTLE_ENDIAN = 1

    #: Big-endian
    BYTE_ORDER_BIG_ENDIAN = 2

    #: Network byte order (big-endian)
    BYTE_ORDER_NETWORK = 3

    #: Unknown byte order
    BYTE_ORDER_UNKNOWN = 4  # Python-specific entry


# enum equivalent, accessible constants
# These are taken directly from ctf/events.h
# All changes to enums must also be made here
class CTFTypeId:
    """
    CTF numeric type identifiers.
    """

    #: Unknown type
    UNKNOWN = 0

    #: Integer
    INTEGER = 1

    #: Floating point number
    FLOAT = 2

    #: Enumeration
    ENUM = 3

    #: String
    STRING = 4

    #: Structure
    STRUCT = 5

    #: Untagged variant
    UNTAGGED_VARIANT = 6

    #: Variant
    VARIANT = 7

    #: Array
    ARRAY = 8

    #: Sequence
    SEQUENCE = 9

    NR_CTF_TYPES = 10

    def type_name(id):
        """
        Returns the name of the CTF numeric type identifier *id*.
        """

        name = "UNKNOWN_TYPE"
        constants = [
            attr for attr in dir(CTFTypeId) if not callable(
                getattr(
                    CTFTypeId,
                    attr)) and not attr.startswith("__")]

        for attr in constants:
            if getattr(CTFTypeId, attr) == id:
                name = attr
                break

        return name


class CTFScope:
    """
    CTF scopes.
    """

    #: Packet header
    TRACE_PACKET_HEADER = 0

    #: Packet context
    STREAM_PACKET_CONTEXT = 1

    #: Event header
    STREAM_EVENT_HEADER = 2

    #: Stream event context
    STREAM_EVENT_CONTEXT = 3

    #: Event context
    EVENT_CONTEXT = 4

    #: Event fields
    EVENT_FIELDS = 5

    def scope_name(scope):
        """
        Returns the name of the CTF scope *scope*.
        """

        name = "UNKNOWN_SCOPE"
        constants = [
            attr for attr in dir(CTFScope) if not callable(
                getattr(
                    CTFScope,
                    attr)) and not attr.startswith("__")]

        for attr in constants:
            if getattr(CTFScope, attr) == scope:
                name = attr
                break

        return name
