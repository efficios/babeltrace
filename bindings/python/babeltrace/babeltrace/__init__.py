# backward compatibility with old `babeltrace` module: import common members
from .common import \
    CTFStringEncoding, \
    ByteOrder, \
    CTFTypeId, \
    CTFScope


# backward compatibility with old `babeltrace` module: import reader API members
from .reader import \
    TraceCollection, \
    TraceHandle, \
    Event, \
    FieldError, \
    EventDeclaration, \
    FieldDeclaration, \
    IntegerFieldDeclaration, \
    EnumerationFieldDeclaration, \
    ArrayFieldDeclaration, \
    SequenceFieldDeclaration, \
    FloatFieldDeclaration, \
    StructureFieldDeclaration, \
    StringFieldDeclaration, \
    VariantFieldDeclaration


# backward compatibility with old `babeltrace` module: import CTF writer API
# module as `CTFWriter`, since `CTFWriter` used to be a class in the
# `babeltrace` module
import babeltrace.writer as CTFWriter


__version__ = '2.0.0-pre1'
