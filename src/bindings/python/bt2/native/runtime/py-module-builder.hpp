/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#ifndef BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_PY_MODULE_BUILDER_HPP
#define BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_PY_MODULE_BUILDER_HPP

#include <Python.h>

#include <cctype>
#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

/*
 * Include before "common/func-status.h" because
 * `<babeltrace2/babeltrace.h>` undefines the `__BT_FUNC_STATUS_*`
 * definitions.
 */
#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "common/func-status.h"
#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/vendor/wise-enum/wise_enum.h"

#include "exc.hpp"
#include "ptr-obj.hpp"
#include "py-conv.hpp"
#include "utils.hpp"

namespace bt2py {
namespace internal {

/*
 * Signature of the function pointer type `FuncPtrT`.
 *
 * Offers:
 *
 * `Ret`:
 *     Return type.
 *
 * `Args`:
 *     `std::tuple` of the parameter types.
 */
template <typename FuncPtrT>
struct FuncSig;

template <typename RetT, typename... ArgTs>
struct FuncSig<RetT (*)(ArgTs...)>
{
    using Ret = RetT;
    using Args = std::tuple<ArgTs...>;
};

template <typename RetT, typename... ArgTs>
struct FuncSig<RetT (*)(ArgTs...) noexcept> : FuncSig<RetT (*)(ArgTs...)>
{
};

/*
 * Converter for the parameter or return type `T`.
 *
 * Decays `T` so that, for example, a `const bt2c::CStringView&`
 * parameter or a `const bt2::ValueType` return type uses the converter
 * of the bare type.
 */
template <typename T>
using PyConvFor = PyConv<std::decay_t<T>>;

template <auto FuncV, typename RetT, typename ArgsT>
class PyFuncAdapter;

/*
 * Python C function adapting the C++ function `FuncV`.
 *
 * Only PyModuleBuilder::_pyMethodDef() uses this.
 *
 * Picks the cheapest calling convention for the arity of `FuncV`:
 *
 * No parameters:
 *     `METH_NOARGS`
 *
 * One parameter:
 *     `METH_O` (no argument tuple unpacking; most adapted functions)
 *
 * Two parameters or more:
 *     `METH_VARARGS`
 *
 * All three share the `PyCFunction` signature, so the method table
 * needs no function pointer cast.
 */
template <auto FuncV, typename RetT, typename... ArgTs>
class PyFuncAdapter<FuncV, RetT, std::tuple<ArgTs...>> final
{
private:
    static constexpr std::size_t _argCount = sizeof...(ArgTs);

public:
    static constexpr int flags = _argCount == 0 ? METH_NOARGS :
                                 _argCount == 1 ? METH_O :
                                                  METH_VARARGS;

    static PyObject *pyFunc(PyObject *, PyObject * const pyArgs) noexcept
    {
        try {
            if constexpr (_argCount == 0) {
                return &_call();
            } else if constexpr (_argCount == 1) {
                /* `METH_O`: `pyArgs` is the single argument */
                return &_call(PyConvFor<ArgTs>::fromPy(*pyArgs)...);
            } else {
                return &_callWithTuple(*pyArgs, std::index_sequence_for<ArgTs...> {});
            }
        } catch (...) {
            setPyErrFromCurCppExc();
            return nullptr;
        }
    }

private:
    template <std::size_t... IndexVs>
    static PyObject& _callWithTuple(PyObject& pyArgs, std::index_sequence<IndexVs...>)
    {
        BT_ASSERT_DBG(PyTuple_GET_SIZE(&pyArgs) == static_cast<Py_ssize_t>(_argCount));

        /* Braced initialization: converts from left to right */
        std::tuple<std::decay_t<ArgTs>...> args {
            PyConvFor<ArgTs>::fromPy(*PyTuple_GET_ITEM(&pyArgs, IndexVs))...};

        return _call(std::get<IndexVs>(std::move(args))...);
    }

    template <typename... CallArgTs>
    static PyObject& _call(CallArgTs&&...args)
    {
        if constexpr (std::is_void_v<RetT>) {
            FuncV(std::forward<CallArgTs>(args)...);
            return newPyNoneRef();
        } else {
            return PyConvFor<RetT>::toPy(FuncV(std::forward<CallArgTs>(args)...));
        }
    }
};

/*
 * Returns the length of the C identifier (`[A-Za-z0-9_]` characters)
 * at the beginning of `str`.
 */
constexpr std::size_t identLen(const std::string_view str) noexcept
{
    std::size_t len = 0;

    for (const auto ch : str) {
        if ((ch < 'a' || ch > 'z') && (ch < 'A' || ch > 'Z') && (ch < '0' || ch > '9') &&
            ch != '_') {
            break;
        }

        ++len;
    }

    return len;
}

/*
 * Returns the name of the libbabeltrace2 function `FuncV` without its
 * `bt_` prefix, for example `clock_class_create`
 * for bt_clock_class_create().
 *
 * Extracts the name from `__PRETTY_FUNCTION__`, which contains the name
 * of `FuncV`, at build time. For example, with bt_clock_class_create():
 *
 * GCC:
 *     `[...] [with auto FuncV = bt_clock_class_create; [...]]`
 *
 * Clang:
 *     `[...] [FuncV = &bt_clock_class_create]`
 */
template <auto FuncV>
std::string libFuncPyName()
{
    using namespace std::string_view_literals;

    /* Everything after `FuncV = ` */
    constexpr std::string_view prettyFunc {__PRETTY_FUNCTION__};
    constexpr auto funcPrefix = "FuncV = "sv;
    constexpr auto funcPrefixPos = prettyFunc.find(funcPrefix);

    static_assert(funcPrefixPos != std::string_view::npos,
                  "`__PRETTY_FUNCTION__` contains `FuncV = `.");

    constexpr auto afterFuncPrefix = prettyFunc.substr(funcPrefixPos + funcPrefix.size());

    /* Without the `&` prefix (Clang) */
    constexpr auto funcNameAndRest = afterFuncPrefix.substr(afterFuncPrefix.front() == '&' ? 1 : 0);

    constexpr auto libPrefix = "bt_"sv;

    static_assert(funcNameAndRest.substr(0, libPrefix.size()) == libPrefix,
                  "`FuncV` is a libbabeltrace2 function (with the `bt_` prefix).");

    /* Function name (C identifier) */
    constexpr auto funcName = funcNameAndRest.substr(0, identLen(funcNameAndRest));

    return std::string {funcName.substr(libPrefix.size())};
}

template <auto FuncV, typename ArgsT>
struct BoolRetFunc;

/*
 * Function calling `FuncV`, which returns a `bt_bool` value, and
 * returning the result as a `bool` value.
 *
 * Only PyModuleBuilder::addLibBoolRetFunc() uses this.
 */
template <auto FuncV, typename... ArgTs>
struct BoolRetFunc<FuncV, std::tuple<ArgTs...>> final
{
    static_assert(std::is_same_v<typename FuncSig<decltype(FuncV)>::Ret, bt_bool>,
                  "`FuncV` returns `bt_bool`.");

    static bool call(const ArgTs... args) noexcept
    {
        return static_cast<bool>(FuncV(args...));
    }
};

/*
 * Calls the Python callable `pyCallable` with a `PtrObj` object holding
 * `addr` of which the type is `tag`, and then releases `pyCallable`.
 *
 * Logs and clears any exception which the Python callable raises: a
 * destruction listener can't report an error.
 */
void callPyDestructionListener(PyObjUP pyCallable, void *addr, const LibObjTypeTag& tag) noexcept;

template <auto FuncV, typename ArgsT>
struct AddDestructionListenerFunc;

/*
 * Function adding a Python callable as a destruction listener of a
 * `LibObjT` object with `FuncV` (for example,
 * bt_trace_add_destruction_listener()).
 *
 * Only PyModuleBuilder::addLibAddDestructionListenerFunc() uses this.
 */
template <auto FuncV, typename LibObjT, typename ListenerFuncT>
struct AddDestructionListenerFunc<
    FuncV, std::tuple<const LibObjT *, ListenerFuncT, void *, bt_listener_id *>> final
{
private:
    using _Status = typename FuncSig<decltype(FuncV)>::Ret;

public:
    /*
     * Adds the Python callable `pyCallable` as a destruction listener
     * of `libObjPtr`, returning the status and the listener ID.
     *
     * On success, the listener owns a reference to `pyCallable` until
     * libbabeltrace2 calls it or the Python layer removes it.
     */
    static std::tuple<_Status, bt_listener_id> call(const LibObjT * const libObjPtr,
                                                    PyObject * const pyCallable) noexcept
    {
        BT_ASSERT_DBG(libObjPtr);
        BT_ASSERT_DBG(PyCallable_Check(pyCallable));

        bt_listener_id id = 0;
        const auto status = FuncV(libObjPtr, _listener, pyCallable, &id);

        if (static_cast<int>(status) == BT_FUNC_STATUS_OK) {
            Py_INCREF(pyCallable);
        }

        return std::make_tuple(status, id);
    }

private:
    /*
     * Calls the Python callable `data` with a `PtrObj` object holding
     * `libObjPtr`, and then releases the reference to `data`.
     */
    static void _listener(const LibObjT * const libObjPtr, void * const data) noexcept
    {
        /* libbabeltrace2 calls a destruction listener once */
        callPyDestructionListener(PyObjUP {static_cast<PyObject *>(data)},
                                  const_cast<LibObjT *>(libObjPtr), libObjTypeTagOf<LibObjT>());
    }
};

template <auto FuncV, typename ArgsT>
struct RemoveDestructionListenerFunc;

/*
 * Function removing a Python destruction listener from a `LibObjT`
 * object with `FuncV` (for example,
 * bt_trace_remove_destruction_listener()).
 *
 * Only PyModuleBuilder::addLibRemoveDestructionListenerFunc()
 * uses this.
 */
template <auto FuncV, typename LibObjT>
struct RemoveDestructionListenerFunc<FuncV, std::tuple<const LibObjT *, bt_listener_id>> final
{
private:
    using _Status = typename FuncSig<decltype(FuncV)>::Ret;

public:
    /*
     * Removes the destruction listener `id`, of which the Python
     * callable is `pyCallable`, from `libObjPtr`.
     *
     * On success, releases the reference to `pyCallable` which the
     * listener owns.
     */
    static _Status call(const LibObjT * const libObjPtr, const bt_listener_id id,
                        PyObject * const pyCallable) noexcept
    {
        const auto status = FuncV(libObjPtr, id);

        if (static_cast<int>(status) == BT_FUNC_STATUS_OK) {
            Py_DECREF(pyCallable);
        }

        return status;
    }
};

} /* namespace internal */

/*
 * Collects the functions, constants, and objects of the native module,
 * and then builds it.
 *
 * Each libbabeltrace2 API area unit in the `api` directory adds its
 * own entries.
 *
 * An instance must outlive the module it builds: the module refers to
 * its method table.
 */
class PyModuleBuilder final
{
public:
    /*
     * Adds the function `name` which adapts the C++ function `FuncV`.
     */
    template <auto FuncV>
    void addFunc(const char * const name)
    {
        /* Keep the sentinel last */
        _mMethodDefs.insert(std::prev(_mMethodDefs.end()), _pyMethodDef<FuncV>(name));
    }

    /*
     * Adds the function which adapts the libbabeltrace2 function
     * `FuncV`, named after `FuncV` without its `bt_` prefix.
     *
     * For example, `addLibFunc<bt_clock_class_create>()` adds the
     * function `clock_class_create`.
     */
    template <auto FuncV>
    void addLibFunc()
    {
        this->addFunc<FuncV>(this->_libFuncPyName<FuncV>());
    }

    /*
     * Like addLibFunc(), but for the libbabeltrace2 function `FuncV`
     * which returns a `bt_bool` value, so that the Python function
     * returns a Python `bool`.
     *
     * `bt_bool` is an alias of `int`: addLibFunc() can't distinguish
     * a `bt_bool` return value from an integer one and would return
     * a Python `int`.
     */
    template <auto FuncV>
    void addLibBoolRetFunc()
    {
        using Sig = internal::FuncSig<decltype(FuncV)>;

        this->addFunc<internal::BoolRetFunc<FuncV, typename Sig::Args>::call>(
            this->_libFuncPyName<FuncV>());
    }

    /*
     * Adds the function, named like addLibFunc() does, which adds a
     * Python callable as a destruction listener of a libbabeltrace2
     * object with the libbabeltrace2 function `FuncV` (for
     * example, bt_trace_add_destruction_listener()).
     *
     * The Python function accepts the libbabeltrace2 object and the
     * Python callable, and returns the status and the listener ID.
     *
     * On success, the native listener owns a reference to the Python
     * callable until libbabeltrace2 calls it or the function which
     * addLibRemoveDestructionListenerFunc() adds removes it.
     */
    template <auto FuncV>
    void addLibAddDestructionListenerFunc()
    {
        using Sig = internal::FuncSig<decltype(FuncV)>;

        this->addFunc<internal::AddDestructionListenerFunc<FuncV, typename Sig::Args>::call>(
            this->_libFuncPyName<FuncV>());
    }

    /*
     * Adds the function, named like addLibFunc() does, which removes a
     * Python destruction listener from a libbabeltrace2 object with the
     * libbabeltrace2 function `FuncV` (for
     * example, bt_trace_remove_destruction_listener()).
     *
     * The Python function accepts the libbabeltrace2 object, the
     * listener ID, and the Python callable which the Python layer
     * passed when adding the listener, and returns the status.
     *
     * On success, releases the reference to the Python callable which
     * the native listener owns.
     */
    template <auto FuncV>
    void addLibRemoveDestructionListenerFunc()
    {
        using Sig = internal::FuncSig<decltype(FuncV)>;

        this->addFunc<internal::RemoveDestructionListenerFunc<FuncV, typename Sig::Args>::call>(
            this->_libFuncPyName<FuncV>());
    }

    /*
     * Adds the integer constant `name` having the value `val`.
     */
    void addConstant(std::string name, const long long val)
    {
        _mConsts.emplace_back(std::move(name), val);
    }

    /*
     * Adds one integer constant per enumerator of the wise enum
     * `EnumT`, named `prefix` followed with the enumerator name
     * converted to upper snake case.
     *
     * For example, with the prefix `VALUE_TYPE_`,
     * `bt2::ValueType::UnsignedInteger` becomes
     * `VALUE_TYPE_UNSIGNED_INTEGER`.
     */
    template <typename EnumT>
    void addWiseEnumConstants(const char * const prefix)
    {
        for (const auto& enumerator : wise_enum::range<EnumT>) {
            this->addConstant(prefix + _upperSnakeCase(enumerator.name),
                              static_cast<long long>(enumerator.value));
        }
    }

    /*
     * Adds the object `name`, created by `createFunc` once the
     * module exists.
     *
     * `createFunc` returns a new reference, throwing `PyErrSet`
     * on error.
     */
    void addObject(std::string name, std::function<PyObject&()> createFunc)
    {
        _mObjs.emplace_back(std::move(name), std::move(createFunc));
    }

    /*
     * Builds and returns the module named `name` (new reference).
     *
     * The module refers to `name` for its whole lifetime.
     *
     * Throws `PyErrSet` on error.
     */
    PyObject& build(bt2c::CStringView name) const;

private:
    /*
     * Returns whether or not the names of all the functions,
     * constants, and objects are unique.
     */
    bool _namesAreUnique() const;

    /*
     * Returns the Python name of the libbabeltrace2 function `FuncV`
     * (see internal::libFuncPyName()).
     *
     * This builder owns the returned string.
     */
    template <auto FuncV>
    const char *_libFuncPyName()
    {
        return _mLibFuncPyNames.emplace_back(internal::libFuncPyName<FuncV>()).c_str();
    }

    /*
     * Returns a Python method definition named `name` which adapts the
     * C++ function `FuncV`.
     *
     * `bt2py::PyConv` must be specialized for each parameter type and
     * for the return type (unless `void`) of `FuncV`.
     */
    template <auto FuncV>
    static PyMethodDef _pyMethodDef(const char * const name) noexcept
    {
        using Sig = internal::FuncSig<decltype(FuncV)>;
        using Adapter = internal::PyFuncAdapter<FuncV, typename Sig::Ret, typename Sig::Args>;

        return PyMethodDef {name, Adapter::pyFunc, Adapter::flags, nullptr};
    }

    static std::string _upperSnakeCase(const std::string_view camelCaseName)
    {
        std::string res;

        /* Worst case: one `_` before each character but the first */
        res.reserve(camelCaseName.size() * 2);

        for (const auto ch : camelCaseName) {
            if (std::isupper(static_cast<unsigned char>(ch)) && !res.empty()) {
                res += '_';
            }

            res += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }

        return res;
    }

    /* Always ends with the sentinel which Python expects */
    std::vector<PyMethodDef> _mMethodDefs {{nullptr, nullptr, 0, nullptr}};

    /*
     * Names of the functions which the addLib*() methods add.
     *
     * A list because the method table refers to those strings: adding
     * an element doesn't move the existing ones.
     */
    std::list<std::string> _mLibFuncPyNames;

    std::vector<std::pair<std::string, long long>> _mConsts;
    std::vector<std::pair<std::string, std::function<PyObject&()>>> _mObjs;

    /*
     * Mutable: build() fills it, and then the interpreter owns it in
     * practice (it writes to its `m_base` member).
     */
    mutable PyModuleDef _mModDef {};
};

} /* namespace bt2py */

#endif /* BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_PY_MODULE_BUILDER_HPP */
