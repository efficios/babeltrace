# SPDX-License-Identifier: MIT
#
# Copyright (C) 2023 EfficiOS, inc.
#
# pyright: strict, reportTypeCommentUsage=false


import re
import json
import typing
from typing import (
    Any,
    Dict,
    List,
    Type,
    Union,
    TextIO,
    Generic,
    Mapping,
    TypeVar,
    Optional,
    Sequence,
    overload,
)

# Internal type aliases and variables
_RawArrayT = List["_RawValT"]
_RawObjT = Dict[str, "_RawValT"]
_RawValT = Union[None, bool, int, float, str, _RawArrayT, _RawObjT]
_RawValTV = TypeVar("_RawValTV", bound="_RawValT")
_ValTV = TypeVar("_ValTV", bound="Val")


# Type of a single JSON value path element.
PathElemT = Union[str, int]


# A JSON value path.
class Path:
    def __init__(self, elems: Optional[List[PathElemT]] = None):
        if elems is None:
            elems = []

        self._elems = elems

    # Elements of this path.
    @property
    def elems(self):
        return self._elems

    # Returns a new path containing the current elements plus `elem`.
    def __truediv__(self, elem: PathElemT):
        return Path(self._elems + [elem])

    # Returns a valid jq filter.
    def __str__(self):
        s = ""

        for elem in self._elems:
            if type(elem) is str:
                if re.match(r"[a-zA-Z]\w*$", elem):
                    s += ".{}".format(elem)
                else:
                    s += '."{}"'.format(elem)
            else:
                assert type(elem) is int
                s += "[{}]".format(elem)

        if not s.startswith("."):
            s = "." + s

        return s


# Base of any JSON value.
class Val:
    _name = "a value"

    def __init__(self, path: Optional[Path] = None):
        if path is None:
            path = Path()

        self._path = path

    # Path to this JSON value.
    @property
    def path(self):
        return self._path


# JSON null value.
class NullVal(Val):
    _name = "a null"


# JSON scalar value.
class _ScalarVal(Val, Generic[_RawValTV]):
    def __init__(self, raw_val: _RawValTV, path: Optional[Path] = None):
        super().__init__(path)
        self._raw_val = raw_val

    # Raw value.
    @property
    def val(self):
        return self._raw_val


# JSON boolean value.
class BoolVal(_ScalarVal[bool]):
    _name = "a boolean"

    def __bool__(self):
        return self.val


# JSON integer value.
class IntVal(_ScalarVal[int]):
    _name = "an integer"

    def __int__(self):
        return self.val


# JSON floating point number value.
class FloatVal(_ScalarVal[float]):
    _name = "a floating point number"

    def __float__(self):
        return self.val


# JSON string value.
class StrVal(_ScalarVal[str]):
    _name = "a string"

    def __str__(self):
        return self.val


# JSON array value.
class ArrayVal(Val, Sequence[Val]):
    _name = "an array"

    def __init__(self, raw_val: _RawArrayT, path: Optional[Path] = None):
        super().__init__(path)
        self._raw_val = raw_val

    # Returns the value at index `index`.
    #
    # Raises `TypeError` if the type of the returned value isn't
    # `expected_elem_type`.
    def at(self, index: int, expected_elem_type: Type[_ValTV]):
        try:
            elem = self._raw_val[index]
        except IndexError:
            raise IndexError(
                "`{}`: array index {} out of range".format(self._path, index)
            )

        return wrap(elem, self._path / index, expected_elem_type)

    # Returns an iterator yielding the values of this array value.
    #
    # Raises `TypeError` if the type of any yielded value isn't
    # `expected_elem_type`.
    def iter(self, expected_elem_type: Type[_ValTV]):
        for i in range(len(self._raw_val)):
            yield self.at(i, expected_elem_type)

    @overload
    def __getitem__(self, index: int) -> Val:
        ...

    @overload
    def __getitem__(self, index: slice) -> Sequence[Val]:
        ...

    def __getitem__(self, index: Union[int, slice]) -> Union[Val, Sequence[Val]]:
        if type(index) is slice:
            raise NotImplementedError

        return self.at(index, Val)

    def __len__(self):
        return len(self._raw_val)


# JSON object value.
class ObjVal(Val, Mapping[str, Val]):
    _name = "an object"

    def __init__(self, raw_val: _RawObjT, path: Optional[Path] = None):
        super().__init__(path)
        self._raw_val = raw_val

    # Returns the value having the key `key`.
    #
    # Raises `TypeError` if the type of the returned value isn't
    # `expected_type`.
    def at(self, key: str, expected_type: Type[_ValTV]):
        try:
            val = self._raw_val[key]
        except KeyError:
            raise KeyError("`{}`: no value has the key `{}`".format(self._path, key))

        return wrap(val, self._path / key, expected_type)

    def __getitem__(self, key: str) -> Val:
        return self.at(key, Val)

    def __len__(self):
        return len(self._raw_val)

    def __iter__(self):
        return iter(self._raw_val)


# Raises `TypeError` if the type of `val` is not `expected_type`.
def _check_type(val: Val, expected_type: Type[Val]):
    if not isinstance(val, expected_type):
        raise TypeError(
            "`{}`: expecting {} value, got {}".format(
                val.path,
                expected_type._name,  # pyright: ignore [reportPrivateUsage]
                type(val)._name,  # pyright: ignore [reportPrivateUsage]
            )
        )


# Wraps the raw value `raw_val` into an equivalent instance of some
# `Val` subclass having the path `path` and returns it.
#
# If the resulting JSON value type isn't `expected_type`, then this
# function raises `TypeError`.
def wrap(
    raw_val: _RawValT, path: Optional[Path] = None, expected_type: Type[_ValTV] = Val
) -> _ValTV:
    val = None

    if raw_val is None:
        val = NullVal(path)
    elif isinstance(raw_val, bool):
        val = BoolVal(raw_val, path)
    elif isinstance(raw_val, int):
        val = IntVal(raw_val, path)
    elif isinstance(raw_val, float):
        val = FloatVal(raw_val, path)
    elif isinstance(raw_val, str):
        val = StrVal(raw_val, path)
    elif isinstance(raw_val, list):
        val = ArrayVal(raw_val, path)
    else:
        assert isinstance(raw_val, dict)
        val = ObjVal(raw_val, path)

    assert val is not None
    _check_type(val, expected_type)
    return typing.cast(_ValTV, val)


# Like json.loads(), but returns a `Val` instance, raising `TypeError`
# if its type isn't `expected_type`.
def loads(s: str, expected_type: Type[_ValTV] = Val, **kwargs: Any) -> _ValTV:
    return wrap(json.loads(s, **kwargs), Path(), expected_type)


# Like json.load(), but returns a `Val` instance, raising `TypeError` if
# its type isn't `expected_type`.
def load(fp: TextIO, expected_type: Type[_ValTV] = Val, **kwargs: Any) -> _ValTV:
    return wrap(json.load(fp, **kwargs), Path(), expected_type)
