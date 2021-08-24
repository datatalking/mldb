/** opencl_types.h
    Jeremy Barnes, 21 September 2017
    Copyright (c) 2017 Element AI Inc.  All rights reserved.
    This file is part of MLDB. Copyright 2015 mldb.ai inc. All rights reserved.

    OpenCL plugin, to allow execution of OpenCL code and OpenCL functions
    to be defined.
*/

#include "opencl_types.h"
#include "mldb/types/basic_value_descriptions.h"
#include "mldb/types/array_description.h"
#include "mldb/types/set_description.h"
#include "mldb/http/http_exception.h"
#include <regex>
#include <iostream>

using namespace std;

namespace MLDB {


/*****************************************************************************/
/* OPENCL EXCEPTION                                                          */
/*****************************************************************************/

std::string
OpenCLException::
printCode(cl_int returnCode)
{
    return jsonEncode(OpenCLStatus(returnCode)).asString();
}

void checkOpenCLError(cl_int returnCode,
                      const char * operation)
{
    if (returnCode == CL_SUCCESS)
        return;
    throw OpenCLException(returnCode, operation);
}

void checkOpenCLError(cl_int returnCode,
                      const std::string & operation)
{
    checkOpenCLError(returnCode, operation.c_str());
}


/*****************************************************************************/
/* OPENCL STATUS                                                             */
/*****************************************************************************/

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLStatus)
{
#define DO_VALUE(name) addValue(#name, (OpenCLStatus)CL_##name)
    DO_VALUE(SUCCESS);
    DO_VALUE(DEVICE_NOT_FOUND);
    DO_VALUE(DEVICE_NOT_AVAILABLE);
    DO_VALUE(COMPILER_NOT_AVAILABLE);
    DO_VALUE(MEM_OBJECT_ALLOCATION_FAILURE);
    DO_VALUE(OUT_OF_RESOURCES);
    DO_VALUE(OUT_OF_HOST_MEMORY);
    DO_VALUE(PROFILING_INFO_NOT_AVAILABLE);
    DO_VALUE(MEM_COPY_OVERLAP);
    DO_VALUE(IMAGE_FORMAT_MISMATCH);
    DO_VALUE(IMAGE_FORMAT_NOT_SUPPORTED);
    DO_VALUE(BUILD_PROGRAM_FAILURE);
    DO_VALUE(MAP_FAILURE);
    DO_VALUE(MISALIGNED_SUB_BUFFER_OFFSET);
    DO_VALUE(EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
    DO_VALUE(COMPILE_PROGRAM_FAILURE);
    DO_VALUE(LINKER_NOT_AVAILABLE);
    DO_VALUE(LINK_PROGRAM_FAILURE);
    DO_VALUE(DEVICE_PARTITION_FAILED);
    DO_VALUE(KERNEL_ARG_INFO_NOT_AVAILABLE);

    DO_VALUE(INVALID_VALUE);
    DO_VALUE(INVALID_DEVICE_TYPE);
    DO_VALUE(INVALID_PLATFORM);
    DO_VALUE(INVALID_DEVICE);
    DO_VALUE(INVALID_CONTEXT);
    DO_VALUE(INVALID_QUEUE_PROPERTIES);
    DO_VALUE(INVALID_COMMAND_QUEUE);
    DO_VALUE(INVALID_HOST_PTR);
    DO_VALUE(INVALID_MEM_OBJECT);
    DO_VALUE(INVALID_IMAGE_FORMAT_DESCRIPTOR);
    DO_VALUE(INVALID_IMAGE_SIZE);
    DO_VALUE(INVALID_SAMPLER);
    DO_VALUE(INVALID_BINARY);
    DO_VALUE(INVALID_BUILD_OPTIONS);
    DO_VALUE(INVALID_PROGRAM);
    DO_VALUE(INVALID_PROGRAM_EXECUTABLE);
    DO_VALUE(INVALID_KERNEL_NAME);
    DO_VALUE(INVALID_KERNEL_DEFINITION);
    DO_VALUE(INVALID_KERNEL);
    DO_VALUE(INVALID_ARG_INDEX);
    DO_VALUE(INVALID_ARG_VALUE);
    DO_VALUE(INVALID_ARG_SIZE);
    DO_VALUE(INVALID_KERNEL_ARGS);
    DO_VALUE(INVALID_WORK_DIMENSION);
    DO_VALUE(INVALID_WORK_GROUP_SIZE);
    DO_VALUE(INVALID_WORK_ITEM_SIZE);
    DO_VALUE(INVALID_GLOBAL_OFFSET);
    DO_VALUE(INVALID_EVENT_WAIT_LIST);
    DO_VALUE(INVALID_EVENT);
    DO_VALUE(INVALID_OPERATION);
    DO_VALUE(INVALID_GL_OBJECT);
    DO_VALUE(INVALID_BUFFER_SIZE);
    DO_VALUE(INVALID_MIP_LEVEL);
    DO_VALUE(INVALID_GLOBAL_WORK_SIZE);
    DO_VALUE(INVALID_PROPERTY);
    DO_VALUE(INVALID_IMAGE_DESCRIPTOR);
    DO_VALUE(INVALID_COMPILER_OPTIONS);
    DO_VALUE(INVALID_LINKER_OPTIONS);
    DO_VALUE(INVALID_DEVICE_PARTITION_COUNT);
    DO_VALUE(INVALID_PIPE_SIZE);
    DO_VALUE(INVALID_DEVICE_QUEUE);
#undef DO_VALUE
};

/*****************************************************************************/

/*****************************************************************************/
/* PROPERTY GETTERS                                                          */
/*****************************************************************************/

struct OpenCLIgnoreExceptions {
};

template<typename... Args>
struct ThrowArgException;

template<>
struct ThrowArgException<> {
    static constexpr bool value = true;
};

template<typename First, typename... Others>
struct ThrowArgException<First, Others...> {
    static constexpr bool value = ThrowArgException<Others...>::value;
};

template<typename... Others>
struct ThrowArgException<OpenCLIgnoreExceptions, Others...> {
    static constexpr bool value = false;
};

void throwInfoException(cl_int code,
                        const ValueDescription & desc,
                        const void * field,
                        const void * base) MLDB_NORETURN;

void throwInfoException(cl_int code,
                        const ValueDescription & desc,
                        const void * field,
                        const void * base)
{
    // Use the value info to find which field caused the error

    const ValueDescription::FieldDescription * fieldDescription
        = desc.getFieldDescription(base, field);
    std::string fieldName;
    if (!fieldDescription) {
        fieldName = "<unknown field>";
    }
    else {
        fieldName = fieldDescription->fieldName;
    }
    throw OpenCLException(code, "clGetXXXInfo "
                          + desc.typeName
                          + "::" + fieldName);
}

template<typename Fn>
cl_int extractArgType(Fn&&fn, std::string & val)
{
    size_t len = 0;
    int res = fn(0, nullptr, &len);
    if (res != CL_SUCCESS)
        return res;
    
    char buf[len];
    res = fn(len, buf, nullptr);
    if (res != CL_SUCCESS)
        return res;

    val = string(buf, buf + len);
    return CL_SUCCESS;
}

template<typename Fn>
cl_int extractArgType(Fn&&fn, std::vector<std::string> & val,
                      const std::regex & splitOn)
{
    std::string unsplit;
    cl_int res = extractArgType(std::forward<Fn>(fn), unsplit);
    if (res != CL_SUCCESS)
        return res;
    
    val = { std::sregex_token_iterator(unsplit.begin(),
                                       unsplit.end(),
                                       splitOn),
            std::sregex_token_iterator() };

    if (val.size() == 1 && val[0] == "") {
        val.clear();
    }

    return CL_SUCCESS;
}

template<typename Fn, typename T>
cl_int extractArgType(Fn&&fn, std::vector<T> & where,
                      typename std::enable_if<std::is_pod<T>::value>::type * = 0)
{
    size_t len = 0;
    int res = fn(0, nullptr, &len);
    if (res != CL_SUCCESS)
        return res;

    std::vector<T> result(len / sizeof(T));
        
    res = fn(len, result.data(), nullptr);

    if (res != CL_SUCCESS)
        return res;

    where = std::move(result);

    return res;
}

template<typename Fn, typename T>
cl_int extractArgType(Fn&&fn, Bitset<T> & where)
{
    return extractArgType(std::forward<Fn>(fn), where.val);
}

template<typename Fn, typename T>
cl_int extractArgType(Fn && fn, T & val,
                      typename std::enable_if<std::is_pod<T>::value>::type * = 0)
{
    return fn(sizeof(val), &val, nullptr);
}

template<typename Fn, typename T>
cl_int extractArgType(Fn && fn, T& val, OpenCLIgnoreExceptions)
{
    return extractArgType(std::forward<Fn>(fn), val);
}

template<typename Fn, typename T, typename Arg>
cl_int extractArgType(Fn && fn, T& val, Arg&& arg, OpenCLIgnoreExceptions)
{
    return extractArgType(std::forward<Fn>(fn), val, std::forward<Arg>(arg));
}

template<typename Fn, typename T, typename Base, typename... Args>
cl_int doArgType(Fn&& fn, T & where, Base * base, Args&&... args)
{
    cl_int res = extractArgType(std::forward<Fn>(fn), where,
                                std::forward<Args>(args)...);
    if (res == CL_SUCCESS)
        return res;

    // Throw the right exception
    static const auto descr = getDefaultDescriptionSharedT<Base>();

    if (ThrowArgException<Args...>::value) {
        throwInfoException(res, *descr, &where, base);
    }

    return res;
}

template<typename Entity, typename T, typename Base, typename... Args>
cl_int clInfoCall(cl_int (*fn) (Entity obj, cl_uint what, size_t, void *, size_t *),
                Entity obj,
                cl_uint what,
                T & where, Base * base,
                Args&&... args)
{
    auto bound = [&] (size_t szin, void * arg, size_t * szout) -> cl_int
        {
            return fn(obj, what, szin, arg, szout);
        };

    return doArgType(bound, where, base, std::forward<Args>(args)...);
}

template<typename Entity, typename Param, typename T, typename Base, typename... Args>
cl_int clInfoCall(cl_int (*fn) (Entity obj, Param param, cl_uint what, size_t, void *, size_t *),
                Entity obj,
                Param param,
                cl_uint what,
                T & where, Base * base,
                Args&&... args)
{
    auto bound = [&] (size_t szin, void * arg, size_t * szout) -> cl_int
        {
            return fn(obj, param, what, szin, arg, szout);
        };

    return doArgType(bound, where, base, std::forward<Args>(args)...);
}


/*****************************************************************************/
/* OPENCL DEVICE INFO                                                        */
/*****************************************************************************/

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLFpConfig)
{
    addValue("DENORM", OpenCLFpConfig::DENORM);
    addValue("INF_NAN", OpenCLFpConfig::INF_NAN);
    addValue("ROUND_TO_NEAREST", OpenCLFpConfig::ROUND_TO_NEAREST);
    addValue("ROUND_TO_ZERO", OpenCLFpConfig::ROUND_TO_ZERO);
    addValue("ROUND_TO_INF", OpenCLFpConfig::ROUND_TO_INF);
    addValue("FMA", OpenCLFpConfig::FMA);
    addValue("SOFT_FLOAT", OpenCLFpConfig::SOFT_FLOAT);
    addValue("CORRECTLY_ROUNDED_DIVIDE_SQRT",
             OpenCLFpConfig::CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLCacheType)
{
    addValue("NONE", OpenCLCacheType::NONE);
    addValue("READ_ONLY", OpenCLCacheType::READ_ONLY);
    addValue("READ_WRITE", OpenCLCacheType::READ_WRITE);
}

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLExecutionCapabilities)
{
    addValue("NONE", OpenCLExecutionCapabilities::NONE);
    addValue("KERNEL", OpenCLExecutionCapabilities::KERNEL);
    addValue("NATIVE_KERNEL", OpenCLExecutionCapabilities::NATIVE_KERNEL);
}

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLLocalMemoryType)
{
    addValue("NONE", OpenCLLocalMemoryType::NONE);
    addValue("LOCAL", OpenCLLocalMemoryType::LOCAL);
    addValue("GLOBAL", OpenCLLocalMemoryType::GLOBAL);
}

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLPartitionProperty)
{
    addValue("EQUALLY", OpenCLPartitionProperty::EQUALLY);
    addValue("BY_COUNTS", OpenCLPartitionProperty::BY_COUNTS);
    addValue("BY_COUNTS_LIST_END", OpenCLPartitionProperty::BY_COUNTS_LIST_END);
    addValue("BY_AFFINITY_DOMAIN",
             OpenCLPartitionProperty::BY_AFFINITY_DOMAIN);
    
}

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLPartitionAffinityDomain)
{
    addValue("NUMA", OpenCLPartitionAffinityDomain::NUMA);
    addValue("L4_CACHE", OpenCLPartitionAffinityDomain::L4_CACHE);
    addValue("L3_CACHE", OpenCLPartitionAffinityDomain::L3_CACHE);
    addValue("L2_CACHE", OpenCLPartitionAffinityDomain::L2_CACHE);
    addValue("L1_CACHE", OpenCLPartitionAffinityDomain::L1_CACHE);
    addValue("NEXT_PARTITIONABLE",
             OpenCLPartitionAffinityDomain::NEXT_PARTITIONABLE);
}

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLDeviceQueueProperties)
{
    addValue("OUT_OF_ORDER_EXEC_MODE_ENABLE",
             OpenCLDeviceQueueProperties::OUT_OF_ORDER_EXEC_MODE_ENABLE);
    addValue("PROFILING_ENABLE",
             OpenCLDeviceQueueProperties::PROFILING_ENABLE);
    addValue("ON_DEVICE", OpenCLDeviceQueueProperties::ON_DEVICE);
    addValue("ON_DEVICE_DEFAULT",
             OpenCLDeviceQueueProperties::ON_DEVICE_DEFAULT);
}

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLDeviceType)
{
    addValue("DEFAULT", OpenCLDeviceType::DEFAULT);
    addValue("CPU", OpenCLDeviceType::CPU);
    addValue("GPU", OpenCLDeviceType::GPU);
    addValue("ACCELERATOR", OpenCLDeviceType::ACCELERATOR);
    addValue("CUSTOM", OpenCLDeviceType::CUSTOM);
}

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLDeviceSvmCapabilities)
{
    addValue("COARSE_GRAIN_BUFFER",
             OpenCLDeviceSvmCapabilities::COARSE_GRAIN_BUFFER);
    addValue("FINE_GRAIN_BUFFER",
             OpenCLDeviceSvmCapabilities::FINE_GRAIN_BUFFER);
    addValue("FINE_GRAIN_SYSTEM",
             OpenCLDeviceSvmCapabilities::FINE_GRAIN_SYSTEM);
    addValue("ATOMICS",
             OpenCLDeviceSvmCapabilities::ATOMICS);
}


/*****************************************************************************/
/* OPENCL DEVICE INFO                                                        */
/*****************************************************************************/

OpenCLDeviceInfo::
OpenCLDeviceInfo(cl_device_id device)
    : device(device)
{
#define DO_FIELD(name, id) doField(CL_DEVICE_##id, name)
    DO_FIELD(addressBits, ADDRESS_BITS);
    DO_FIELD(available, AVAILABLE);
    static const std::regex splitExtensionsOn("[^ ]+");
    doField(CL_DEVICE_BUILT_IN_KERNELS, builtInKernels, splitExtensionsOn);
    DO_FIELD(compilerAvailable, COMPILER_AVAILABLE);
    DO_FIELD(singleFpConfig, SINGLE_FP_CONFIG);
    DO_FIELD(doubleFpConfig, DOUBLE_FP_CONFIG);
    DO_FIELD(endianLittle, ENDIAN_LITTLE);
    DO_FIELD(errorCorrection, ERROR_CORRECTION_SUPPORT);
    DO_FIELD(executionCapabilities, EXECUTION_CAPABILITIES);
    doField(CL_DEVICE_EXTENSIONS, extensions, splitExtensionsOn);
    DO_FIELD(globalMemCacheSize, GLOBAL_MEM_CACHE_SIZE);
    DO_FIELD(globalMemCacheType, GLOBAL_MEM_CACHE_TYPE);
    DO_FIELD(globalMemCacheLineSize, GLOBAL_MEM_CACHELINE_SIZE);
    DO_FIELD(globalMemSize, GLOBAL_MEM_SIZE);
    doField(CL_DEVICE_HALF_FP_CONFIG, halfFpConfig, OpenCLIgnoreExceptions());
    DO_FIELD(unifiedMemory, HOST_UNIFIED_MEMORY);
    DO_FIELD(imageSupport, IMAGE_SUPPORT);
    DO_FIELD(image2dMaxDimensions, IMAGE2D_MAX_WIDTH);
    DO_FIELD(image3dMaxDimensions, IMAGE3D_MAX_WIDTH);
    DO_FIELD(imageMaxBufferSize, IMAGE_MAX_BUFFER_SIZE);
    DO_FIELD(imageMaxArraySize, IMAGE_MAX_ARRAY_SIZE);
    DO_FIELD(linkerAvailable, LINKER_AVAILABLE);
    DO_FIELD(localMemSize, LOCAL_MEM_SIZE);
    DO_FIELD(localMemType, LOCAL_MEM_TYPE);
    DO_FIELD(maxClockFrequency, MAX_CLOCK_FREQUENCY);
    DO_FIELD(maxComputeUnits, MAX_COMPUTE_UNITS);
    DO_FIELD(maxConstantArgs, MAX_CONSTANT_ARGS);
    DO_FIELD(maxConstantBufferSize, MAX_CONSTANT_BUFFER_SIZE);
    DO_FIELD(maxMemAllocSize, MAX_MEM_ALLOC_SIZE);
    DO_FIELD(maxParameterSize, MAX_PARAMETER_SIZE);
    DO_FIELD(maxReadImageArgs, MAX_READ_IMAGE_ARGS);
    DO_FIELD(maxSamplers, MAX_SAMPLERS);
    DO_FIELD(maxWorkGroupSize, MAX_WORK_GROUP_SIZE);
    DO_FIELD(maxWorkItemDimensions, MAX_WORK_ITEM_DIMENSIONS);
    DO_FIELD(maxWorkItemSizes, MAX_WORK_ITEM_SIZES);
    DO_FIELD(maxWriteImageArgs, MAX_WRITE_IMAGE_ARGS);
    DO_FIELD(memBaseAddrAlign, MEM_BASE_ADDR_ALIGN);
    DO_FIELD(name, NAME);
    DO_FIELD(nativeVectorWidth, NATIVE_VECTOR_WIDTH_CHAR);
    DO_FIELD(openCLCVersion, OPENCL_C_VERSION);
    DO_FIELD(partitionMaxSubDevices, PARTITION_MAX_SUB_DEVICES);
    DO_FIELD(partitionProperties, PARTITION_PROPERTIES);
    DO_FIELD(partitionAffinityDomain, PARTITION_AFFINITY_DOMAIN);
    DO_FIELD(partitionType, PARTITION_TYPE);
    DO_FIELD(preferredVectorWidth, PREFERRED_VECTOR_WIDTH_CHAR);
    DO_FIELD(printfBufferSize, PRINTF_BUFFER_SIZE);
    DO_FIELD(preferredInteropUserSync, PREFERRED_INTEROP_USER_SYNC);
    DO_FIELD(profile, PROFILE);
    DO_FIELD(profilingTimerResolution, PROFILING_TIMER_RESOLUTION);
    DO_FIELD(queueProperties, QUEUE_PROPERTIES);
    DO_FIELD(referenceCount, REFERENCE_COUNT);
    DO_FIELD(type, TYPE);
    DO_FIELD(vendor, VENDOR);
    DO_FIELD(vendorId, VENDOR_ID);
    DO_FIELD(version, VERSION);
    doField(CL_DRIVER_VERSION, driverVersion);
    doField(CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF, preferredVectorWidth[6]);


#define DO_OPTIONAL(name, id) doField(CL_DEVICE_##id, name, OpenCLIgnoreExceptions());

    DO_OPTIONAL(svmCapabilities, SVM_CAPABILITIES);
    DO_OPTIONAL(imagePitchAlignment, IMAGE_PITCH_ALIGNMENT);
    DO_OPTIONAL(imageBaseAddressAlignment, IMAGE_BASE_ADDRESS_ALIGNMENT);
    DO_OPTIONAL(maxReadWriteImageArgs, MAX_READ_WRITE_IMAGE_ARGS);
    DO_OPTIONAL(maxGlobalVariableSize, MAX_GLOBAL_VARIABLE_SIZE);
    DO_OPTIONAL(globalVariablePreferredTotalSize,
                GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE);
    DO_OPTIONAL(pipeMaxActiveReservations, PIPE_MAX_ACTIVE_RESERVATIONS);
    DO_OPTIONAL(pipeMaxPacketSize, PIPE_MAX_PACKET_SIZE);
    DO_OPTIONAL(maxOnDeviceQueues, MAX_ON_DEVICE_QUEUES);
    DO_OPTIONAL(maxOnDeviceEvents, MAX_ON_DEVICE_EVENTS);
    DO_OPTIONAL(queueOnDeviceMaxSize, QUEUE_ON_DEVICE_MAX_SIZE);
    DO_OPTIONAL(queueOnDevicePreferredSize, QUEUE_ON_DEVICE_PREFERRED_SIZE);
    DO_OPTIONAL(queueOnDeviceProperties, QUEUE_ON_DEVICE_PROPERTIES);
    DO_OPTIONAL(maxPipeArgs, MAX_PIPE_ARGS);
    DO_OPTIONAL(pipeMaxActiveReservations, PIPE_MAX_ACTIVE_RESERVATIONS);
    DO_OPTIONAL(pipeMaxPacketSize, PIPE_MAX_PACKET_SIZE);
    DO_OPTIONAL(preferredPlatformAtomicAlignment,
                PREFERRED_PLATFORM_ATOMIC_ALIGNMENT);
    DO_OPTIONAL(preferredGlobalAtomicAlignment,
                PREFERRED_GLOBAL_ATOMIC_ALIGNMENT);
    DO_OPTIONAL(preferredLocalAtomicAlignment,
                PREFERRED_LOCAL_ATOMIC_ALIGNMENT);
    DO_OPTIONAL(partitionProperties, PARTITION_PROPERTIES);
#undef DO_FIELD
#undef DO_OPTIONAL
}

template<typename T, typename... Args>
void
OpenCLDeviceInfo::
doField(cl_uint what, T & where, Args&&... args)
{
    clInfoCall(clGetDeviceInfo, device, what, where, this,
               std::forward<Args>(args)...);
}

template<typename T, size_t N>
void
OpenCLDeviceInfo::
doField(cl_device_info what, std::array<T, N> & where)
{
    for (size_t i = 0;  i < N;  ++i) {
        doField(what + i, where[i]);
    }
}

#if 0
template<typename T>
void
OpenCLDeviceInfo::
doField(cl_device_info id, T & val,
        const char * call,
        std::enable_if<std::is_pod<T>::value>::type *)
{
    int res = clGetDeviceInfo(device, id, sizeof(val), &val, nullptr);
    checkOpenCLError(res, "glGetDeviceInfo(" + string(call) + ")");
}

void
OpenCLDeviceInfo::
doField(cl_device_info what, std::string & where,
        const char * call)
{
    size_t len = 0;
    int res = clGetDeviceInfo(device, what, 0, nullptr, &len);
    checkOpenCLError(res, "glGetDeviceInfo(" + string(call) + ")");

    char buf[len];
        
    res = clGetDeviceInfo(device, what, len, buf, nullptr);

    if (res != CL_SUCCESS) {
        throw HttpReturnException(400, "clGetDeviceInfo: "
                                  + std::to_string(res));
    }

    where = string(buf, buf + len);
}

void
OpenCLDeviceInfo::
doField(cl_device_info what, std::vector<std::string> & where,
        const char * call)
{
    std::string unsplit;
    doField(what, unsplit, call);

    static const std::regex splitOn("[^ ]+");
    where = { std::sregex_token_iterator(unsplit.begin(),
                                         unsplit.end(),
                                         splitOn),
              std::sregex_token_iterator() };

    if (where.size() == 1 && where[0] == "") {
        where.clear();
    }
}

#endif

DEFINE_STRUCTURE_DESCRIPTION_INLINE(OpenCLDeviceInfo)
{
#define DO_FIELD(name) addField(#name, &OpenCLDeviceInfo::name, "")
    DO_FIELD(addressBits);
    DO_FIELD(available);
    DO_FIELD(builtInKernels);
    DO_FIELD(compilerAvailable);
    DO_FIELD(singleFpConfig);
    DO_FIELD(doubleFpConfig);
    DO_FIELD(endianLittle);
    DO_FIELD(errorCorrection);
    DO_FIELD(executionCapabilities);
    DO_FIELD(extensions);
    DO_FIELD(globalMemCacheSize);
    DO_FIELD(globalMemCacheType);
    DO_FIELD(globalMemCacheLineSize);
    DO_FIELD(globalMemSize);
    DO_FIELD(halfFpConfig);
    DO_FIELD(unifiedMemory);
    DO_FIELD(imageSupport);
    DO_FIELD(image2dMaxDimensions);
    DO_FIELD(image3dMaxDimensions);
    DO_FIELD(imageMaxBufferSize);
    DO_FIELD(imageMaxArraySize);
    DO_FIELD(linkerAvailable);
    DO_FIELD(localMemSize);
    DO_FIELD(localMemType);
    DO_FIELD(maxClockFrequency);
    DO_FIELD(maxComputeUnits);
    DO_FIELD(maxConstantArgs);
    DO_FIELD(maxConstantBufferSize);
    DO_FIELD(maxMemAllocSize);
    DO_FIELD(maxParameterSize);
    DO_FIELD(maxReadImageArgs);
    DO_FIELD(maxSamplers);
    DO_FIELD(maxWorkGroupSize);
    DO_FIELD(maxWorkItemDimensions);
    DO_FIELD(maxWorkItemSizes);
    DO_FIELD(maxWriteImageArgs);
    DO_FIELD(memBaseAddrAlign);
    DO_FIELD(name);
    DO_FIELD(nativeVectorWidth);
    DO_FIELD(openCLCVersion);
    DO_FIELD(partitionMaxSubDevices);
    DO_FIELD(partitionProperties);
    DO_FIELD(partitionAffinityDomain);
    DO_FIELD(partitionType);
    DO_FIELD(preferredVectorWidth);
    DO_FIELD(printfBufferSize);
    DO_FIELD(preferredInteropUserSync);
    DO_FIELD(profile);
    DO_FIELD(profilingTimerResolution);
    DO_FIELD(queueProperties);
    DO_FIELD(referenceCount);
    DO_FIELD(type);
    DO_FIELD(vendor);
    DO_FIELD(vendorId);
    DO_FIELD(version);
    DO_FIELD(driverVersion);

    DO_FIELD(svmCapabilities);
    DO_FIELD(imagePitchAlignment);
    DO_FIELD(imageBaseAddressAlignment);
    DO_FIELD(maxReadWriteImageArgs);
    DO_FIELD(maxGlobalVariableSize);
    DO_FIELD(globalVariablePreferredTotalSize);
    DO_FIELD(pipeMaxActiveReservations);
    DO_FIELD(pipeMaxPacketSize);
    DO_FIELD(maxOnDeviceQueues);
    DO_FIELD(maxOnDeviceEvents);
    DO_FIELD(queueOnDeviceMaxSize);
    DO_FIELD(queueOnDevicePreferredSize);
    DO_FIELD(queueOnDeviceProperties);
    DO_FIELD(maxPipeArgs);

    DO_FIELD(preferredPlatformAtomicAlignment);
    DO_FIELD(preferredGlobalAtomicAlignment);
    DO_FIELD(preferredLocalAtomicAlignment);

#undef DO_FIELD
}


/*****************************************************************************/
/* OPENCL PLATFORM INFO                                                      */
/*****************************************************************************/

OpenCLPlatformInfo::
OpenCLPlatformInfo(cl_platform_id platform)
    : platform(platform)
{
    get(CL_PLATFORM_PROFILE, profile, "PROFILE");
    get(CL_PLATFORM_VERSION, version, "VERSION");
    get(CL_PLATFORM_NAME, name, "NAME");
    get(CL_PLATFORM_VENDOR, vendor, "VENDOR");

    std::string extensionsStr;
    get(CL_PLATFORM_EXTENSIONS, extensionsStr, "EXTENSIONS");

    static const std::regex splitOn("[^ ]+");
    extensions = { std::sregex_token_iterator(extensionsStr.begin(),
                                              extensionsStr.end(),
                                              splitOn),
                   std::sregex_token_iterator() };
}
    
void
OpenCLPlatformInfo::
get(cl_platform_info what, std::string & where,
    const char * call)
{
    size_t len = 0;
    int res = clGetPlatformInfo(platform, what, 0, nullptr, &len);
    checkOpenCLError(res, "glGetPlatformInfo(" + string(call) + ")");

    char buf[len];
        
    res = clGetPlatformInfo(platform, what, len, buf, nullptr);
    checkOpenCLError(res, "glGetPlatformInfo(" + string(call) + ")");

    where = string(buf, buf + len);
}
    
IMPLEMENT_STRUCTURE_DESCRIPTION(OpenCLPlatformInfo)
{
    addField("profile", &OpenCLPlatformInfo::profile,
             "OpenCL profile version of platform");
    addField("version", &OpenCLPlatformInfo::version,
             "OpenCL profile version number of platform");
    addField("name", &OpenCLPlatformInfo::name,
             "OpenCL profile name of platform");
    addField("vendor", &OpenCLPlatformInfo::vendor,
             "OpenCL platform vendor name");
    addField("extensions", &OpenCLPlatformInfo::extensions,
             "OpenCL platform vendor extensions");
}

/*****************************************************************************/
/* OPENCL PROGRAM BUILD INFO                                                 */
/*****************************************************************************/

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLBuildStatus)
{
    addValue("NONE", OpenCLBuildStatus::NONE);
    addValue("ERROR", OpenCLBuildStatus::ERROR);
    addValue("SUCCESS", OpenCLBuildStatus::SUCCESS);
    addValue("IN_PROGRESS", OpenCLBuildStatus::IN_PROGRESS);
}

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLBinaryType)
{
    addValue("NONE", OpenCLBinaryType::NONE);
    addValue("COMPILED_OBJECT", OpenCLBinaryType::COMPILED_OBJECT);
    addValue("LIBRARY", OpenCLBinaryType::LIBRARY);
    addValue("EXECUTABLE", OpenCLBinaryType::EXECUTABLE);
}

OpenCLProgramBuildInfo::
OpenCLProgramBuildInfo(cl_program program, cl_device_id device)
    : program(program), device(device)
{
    doField(CL_PROGRAM_BUILD_STATUS, buildStatus);
    doField(CL_PROGRAM_BUILD_OPTIONS, buildOptions);
    doField(CL_PROGRAM_BUILD_LOG, buildLog);
    doField(CL_PROGRAM_BINARY_TYPE, binaryType);
}

template<typename T, typename... Args>
void
OpenCLProgramBuildInfo::
doField(cl_uint what, T & where, Args&&... args)
{
    clInfoCall(clGetProgramBuildInfo, program, device, what, where, this,
               std::forward<Args>(args)...);
}

DEFINE_STRUCTURE_DESCRIPTION_INLINE(OpenCLProgramBuildInfo)
{
    addField("buildStatus", &OpenCLProgramBuildInfo::buildStatus, "");
    addField("buildOptions", &OpenCLProgramBuildInfo::buildOptions, "");
    addField("buildLog", &OpenCLProgramBuildInfo::buildLog, "");
    addField("binaryType", &OpenCLProgramBuildInfo::binaryType, "");
}


/*****************************************************************************/
/* OPENCL KERNEL INFO                                                        */
/*****************************************************************************/

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLArgAddressQualifier)
{
    addValue("GLOBAL", OpenCLArgAddressQualifier::GLOBAL);
    addValue("LOCAL", OpenCLArgAddressQualifier::LOCAL);
    addValue("CONSTANT", OpenCLArgAddressQualifier::CONSTANT);
    addValue("PRIVATE", OpenCLArgAddressQualifier::PRIVATE);
}

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLArgAccessQualifier)
{
    addValue("READ_ONLY", OpenCLArgAccessQualifier::READ_ONLY);
    addValue("WRITE_ONLY", OpenCLArgAccessQualifier::WRITE_ONLY);
    addValue("READ_WRITE", OpenCLArgAccessQualifier::READ_WRITE);
    addValue("NONE", OpenCLArgAccessQualifier::NONE);
}

DEFINE_ENUM_DESCRIPTION_INLINE(OpenCLArgTypeQualifier)
{
    addValue("NONE", OpenCLArgTypeQualifier::NONE);
    addValue("CONST", OpenCLArgTypeQualifier::CONST);
    addValue("RESTRICT", OpenCLArgTypeQualifier::RESTRICT);
    addValue("VOLATILE", OpenCLArgTypeQualifier::VOLATILE);
    addValue("PIPE", OpenCLArgTypeQualifier::PIPE);
}

OpenCLKernelArgInfo::
OpenCLKernelArgInfo(cl_kernel kernel,
                    cl_uint argNum)
    : kernel(kernel), argNum(argNum)
{
    doField(CL_KERNEL_ARG_ADDRESS_QUALIFIER, addressQualifier);
    doField(CL_KERNEL_ARG_ACCESS_QUALIFIER, accessQualifier);
    doField(CL_KERNEL_ARG_TYPE_NAME, typeName);
    doField(CL_KERNEL_ARG_TYPE_QUALIFIER, typeQualifier);
    doField(CL_KERNEL_ARG_NAME, name);
}

template<typename T, typename... Args>
void
OpenCLKernelArgInfo::
doField(cl_uint what, T & where, Args&&... args)
{
    clInfoCall(clGetKernelArgInfo, kernel, argNum, what, where, this,
               std::forward<Args>(args)...);
}

DEFINE_STRUCTURE_DESCRIPTION_INLINE(OpenCLKernelArgInfo)
{
    addField("addressQualifier", &OpenCLKernelArgInfo::addressQualifier,
             "");
    addField("accessQualifier", &OpenCLKernelArgInfo::accessQualifier,
             "");
    addField("typeName", &OpenCLKernelArgInfo::typeName, "");
    addField("typeQualifier", &OpenCLKernelArgInfo::typeQualifier, "");
    addField("name", &OpenCLKernelArgInfo::name, "");
}

OpenCLKernelInfo::
OpenCLKernelInfo(cl_kernel kernel)
    : kernel(kernel)
{
    doField(CL_KERNEL_FUNCTION_NAME, functionName);
    doField(CL_KERNEL_NUM_ARGS, numArgs);
    static const std::regex splitAttributesOn("[^ ]+");
    doField(CL_KERNEL_ATTRIBUTES, attributes, splitAttributesOn);
    
    args.reserve(numArgs);
    for (size_t i = 0;  i < numArgs;  ++i) {
        args.emplace_back(kernel, i);
    }
}

template<typename T, typename... Args>
void
OpenCLKernelInfo::
doField(cl_uint what, T & where, Args&&... args)
{
    clInfoCall(clGetKernelInfo, kernel, what, where, this,
               std::forward<Args>(args)...);
}

DEFINE_STRUCTURE_DESCRIPTION_INLINE(OpenCLKernelInfo)
{
    addField("functionName", &OpenCLKernelInfo::functionName, "");
    addField("numArgs", &OpenCLKernelInfo::numArgs, "");
    addField("attributes", &OpenCLKernelInfo::attributes, "");
    addField("args", &OpenCLKernelInfo::args, "");
}


/*****************************************************************************/
/* OPENCL PROFILING INFO                                                     */
/*****************************************************************************/

OpenCLProfilingInfo::
OpenCLProfilingInfo(cl_event event)
    : event(event)
{
    doField(CL_PROFILING_COMMAND_QUEUED, queued);
    doField(CL_PROFILING_COMMAND_SUBMIT, submit);
    doField(CL_PROFILING_COMMAND_START, start);
    doField(CL_PROFILING_COMMAND_END, end);
    doField(CL_PROFILING_COMMAND_COMPLETE, complete, OpenCLIgnoreExceptions());
}

template<typename T, typename... Args>
void
OpenCLProfilingInfo::
doField(cl_uint what, T & where, Args&&... args)
{
    clInfoCall(clGetEventProfilingInfo, event, what, where, this,
               std::forward<Args>(args)...);
}

DEFINE_STRUCTURE_DESCRIPTION_INLINE(OpenCLProfilingInfo)
{
    addField("queued", &OpenCLProfilingInfo::queued, "");
    addField("submit", &OpenCLProfilingInfo::submit, "");
    addField("start", &OpenCLProfilingInfo::start, "");
    addField("end", &OpenCLProfilingInfo::end, "");
    addField("complete", &OpenCLProfilingInfo::complete, "");
}

} // namespace MLDB
