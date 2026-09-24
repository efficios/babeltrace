# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>


import abc
import typing


class _BaseObject:
    # Ensure that the object always has _ptr_internal set, even if it throws during
    # construction.

    def __new__(cls, *args, **kwargs):
        obj = super().__new__(cls)
        obj._ptr_internal = None
        return obj

    def __init__(self, ptr):
        self._ptr_internal = ptr

    @property
    def _ptr(self):
        return self._ptr_internal

    @property
    def addr(self) -> int:
        return int(self._ptr_internal)

    def __repr__(self) -> str:
        return f"<{self.__class__.__module__}.{self.__class__.__name__} object @ {hex(self.addr)}>"

    def __copy__(self):
        raise NotImplementedError

    def __deepcopy__(self, memo):
        raise NotImplementedError


# Type variable representing any sub-class of _UniqueObject.
_UniqueObjectT = typing.TypeVar("_UniqueObjectT", bound="_UniqueObject")

# A Python object that is itself not refcounted, but is wholly owned by an
# object that is itself refcounted (a _SharedObject).  A Babeltrace unique
# object gets destroyed once its owner gets destroyed (its refcount drops to
# 0).
#
# In the Python bindings, to avoid having to deal with issues with the lifetime
# of unique objects, we make it so acquiring a reference on a unique object
# acquires a reference on its owner.


class _UniqueObject(_BaseObject):
    # Create a _UniqueObject.
    #
    #   - ptr: `native_bt.PtrObj` object, pointer to the unique object.
    #   - owner_ptr: `native_bt.PtrObj` object, pointer to the owner of
    #     the unique object.  A new reference is acquired.
    #   - owner_get_ref: Callback to get a reference on the owner
    #   - owner_put_ref: Callback to put a reference on the owner.

    @classmethod
    def _create_from_ptr_and_get_ref(
        cls: typing.Type[_UniqueObjectT], ptr, owner_ptr, owner_get_ref, owner_put_ref
    ) -> _UniqueObjectT:
        assert ptr is not None
        assert owner_ptr is not None

        obj = cls.__new__(cls)

        obj._ptr_internal = ptr
        obj._owner_ptr = owner_ptr
        obj._owner_get_ref = owner_get_ref
        obj._owner_put_ref = owner_put_ref

        obj._owner_get_ref(obj._owner_ptr)

        return obj

    def __del__(self):
        self._owner_put_ref(self._owner_ptr)


# Type variable representing any sub-class of _SharedObject.
_SharedObjectT = typing.TypeVar("_SharedObjectT", bound="_SharedObject")


# Python object that owns a reference to a Babeltrace object.
class _SharedObject(_BaseObject, abc.ABC):
    # Get a new reference on ptr.
    #
    # This must be implemented by subclasses to work correctly with a pointer
    # of the native type they wrap.

    @staticmethod
    @abc.abstractmethod
    def _get_ref(ptr):
        raise NotImplementedError

    # Put a reference on ptr.
    #
    # This must be implemented by subclasses to work correctly with a pointer
    # of the native type they wrap.

    @staticmethod
    @abc.abstractmethod
    def _put_ref(ptr):
        raise NotImplementedError

    # Create a _SharedObject from an existing reference.
    #
    # This assumes that the caller owns a reference to the Babeltrace object
    # and transfers this ownership to the newly created Python object.

    @classmethod
    def _create_from_ptr(cls: typing.Type[_SharedObjectT], ptr_owned) -> _SharedObjectT:
        assert ptr_owned is not None

        obj = cls.__new__(cls)
        obj._ptr_internal = ptr_owned
        return obj

    # Like _create_from_ptr, but acquire a new reference rather than
    # stealing the caller's reference.

    @classmethod
    def _create_from_ptr_and_get_ref(
        cls: typing.Type[_SharedObjectT], ptr
    ) -> _SharedObjectT:
        obj = cls._create_from_ptr(ptr)
        cls._get_ref(obj._ptr_internal)
        return obj

    def __del__(self):
        self._put_ref(self._ptr_internal)
