/** compute_kernel_host.h                                                -*- C++ -*-
    Jeremy Barnes, 27 March 2016
    This file is part of MLDB. Copyright 2016 mldb.ai inc. All rights reserved.

    Compute kernel runtime for CPU devices.
*/

#pragma once

#include "compute_kernel.h"

namespace MLDB {

// HostComputeEvent
// We do everything synchronously (for now), so nothing much really going on here

struct HostComputeEvent: public ComputeEvent {
    virtual ~HostComputeEvent() = default;

    virtual std::shared_ptr<ComputeProfilingInfo> getProfilingInfo() const override
    {
        return std::make_shared<ComputeProfilingInfo>();
    }

    virtual void await() const override
    {
    }

    virtual std::shared_ptr<ComputeEvent> thenImpl(std::function<void ()> fn) override
    {
        fn();
        return std::make_shared<HostComputeEvent>();
    }
};

namespace details {

using Pin = std::shared_ptr<const void>;

template<typename T>
std::tuple<ComputeKernelType, std::function<Pin(const std::string & opName, MemoryArrayHandleT<T> & out, ComputeKernelArgument & in, ComputeContext & context)>>
marshalParameterForCpuKernelCall(MemoryArrayHandleT<T> *)
{
    ComputeKernelType result(getDefaultDescriptionSharedT<T>(), "rw");

    auto convertParam = [] (const std::string & opName, MemoryArrayHandleT<T> & out, ComputeKernelArgument & in, ComputeContext & context) -> Pin
    {
        if (in.handler->canGetHandle()) {
            auto handle = in.handler->getHandle(opName + " marshal", context);
            out = {std::move(handle)};
            return nullptr;
        }
        throw MLDB::Exception("attempt to pass non-handle memory region to arg that needs a handle (not implemented)");
    };

    return { result, convertParam };
}

template<typename T>
std::tuple<ComputeKernelType, std::function<Pin(const std::string & opName, MemoryArrayHandleT<const T> & out, ComputeKernelArgument & in, ComputeContext & context)>>
marshalParameterForCpuKernelCall(MemoryArrayHandleT<const T> *)
{
    ComputeKernelType result(getDefaultDescriptionSharedT<T>(), "r");

    auto convertParam = [] (const std::string & opName, MemoryArrayHandleT<const T> & out, ComputeKernelArgument & in, ComputeContext & context) -> Pin
    {
        if (in.handler->canGetHandle()) {
            auto handle = in.handler->getHandle(opName + " marshal", context);
            out = MemoryArrayHandleT<const T>(std::move(handle.handle));
            return nullptr;
        }
        throw MLDB::Exception("attempt to pass non-handle memory region to arg that needs a handle (not implemented)");
    };

    return { result, convertParam };
}

template<typename T>
std::tuple<ComputeKernelType, std::function<Pin(const std::string & opName, MutableMemoryRegionT<T> & out, ComputeKernelArgument & in, ComputeContext & context)>>
marshalParameterForCpuKernelCall(MutableMemoryRegionT<T> *)
{
    ComputeKernelType result(getDefaultDescriptionSharedT<T>(), "rw");

    auto convertParam = [] (const std::string & opName, MutableMemoryRegionT<T> & out, ComputeKernelArgument & in, ComputeContext & context) -> Pin
    {
        if (in.handler->canGetRange()) {
            auto [data, length, handle] = in.handler->getRange(opName + " marshal", context);
            MutableMemoryRegion raw{ handle, (char *)data, length };
            out = std::move(raw);
            return nullptr;
        }
        throw MLDB::Exception("attempt to pass non-mutable range memory region to arg that needs a mutable range");
    };

    return { result, convertParam };
}

template<typename T>
std::tuple<ComputeKernelType, std::function<Pin(const std::string & opName, FrozenMemoryRegionT<T> & out, ComputeKernelArgument & in, ComputeContext & context)>>
marshalParameterForCpuKernelCall(FrozenMemoryRegionT<T> *)
{
    ComputeKernelType result(getDefaultDescriptionSharedT<T>(), "r");

    auto convertParam = [] (const std::string & opName, FrozenMemoryRegionT<T> & out, ComputeKernelArgument & in, ComputeContext & context) -> Pin
    {
        if (in.handler->canGetConstRange()) {
            auto [data, length, handle] = in.handler->getConstRange(opName + " marshal", context);
            FrozenMemoryRegion raw{ handle, (const char *)data, length };
            out = std::move(raw);
            return nullptr;
        }
        throw MLDB::Exception("attempt to pass non-handle memory region to arg that needs a handle (not implemented)");
    };

    return { result, convertParam };
}

template<typename T>
std::tuple<ComputeKernelType, std::function<Pin (const std::string & opName, T * & out, ComputeKernelArgument & in, ComputeContext & context)>>
marshalParameterForCpuKernelCall(T **);

template<typename T>
std::tuple<ComputeKernelType, std::function<Pin (const std::string & opName, const T * & out, ComputeKernelArgument & in, ComputeContext & context)>>
marshalParameterForCpuKernelCall(const T **);

template<typename T>
std::tuple<ComputeKernelType, std::function<Pin (const std::string & opName, std::span<T> & out, ComputeKernelArgument & in, ComputeContext & context)>>
marshalParameterForCpuKernelCall(std::span<T> *)
{
    ComputeKernelType result(getDefaultDescriptionSharedT<T>(), "rw");
 
    auto convertParam = [] (const std::string & opName, std::span<T> & out, ComputeKernelArgument & in, ComputeContext & context) -> Pin
    {
        if (in.handler->canGetRange()) {
            auto [ptr, size, pin] = in.handler->getRange(opName + " marshal", context);
            out = { reinterpret_cast<T *>(ptr), size / sizeof(T) };
            return std::move(pin);
        }
        throw MLDB::Exception("attempt to pass non-range memory region to arg that needs a span (not implemented)");
    };

    return { result, convertParam };   
}

template<typename T>
std::tuple<ComputeKernelType, std::function<Pin (const std::string & opName, std::span<const T> & out, ComputeKernelArgument & in, ComputeContext & context)>>
marshalParameterForCpuKernelCall(std::span<const T> *)
{
    ComputeKernelType result(getDefaultDescriptionSharedT<T>(), "r");

    auto convertParam = [] (const std::string & opName, std::span<const T> & out, ComputeKernelArgument & in, ComputeContext & context) -> Pin
    {
        if (in.handler->canGetConstRange()) {
            auto [ptr, size, pin] = in.handler->getConstRange(opName, context);
            out = { reinterpret_cast<const T *>(ptr), size / sizeof(T) };
            return std::move(pin);
        }
        throw MLDB::Exception("attempt to pass non-range memory region to arg that needs a span (not implemented)");
    };

    return { result, convertParam };
}

// Implemented in .cc file to avoid including value_description.h
// Copies from to to via the value description
void copyUsingValueDescription(const ValueDescription * desc,
                               std::span<const std::byte> from, void * to,
                               const std::type_info & toType);

const std::type_info & getTypeFromValueDescription(const ValueDescription * desc);

template<typename T>
std::tuple<ComputeKernelType, std::function<Pin(const std::string & opName, T & out, ComputeKernelArgument & in, ComputeContext & context)>>
marshalParameterForCpuKernelCall(T *)
{
    ComputeKernelType result(getDefaultDescriptionSharedT<std::remove_const_t<T>>(),
                             std::is_const_v<T> ? "r" : "rw");

    auto convertParam = [] (const std::string & opName, T & out, ComputeKernelArgument & in, ComputeContext & context) -> Pin
    {
        ExcAssert(in.handler->canGetPrimitive());
        std::span<const std::byte> mem = in.handler->getPrimitive(opName, context);
        copyUsingValueDescription(in.handler->type.baseType.get(), mem, &out, typeid(T));
        return nullptr;
    };

    return { result, convertParam };    
}

template<typename T>
std::tuple<ComputeKernelType, std::function<Pin (const std::string & opName, T & out, ComputeKernelArgument & in, ComputeContext & context)>>
marshalParameterForCpuKernelCall()
{
    return marshalParameterForCpuKernelCall((T *)nullptr);
}

} // namespace details

struct HostComputeKernel: public ComputeKernel {
    // This is called for each passed parameter, with T representing the type of the parameter
    // which was passed and arg its value.  The formal specification of the parameter is in
    // params[n].
    template<typename T>
    void extractParam(T & arg, ComputeKernelArgument param, size_t n, ComputeContext & context,
                      std::vector<details::Pin> & pins) const
    {
        //const ComputeKernel::ParameterInfo & formalArgument = params[n];
        //const ComputeKernelType & formalType = formalArgument.type;
        //const ComputeKernelType & inputType = param.abstractType;

        auto [outputType, marshal] = details::marshalParameterForCpuKernelCall<T>();

#if 0
        // Verify dimensionality
        int formalDims = formalType.dims();
        int inputDims = inputType.dims();
        int outputDims = outputType.dims();

        if (formalDims != inputDims || formalDims != outputDims) {
            throw AnnotatedException("attempt to pass parameter with incompatible number of dimensions");
        }

        bool isConst = params[n].isConst;
        bool argIsConst

#endif
        const std::type_info & requiredType
            = details::getTypeFromValueDescription(param.handler->type.baseType.get());

        try {
            //using namespace std;
            //cerr << "setting parameter " << n << " named " << this->params[n].name << " from " << param.handler->info() << endl;
            //using namespace std;
            //cerr << "converting parameter " << n << " with formal type " << params[n].type.print()
            //     << " from type " << param.handler->type.print() << " to type " << type_name<T>(arg)
            //     << endl;
            auto pin = marshal("kernel " + this->kernelName + " bind param " + std::to_string(n) + " " + this->params[n].name,
                               arg, param, context);
            if (pin) {
                pins.emplace_back(std::move(pin));
            }
        } MLDB_CATCH_ALL {
            rethrowException(500, "Attempting to convert parameter from passed type " + type_name<T>()
                             + " to required type " + demangle(requiredType.name()) + " passing parameter "
                             + std::to_string(n) + " ('" + params[n].name + "')  of kernel " + kernelName
                             + " with abstract type " + params[n].type.print(),
                             "abstractType", params[n].type);
        }
    }

    template<size_t N, typename... Args>
    void extractParams(std::tuple<Args...> & args, std::vector<ComputeKernelArgument> & params,
                       ComputeContext & context, std::vector<details::Pin> & pins) const
    {
        if constexpr (N < sizeof...(Args)) {
            this->extractParam(std::get<N>(args), params.at(N), N, context, pins);
            this->extractParams<N + 1>(args, params, context, pins);
        }
        else {
            // validate number of parameters
            if (N < params.size()) {
                throw AnnotatedException(500, "Error in calling compute function '" + kernelName + "': not enough parameters");
            }
            else if (N > params.size()) {
                throw AnnotatedException(500, "Error in calling compute function '" + kernelName + "': too many parameters");
            }
            // Make sure all formal parameters are set
        }
    }

    template<typename Fn, typename... InitialArgs, typename Tuple, std::size_t... I>
    static MLDB_ALWAYS_INLINE void apply_impl(Fn && fn, const Tuple & tupleArgs, std::integer_sequence<size_t, I...>,
                          InitialArgs&&... initialArgs)
    {
        fn(std::forward<InitialArgs>(initialArgs)..., std::get<I>(tupleArgs)...);
    }

    template<typename Fn, typename... InitialArgs, typename... TupleArgs>
    static MLDB_ALWAYS_INLINE void apply(Fn && fn, const std::tuple<TupleArgs...> & tupleArgs, InitialArgs&&... initialArgs)
    {
        return apply_impl(std::forward<Fn>(fn), tupleArgs, std::make_index_sequence<sizeof...(TupleArgs)>{},
                          std::forward<InitialArgs>(initialArgs)...);
    }

    void checkComputeFunctionArity(size_t numExtraComputeFunctionArgs) const
    {
        if (numExtraComputeFunctionArgs != params.size()) {
            throw AnnotatedException(500, "Error setting compute function for '" + kernelName
                                     + "': compute function needs " + std::to_string(numExtraComputeFunctionArgs)
                                     + " but there are " + std::to_string(params.size()) + " parameters listed");
        }
    }

    // Perform the abstract bind() operation, returning a BoundComputeKernel
    virtual BoundComputeKernel bindImpl(std::vector<ComputeKernelArgument> arguments) const override;

#if 0
    // Enqueue the given kernel on the queue, returning the event
    virtual std::shared_ptr<ComputeEvent>
    enqueue(const BoundComputeKernel & bound,
            ComputeQueue & queue,
            std::span<ComputeKernelGridRange> grid,
            std::span<const std::shared_ptr<ComputeEvent>> prereqs) const override;
#endif

    virtual void call(const BoundComputeKernel & bound, std::span<ComputeKernelGridRange> grid) const;

#if 0
    template<typename N, typename Fn, typename... Args>
    void doSetComputeFunction(void (*fn) (ComputeContext & context, Args...))
    {

    }

    template<typename... Args>
    void setComputeFunction(void (*fn) (ComputeContext & context, Args...))
    {
        doSetComputeFunction<0>(fn);
    }

    template<typename... Args>
    void set1DComputeFunction(void (*fn) (ComputeContext & context, Args...))
    {
        doSetComputeFunction<1>(fn);
    }

    template<typename... Args>
    void set2DComputeFunction(void (*fn) (ComputeContext & context, Args...))
    {
        doSetComputeFunction<2>(fn);
    }

    template<typename... Args>
    void set3DComputeFunction(void (*fn) (ComputeContext & context, Args...))
    {
        doSetComputeFunction<3>(fn);
    }
#endif

    using Callable
        = std::function<std::shared_ptr<ComputeEvent> (ComputeContext & context, std::span<ComputeKernelGridRange> idx)>;

    using CreateCallable = std::function<Callable (ComputeContext & context, std::vector<ComputeKernelArgument> & params)>;
    CreateCallable createCallable;

    template<typename Fn>
    void setCreateCallable(Fn && createCallable)
    {
        this->createCallable = createCallable;
    }

    template<typename... Args>
    void setComputeFunction(void (*fn) (ComputeContext & context, Args...))
    {
        checkComputeFunctionArity(sizeof...(Args));

        auto result = [this, fn] (ComputeContext & context, std::vector<ComputeKernelArgument> & params) -> Callable
        {
            ExcAssertEqual(params.size(), sizeof...(Args));
            std::tuple<Args...> args;
            std::vector<details::Pin> pins;
            this->extractParams<0>(args, params, context, pins);
            return [fn, args, pins = std::move(pins)]
                (ComputeContext & context,
                 std::span<ComputeKernelGridRange> grid)
            {
                ExcAssertEqual(grid.size(), 0);
                HostComputeKernel::apply(fn, args, context);
                return std::make_shared<HostComputeEvent>();
            };
        };

        setCreateCallable(result);
    }

    template<typename... Args>
    void set1DComputeFunction(void (*fn) (ComputeContext & context, uint32_t i1, uint32_t r1, Args...))
    {
        checkComputeFunctionArity(sizeof...(Args));
        
        auto result = [this, fn] (ComputeContext & context, std::vector<ComputeKernelArgument> & params) -> Callable
        {
            ExcAssertEqual(params.size(), sizeof...(Args));
            std::tuple<Args...> args;
            std::vector<details::Pin> pins;
            this->extractParams<0>(args, params, context, pins);
            return [fn, args, pins = std::move(pins)]
                (ComputeContext & context,
                 std::span<ComputeKernelGridRange> grid)
            {
                ExcAssertEqual(grid.size(), 1);
                for (uint32_t idx: grid[0]) {
                    HostComputeKernel::apply(fn, args, context, idx, grid[0].range());
                }
                return std::make_shared<HostComputeEvent>();
            };
        };

        setCreateCallable(result);
    }

    template<typename... Args>
    void set1DComputeFunction(void (*fn) (ComputeContext & context, ComputeKernelGridRange & r1, Args...))
    {
        checkComputeFunctionArity(sizeof...(Args));
        
        auto result = [this, fn] (ComputeContext & context, std::vector<ComputeKernelArgument> & params) -> Callable
        {
            ExcAssertEqual(params.size(), sizeof...(Args));
            std::tuple<Args...> args;
            std::vector<details::Pin> pins;
            this->extractParams<0>(args, params, context, pins);
            return [fn, args, pins = std::move(pins)]
                (ComputeContext & context,
                 std::span<ComputeKernelGridRange> grid)
            {
                ExcAssertEqual(grid.size(), 1);
                HostComputeKernel::apply(fn, args, context, grid[0]);

                return std::make_shared<HostComputeEvent>();
            };
        };

        setCreateCallable(result);
    }

    template<typename... Args>
    void set2DComputeFunction(void (*fn) (ComputeContext & context, uint32_t i1, uint32_t r1, uint32_t i2, uint32_t r2, Args...))
    {
        auto result = [this, fn] (ComputeContext & context, std::vector<ComputeKernelArgument> & params) -> Callable
        {
            ExcAssertEqual(params.size(), sizeof...(Args));
            std::tuple<Args...> args;
            std::vector<details::Pin> pins;
            this->extractParams<0>(args, params, context, pins);
            return [fn, args, pins = std::move(pins)]
                (ComputeContext & context,
                 std::span<ComputeKernelGridRange> grid)
            {
                ExcAssertEqual(grid.size(), 2);
                for (uint32_t i0: grid[0]) {
                    for (uint32_t i1: grid[1]) {
                        HostComputeKernel::apply(fn, args, context, i0, grid[0].range(), i1, grid[1].range());
                    }
                }

                return std::make_shared<HostComputeEvent>();
            };
        };

        setCreateCallable(result);
    }

    template<typename... Args>
    void set2DComputeFunction(void (*fn) (ComputeContext & context, uint32_t i1, uint32_t r1, ComputeKernelGridRange & r2, Args...))
    {
        auto result = [this, fn] (ComputeContext & context, std::vector<ComputeKernelArgument> & params) -> Callable
        {
            ExcAssertEqual(params.size(), sizeof...(Args));
            std::tuple<Args...> args;
            std::vector<details::Pin> pins;
            this->extractParams<0>(args, params, context, pins);
            return [fn, args, pins = std::move(pins)] 
                (ComputeContext & context,
                 std::span<ComputeKernelGridRange> grid)
            {
                ExcAssertEqual(grid.size(), 2);
                for (uint32_t i0: grid[0]) {
                    HostComputeKernel::apply(fn, args, context, i0, grid[0].range(), grid[1]);
                }

                return std::make_shared<HostComputeEvent>();
            };
        };

        setCreateCallable(result);
    }

    template<typename... Args>
    void set2DComputeFunction(void (*fn) (ComputeContext & context, ComputeKernelGridRange & r1, uint32_t i2, uint32_t r2, Args...))
    {
        auto result = [this, fn] (ComputeContext & context, std::vector<ComputeKernelArgument> & params) -> Callable
        {
            ExcAssertEqual(params.size(), sizeof...(Args));
            std::tuple<Args...> args;
            std::vector<details::Pin> pins;
            this->extractParams<0>(args, params, context, pins);
            return [fn, args, pins = std::move(pins)]
                (ComputeContext & context,
                 std::span<ComputeKernelGridRange> grid)
            {
                ExcAssertEqual(grid.size(), 2);
                for (uint32_t i1: grid[1]) {
                    HostComputeKernel::apply(fn, args, context, grid[0], i1, grid[1].range());
                }

                return std::make_shared<HostComputeEvent>();
            };
        };

        setCreateCallable(result);
    }

    template<typename... Args>
    void set3DComputeFunction(void (*fn) (ComputeContext & context, uint32_t i1, uint32_t r1, uint32_t i2, uint32_t r2, ComputeKernelGridRange & r3, Args...))
    {
        auto result = [this, fn] (ComputeContext & context,std::vector<ComputeKernelArgument> & params) -> Callable
        {
            ExcAssertEqual(params.size(), sizeof...(Args));
            std::tuple<Args...> args;
            std::vector<details::Pin> pins;
            this->extractParams<0>(args, params, context, pins);
            return [fn, args, pins = std::move(pins)]
                (ComputeContext & context,
                 std::span<ComputeKernelGridRange> grid)
            {
                ExcAssertEqual(grid.size(), 3);
                for (uint32_t i0: grid[0]) {
                    for (uint32_t i1: grid[1]) {
                        HostComputeKernel::apply(fn, args, context,
                                                 i0, grid[0].range(),
                                                 i1, grid[1].range(),
                                                grid[2]);
                    }
                }

                return std::make_shared<HostComputeEvent>();
            };
        };

        setCreateCallable(result);
    }

    template<typename... Args>
    void set3DComputeFunction(void (*fn) (ComputeContext & context, ComputeKernelGridRange & r1, uint32_t i2, uint32_t r2, uint32_t i3, uint32_t r3, Args...))
    {
        auto result = [this, fn] (ComputeContext & context, std::vector<ComputeKernelArgument> & params) -> Callable
        {
            ExcAssertEqual(params.size(), sizeof...(Args));
            std::tuple<Args...> args;
            std::vector<details::Pin> pins;
            this->extractParams<0>(args, params, context, pins);
            return [fn, args, pins = std::move(pins)]
                (ComputeContext & context,
                 std::span<ComputeKernelGridRange> grid)
            {
               ExcAssertEqual(grid.size(), 3);
                for (uint32_t i1: grid[1]) {
                    for (uint32_t i2: grid[2]) {
                        HostComputeKernel::apply(fn, args, context,
                                                 grid[0],
                                                 i1, grid[1].range(),
                                                 i2, grid[2].range());
                    }
                }
                return std::make_shared<HostComputeEvent>();
            };
        };

        setCreateCallable(result);
    }

};


// HostComputeQueue

struct HostComputeQueue: public ComputeQueue {
    HostComputeQueue(ComputeContext * owner)
        : ComputeQueue(owner)
    {
    }

    virtual ~HostComputeQueue() = default;

    virtual std::shared_ptr<ComputeEvent>
    launch(const std::string & opName,
           const BoundComputeKernel & kernel,
           const std::vector<uint32_t> & grid,
           const std::vector<std::shared_ptr<ComputeEvent>> & prereqs = {}) override;

    virtual ComputePromiseT<MemoryRegionHandle>
    enqueueFillArrayImpl(const std::string & opName,
                         MemoryRegionHandle region, MemoryRegionInitialization init,
                         size_t startOffsetInBytes, ssize_t lengthInBytes,
                         const std::any & arg,
                         std::vector<std::shared_ptr<ComputeEvent>> prereqs) override;

    virtual void flush() override;

    virtual void finish() override;

    virtual std::shared_ptr<ComputeEvent>
    makeAlreadyResolvedEvent() const;
};

void registerHostComputeKernel(const std::string & kernelName,
                           std::function<std::shared_ptr<ComputeKernel>()> generator);

}  // namespace MLDB