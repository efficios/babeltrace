/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include "py-common/py-common.h"

#include <cstdint>
#include <unordered_map>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/component-class-dev.hpp"
#include "cpp-common/bt2/component-class.hpp"
#include "cpp-common/bt2/exc.hpp"
#include "cpp-common/bt2/integer-range-set.hpp"
#include "cpp-common/bt2/message.hpp"
#include "cpp-common/bt2/private-query-executor.hpp"
#include "cpp-common/bt2/self-component-class.hpp"
#include "cpp-common/bt2/self-component-port.hpp"
#include "cpp-common/bt2/self-message-iterator-configuration.hpp"
#include "cpp-common/bt2/self-message-iterator.hpp"
#include "cpp-common/bt2/value.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/bt2c/exc.hpp"
#include "cpp-common/bt2c/glib-up.hpp"
#include "cpp-common/bt2c/logging.hpp"

#include "../api.hpp"
#include "../runtime/exc.hpp"
#include "../runtime/ptr-obj.hpp"
#include "../runtime/py-conv.hpp"
#include "../runtime/py-module-builder.hpp"
#include "../runtime/utils.hpp"

/*
 * A Python component class (a Python class which inherits
 * `bt2._UserSourceComponent`, `bt2._UserFilterComponent`, or
 * `bt2._UserSinkComponent`) is a libbabeltrace2 component class which
 * bt2::createComponentClass() creates from the C++ component class
 * `SrcComp`, `FltComp`, or `SinkComp` below.
 *
 * All the libbabeltrace2 component classes which a given C++ component
 * class creates share the same methods: each method finds the Python
 * class/object to call from the component class, the component, or the
 * message iterator, and then calls a `_bt_*_from_native()`
 * Python method.
 *
 * On error, a method translates the Python exception to a C++ exception
 * with throwCppExcFromCurPyExc() which the C++ component class bridge
 * translates to a method status.
 */

namespace bt2py {
namespace {

/*
 * Returns the map of each libbabeltrace2 component class which
 * createPyCompCls() creates to its Python class (borrowed reference).
 *
 * The Python class, through its metaclass, removes its entry when
 * Python destroys it (see unregisterPyCompCls()).
 *
 * Never destroyed: Python may destroy a Python component class during
 * its finalization, which can happen after the destruction of the
 * static objects of this module (for example, when a library
 * destructor of libbabeltrace2 finalizes Python).
 */
std::unordered_map<const bt_component_class *, PyObject *>& pyCompClsMap() noexcept
{
    static auto& map = *new std::unordered_map<const bt_component_class *, PyObject *>;

    return map;
}

/*
 * Returns the Python class of the component class `compCls`
 * (borrowed reference).
 *
 * Appends a cause to the error of the current thread with `logger` and
 * throws `bt2::Error` if there's none (Python already destroyed it).
 */
PyObject& pyCompCls(const bt2::ConstComponentClass compCls, const bt2c::Logger& logger)
{
    const auto it = pyCompClsMap().find(compCls.libObjPtr());

    if (it == pyCompClsMap().end()) {
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(
            logger, bt2::Error,
            "Cannot find the Python class of a native component class: comp-cls-addr={}, "
            "comp-cls-name={}",
            fmt::ptr(compCls.libObjPtr()), compCls.name());
    }

    return *it->second;
}

/*
 * Calls the method `name` of `pyObj` with the arguments `args`, each
 * one converted with toPyObjUp(), returning the result.
 *
 * A `PyObject *` argument (typically initialization or query method
 * data) may be `nullptr`, in which case the method receives `None`.
 *
 * Throws `PyErrSet` on error.
 */
template <typename... ArgTs>
PyObjUP callPyMethod(PyObject& pyObj, const char * const name, const ArgTs&...args)
{
    return ownPyObjOrThrow(
        PyObject_CallMethodObjArgs(&pyObj, ownPyObjOrThrow(PyUnicode_FromString(name)).get(),
                                   toPyObjUp(args).get()..., nullptr));
}

/*
 * Calls `func` and returns what it returns, translating a `PyErrSet`
 * exception to the C++ exception which the C++ component class bridge
 * expects (see throwCppExcFromCurPyExc()) with `logger`.
 */
template <typename FuncT>
auto callTranslatingPyExc(const bt2c::Logger& logger, FuncT&& func) -> decltype(func())
{
    try {
        return func();
    } catch (const PyErrSet&) {
        throwCppExcFromCurPyExc(logger);
    }
}

/*
 * Calls the _user_finalize() method of `pyObj`.
 *
 * Logs a warning with `logger` and clears the Python error indicator if
 * it raises: a finalization method can't report an error.
 */
void callPyUserFinalize(PyObject& pyObj, const bt2c::Logger& logger) noexcept
{
    const PyObjUP pyRes {PyObject_CallMethod(&pyObj, "_user_finalize", nullptr)};

    if (pyRes) {
        BT_ASSERT(pyRes.get() == Py_None);
        return;
    }

    {
        /* Doesn't modify the Python error indicator */
        const bt2c::GStringUP str {
            bt_py_common_format_current_exception(static_cast<int>(logger.level()))};

        /* On error, bt_py_common_format_current_exception() logs itself */
        if (str) {
            BT_CPPLOGW_SPEC(logger, "User's _user_finalize() method raised an exception: {}",
                            str->str);
        }
    }

    PyErr_Clear();
}

/*
 * Releases the Python object which is the user data of each port
 * of `ports`.
 *
 * The Python layer always sets a Python object (possibly `None`) as
 * the user data of a port which it adds, getting a reference (see
 * `api/self-component.cpp`).
 */
template <typename PortsT>
void releasePortData(const PortsT ports) noexcept
{
    for (const auto port : ports) {
        Py_DECREF(&port.template data<PyObject>());
    }
}

void releaseCompPortData(const bt2::SelfSourceComponent selfComp) noexcept
{
    releasePortData(selfComp.outputPorts());
}

void releaseCompPortData(const bt2::SelfFilterComponent selfComp) noexcept
{
    releasePortData(selfComp.inputPorts());
    releasePortData(selfComp.outputPorts());
}

void releaseCompPortData(const bt2::SelfSinkComponent selfComp) noexcept
{
    releasePortData(selfComp.inputPorts());
}

/*
 * Python component object of a Python component of which the self
 * component wrapper type is `SelfCompT`.
 *
 * Creates the Python component object on construction, and finalizes
 * and releases it, as well as the user data of the ports of the
 * component, on destruction.
 */
template <typename SelfCompT>
class CompPyObj final
{
public:
    /*
     * Creates the Python component object of `selfComp` from its Python
     * class, with the initialization parameters `params` and
     * initialization method data `initData`.
     *
     * Keeps a reference to `logger`, which must outlive this.
     */
    explicit CompPyObj(const SelfCompT selfComp, const bt2::ConstMapValue params,
                       PyObject * const initData, const bt2c::Logger& logger)
        : _mSelfComp {selfComp},
          _mLogger {logger}
    {
        try {
            _mPyObj = callTranslatingPyExc(logger, [&] {
                /*
                 * _UserComponentType._bt_init_from_native() creates the
                 * Python component object and calls its __init__()
                 * method.
                 *
                 * Doesn't get a reference to `initData`: the user's
                 * __init__() method will get one if needed.
                 */
                return callPyMethod(
                    pyCompCls(bt2::SelfComponent {selfComp}.asConstComponent().cls(), logger),
                    "_bt_init_from_native", selfComp, params, initData);
            });
        } catch (...) {
            /*
             * libbabeltrace2 doesn't finalize a component of which the
             * initialization method fails: release the data of the
             * ports which __init__() added now.
             */
            releaseCompPortData(selfComp);
            throw;
        }
    }

    CompPyObj(const CompPyObj&) = delete;
    CompPyObj& operator=(const CompPyObj&) = delete;

    ~CompPyObj()
    {
        callPyUserFinalize(*_mPyObj, _mLogger);
        _mPyObj.reset();
        releaseCompPortData(_mSelfComp);
    }

    /*
     * Python component object (borrowed reference).
     */
    PyObject& pyObj() const noexcept
    {
        return *_mPyObj;
    }

    /*
     * Calls the method `name` of the Python component object with the
     * arguments `args` (see callPyMethod()), returning the result.
     *
     * Translates any Python exception (see callTranslatingPyExc()).
     */
    template <typename... ArgTs>
    PyObjUP call(const char * const name, const ArgTs&...args) const
    {
        return callTranslatingPyExc(_mLogger, [&] {
            return callPyMethod(*_mPyObj, name, args...);
        });
    }

    /*
     * Calls the method _bt_port_connected_from_native() of the Python
     * component object.
     */
    template <typename SelfPortT, typename OtherPortT>
    void portConnected(const SelfPortT selfPort, const bt_port_type selfPortType,
                       const OtherPortT otherPort) const
    {
        const auto pyRes =
            this->call("_bt_port_connected_from_native", selfPort, selfPortType, otherPort);

        BT_ASSERT(pyRes.get() == Py_None);
    }

private:
    SelfCompT _mSelfComp;
    const bt2c::Logger& _mLogger;
    PyObjUP _mPyObj;
};

/*
 * Calls the method _bt_get_supported_mip_versions_from_native() of the
 * Python class of `selfCompCls`, adding the returned ranges
 * to `ranges`.
 */
void callPyGetSupportedMipVersions(const bt2::SelfComponentClass selfCompCls,
                                   const bt2::ConstMapValue params, PyObject * const initData,
                                   const bt2::LoggingLevel logLevel,
                                   const bt2::UnsignedIntegerRangeSet ranges)
{
    const bt2c::Logger logger {selfCompCls, logLevel, "BT2-NATIVE/PY-COMP-CLS"};
    const auto pyRangeSetPtr = callTranslatingPyExc(logger, [&] {
        /*
         * Doesn't get a reference to `initData`: the user's
         * _user_get_supported_mip_versions() method will get one
         * if needed.
         */
        return callPyMethod(pyCompCls(selfCompCls.asConstComponentClass(), logger),
                            "_bt_get_supported_mip_versions_from_native", params, initData,
                            logLevel);
    });

    /* The Python method returns a new reference */
    const auto rangeSet = bt2::ConstUnsignedIntegerRangeSet::Shared::createWithoutRef(
        libObjPtrFromPtrObj<const bt_integer_range_set_unsigned>(*pyRangeSetPtr));

    for (const auto range : *rangeSet) {
        ranges.addRange(range.lower(), range.upper());
    }
}

/*
 * Calls the method `_bt_query_from_native()` of the Python class of
 * `selfCompCls`, returning the query result.
 */
bt2::Value::Shared callPyQuery(const bt2::SelfComponentClass selfCompCls,
                               const bt2::PrivateQueryExecutor privQueryExec,
                               const bt2c::CStringView obj, const bt2::ConstValue params,
                               PyObject * const data)
{
    const bt2c::Logger logger {selfCompCls, privQueryExec, "BT2-NATIVE/PY-COMP-CLS"};
    const auto pyResPtr = callTranslatingPyExc(logger, [&] {
        /*
         * Doesn't get a reference to `data`: the user's _user_query()
         * method will get one if needed.
         */
        return callPyMethod(pyCompCls(selfCompCls.asConstComponentClass(), logger),
                            "_bt_query_from_native", privQueryExec, obj, params, data);
    });

    /* The Python method returns a new reference */
    return bt2::Value::Shared::createWithoutRef(libObjPtrFromPtrObj<bt_value>(*pyResPtr));
}

/*
 * Message iterator of a Python source or filter component of which the
 * C++ component class is `CompT`.
 *
 * The Python message iterator object is an instance of the `_iter_cls`
 * attribute of the Python component class.
 */
template <typename CompT>
class MsgIter final : public bt2::UserMessageIterator<MsgIter<CompT>, CompT>
{
    friend bt2::UserMessageIterator<MsgIter<CompT>, CompT>;

public:
    explicit MsgIter(const bt2::SelfMessageIterator selfMsgIter,
                     const bt2::SelfMessageIteratorConfiguration config,
                     const bt2::SelfComponentOutputPort port)
        : bt2::UserMessageIterator<MsgIter<CompT>, CompT> {selfMsgIter, "PY-MSG-ITER"}
    {
        _mPyObj = callTranslatingPyExc(this->_mLogger, [&] {
            auto& pyComp = this->_component().pyObj();
            const auto pyIterCls = ownPyObjOrThrow(PyObject_GetAttrString(
                reinterpret_cast<PyObject *>(Py_TYPE(&pyComp)), "_iter_cls"));

            /* Not initialized yet: _bt_init_from_native() calls __init__() */
            auto pyIter = callPyMethod(*pyIterCls, "__new__", pyIterCls.get());

            callPyMethod(*pyIter, "_bt_init_from_native", selfMsgIter, config, port);
            return pyIter;
        });
    }

    MsgIter(const MsgIter&) = delete;
    MsgIter& operator=(const MsgIter&) = delete;

    ~MsgIter()
    {
        callPyUserFinalize(*_mPyObj, this->_mLogger);
    }

private:
    void _next(bt2::ConstMessageArray& msgs)
    {
        try {
            const auto pyMsgPtr = this->_call("_bt_next_from_native");

            /* The Python method returns a new reference */
            msgs.append(bt2::ConstMessage::Shared::createWithoutRef(
                libObjPtrFromPtrObj<const bt_message>(*pyMsgPtr)));
        } catch (const bt2c::End&) {
            /* Leave `msgs` empty: the C++ bridge returns an end status */
        }
    }

    bool _canSeekBeginning()
    {
        return PyConv<bool>::fromPy(*this->_call("_bt_can_seek_beginning_from_native"));
    }

    void _seekBeginning()
    {
        const auto pyRes = this->_call("_bt_seek_beginning_from_native");

        BT_ASSERT(pyRes.get() == Py_None);
    }

    bool _canSeekNsFromOrigin(const std::int64_t nsFromOrigin)
    {
        return PyConv<bool>::fromPy(
            *this->_call("_bt_can_seek_ns_from_origin_from_native", nsFromOrigin));
    }

    void _seekNsFromOrigin(const std::int64_t nsFromOrigin)
    {
        const auto pyRes = this->_call("_bt_seek_ns_from_origin_from_native", nsFromOrigin);

        BT_ASSERT(pyRes.get() == Py_None);
    }

    /*
     * Calls the method `name` of the Python message iterator object
     * with the arguments `args` (see callPyMethod()), returning
     * the result.
     *
     * Translates any Python exception (see callTranslatingPyExc()).
     */
    template <typename... ArgTs>
    PyObjUP _call(const char * const name, const ArgTs&...args)
    {
        return callTranslatingPyExc(this->_mLogger, [&] {
            return callPyMethod(*_mPyObj, name, args...);
        });
    }

    PyObjUP _mPyObj;
};

class SrcComp;

using SrcCompBase = bt2::UserSourceComponent<SrcComp, MsgIter<SrcComp>, PyObject, PyObject>;

/*
 * Python source component.
 */
class SrcComp final : public SrcCompBase
{
    friend SrcCompBase;

public:
    explicit SrcComp(const bt2::SelfSourceComponent selfComp, const bt2::ConstMapValue params,
                     PyObject * const initData)
        : SrcCompBase {selfComp, "BT2-NATIVE/SRC"},
          _mCompPyObj {selfComp, params, initData, this->_mLogger}
    {
    }

    PyObject& pyObj() const noexcept
    {
        return _mCompPyObj.pyObj();
    }

private:
    static bt2::Value::Shared _query(const bt2::SelfComponentClass selfCompCls,
                                     const bt2::PrivateQueryExecutor privQueryExec,
                                     const bt2c::CStringView obj, const bt2::ConstValue params,
                                     PyObject * const data)
    {
        return callPyQuery(selfCompCls, privQueryExec, obj, params, data);
    }

    static void _getSupportedMipVersions(const bt2::SelfComponentClass selfCompCls,
                                         const bt2::ConstMapValue params, PyObject * const initData,
                                         const bt2::LoggingLevel logLevel,
                                         const bt2::UnsignedIntegerRangeSet ranges)
    {
        callPyGetSupportedMipVersions(selfCompCls, params, initData, logLevel, ranges);
    }

    void _outputPortConnected(const bt2::SelfComponentOutputPort outputPort,
                              const bt2::ConstInputPort inputPort)
    {
        _mCompPyObj.portConnected(outputPort, BT_PORT_TYPE_OUTPUT, inputPort);
    }

    CompPyObj<bt2::SelfSourceComponent> _mCompPyObj;
};

class FltComp;

using FltCompBase = bt2::UserFilterComponent<FltComp, MsgIter<FltComp>, PyObject, PyObject>;

/*
 * Python filter component.
 */
class FltComp final : public FltCompBase
{
    friend FltCompBase;

public:
    explicit FltComp(const bt2::SelfFilterComponent selfComp, const bt2::ConstMapValue params,
                     PyObject * const initData)
        : FltCompBase {selfComp, "BT2-NATIVE/FLT"},
          _mCompPyObj {selfComp, params, initData, this->_mLogger}
    {
    }

    PyObject& pyObj() const noexcept
    {
        return _mCompPyObj.pyObj();
    }

private:
    static bt2::Value::Shared _query(const bt2::SelfComponentClass selfCompCls,
                                     const bt2::PrivateQueryExecutor privQueryExec,
                                     const bt2c::CStringView obj, const bt2::ConstValue params,
                                     PyObject * const data)
    {
        return callPyQuery(selfCompCls, privQueryExec, obj, params, data);
    }

    static void _getSupportedMipVersions(const bt2::SelfComponentClass selfCompCls,
                                         const bt2::ConstMapValue params, PyObject * const initData,
                                         const bt2::LoggingLevel logLevel,
                                         const bt2::UnsignedIntegerRangeSet ranges)
    {
        callPyGetSupportedMipVersions(selfCompCls, params, initData, logLevel, ranges);
    }

    void _inputPortConnected(const bt2::SelfComponentInputPort inputPort,
                             const bt2::ConstOutputPort outputPort)
    {
        _mCompPyObj.portConnected(inputPort, BT_PORT_TYPE_INPUT, outputPort);
    }

    void _outputPortConnected(const bt2::SelfComponentOutputPort outputPort,
                              const bt2::ConstInputPort inputPort)
    {
        _mCompPyObj.portConnected(outputPort, BT_PORT_TYPE_OUTPUT, inputPort);
    }

    CompPyObj<bt2::SelfFilterComponent> _mCompPyObj;
};

class SinkComp;

using SinkCompBase = bt2::UserSinkComponent<SinkComp, PyObject, PyObject>;

/*
 * Python sink component.
 */
class SinkComp final : public SinkCompBase
{
    friend SinkCompBase;

public:
    explicit SinkComp(const bt2::SelfSinkComponent selfComp, const bt2::ConstMapValue params,
                      PyObject * const initData)
        : SinkCompBase {selfComp, "BT2-NATIVE/SINK"},
          _mCompPyObj {selfComp, params, initData, this->_mLogger}
    {
    }

private:
    static bt2::Value::Shared _query(const bt2::SelfComponentClass selfCompCls,
                                     const bt2::PrivateQueryExecutor privQueryExec,
                                     const bt2c::CStringView obj, const bt2::ConstValue params,
                                     PyObject * const data)
    {
        return callPyQuery(selfCompCls, privQueryExec, obj, params, data);
    }

    static void _getSupportedMipVersions(const bt2::SelfComponentClass selfCompCls,
                                         const bt2::ConstMapValue params, PyObject * const initData,
                                         const bt2::LoggingLevel logLevel,
                                         const bt2::UnsignedIntegerRangeSet ranges)
    {
        callPyGetSupportedMipVersions(selfCompCls, params, initData, logLevel, ranges);
    }

    void _inputPortConnected(const bt2::SelfComponentInputPort inputPort,
                             const bt2::ConstOutputPort outputPort)
    {
        _mCompPyObj.portConnected(inputPort, BT_PORT_TYPE_INPUT, outputPort);
    }

    void _graphIsConfigured()
    {
        const auto pyRes = _mCompPyObj.call("_bt_graph_is_configured_from_native");

        BT_ASSERT(pyRes.get() == Py_None);
    }

    bool _consume()
    {
        try {
            _mCompPyObj.call("_user_consume");
            return true;
        } catch (const bt2c::End&) {
            return false;
        }
    }

    CompPyObj<bt2::SelfSinkComponent> _mCompPyObj;
};

/*
 * Creates a component class named `name`, with the description `descr`
 * and the help text `help`, from the C++ component class `CompT`,
 * associating it to the Python class `pyCls`.
 *
 * Doesn't get a reference to `pyCls`: the Python class removes the
 * association when Python destroys it (see unregisterPyCompCls()).
 */
template <typename CompT>
auto createPyCompCls(PyObject * const pyCls, const bt2c::CStringView name,
                     const bt2c::CStringView descr, const bt2c::CStringView help)
{
    auto compCls = bt2::createComponentClass<CompT>(name, descr, help);
    const auto inserted =
        pyCompClsMap().emplace(bt2::ConstComponentClass {*compCls}.libObjPtr(), pyCls).second;

    BT_ASSERT(inserted);
    return compCls;
}

/*
 * Removes the association of the component class `compCls` to its
 * Python class.
 */
void unregisterPyCompCls(const bt_component_class * const compCls) noexcept
{
    const auto count = pyCompClsMap().erase(compCls);

    BT_ASSERT(count == 1);
}

/*
 * Returns whether or not `compCls` is a Python component class.
 */
bool compClsIsPy(const bt_component_class * const compCls) noexcept
{
    return pyCompClsMap().count(compCls) != 0;
}

/*
 * Returns the Python component object of the component of
 * `selfMsgIter` (new reference).
 *
 * The component of a Python message iterator is always a Python source
 * or filter component.
 */
PyObjUP getSelfMsgIterCompData(const bt2::SelfMessageIterator selfMsgIter) noexcept
{
    const auto selfComp = selfMsgIter.component();

    if (selfComp.isSource()) {
        return newPyObjUp(selfComp.data<SrcComp>().pyObj());
    }

    BT_ASSERT_DBG(selfComp.isFilter());
    return newPyObjUp(selfComp.data<FltComp>().pyObj());
}

} /* namespace */

void registerComponentClassDevApi(PyModuleBuilder& mb)
{
    mb.addFunc<createPyCompCls<SrcComp>>("component_class_source_create");
    mb.addFunc<createPyCompCls<FltComp>>("component_class_filter_create");
    mb.addFunc<createPyCompCls<SinkComp>>("component_class_sink_create");
    mb.addFunc<unregisterPyCompCls>("component_class_unregister_py_cls");
    mb.addFunc<compClsIsPy>("component_class_is_python");

    mb.addFunc<getSelfMsgIterCompData>("self_message_iterator_get_component_data");
}

} /* namespace bt2py */
