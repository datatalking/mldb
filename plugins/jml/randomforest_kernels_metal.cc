/** randomforest_kernels_metal.cc                              -*- C++ -*-
    Jeremy Barnes, 8 September 2021
    Copyright (c) 2021 Jeremy Barnes.  All rights reserved.
    This file is part of MLDB. Copyright 2016 mldb.ai inc. All rights reserved.

    Kernels for random forest algorithm.
*/

#include "randomforest_kernels_metal.h"
#include "randomforest_kernels.h"
#include "mldb/utils/environment.h"
#include "mldb/vfs/filter_streams.h"
#include "mldb/builtin/metal/compute_kernel_metal.h"
#include <future>
#include <array>


using namespace std;
using namespace mtlpp;


namespace MLDB {
namespace RF {

EnvOption<bool> DEBUG_RF_METAL_KERNELS("DEBUG_RF_METAL_KERNELS", 0);

// Default of 5.5k allows 8 parallel workgroups for a 48k SM when accounting
// for 0.5k of local memory for the kernels.
// On Nvidia, with 32 registers/work item and 256 work items/workgroup
// (8 warps of 32 threads), we use 32 * 256 * 8 = 64k registers, which
// means full occupancy.
EnvOption<int, true> RF_METAL_LOCAL_BUCKET_MEM("RF_METAL_LOCAL_BUCKET_MEM", 5500);


namespace {

static struct RegisterKernels {

    RegisterKernels()
    {
        auto getLibrary = [] (MetalComputeContext & context) -> mtlpp::Library
        {
            auto compileLibrary = [&] () -> mtlpp::Library
            {
                Library library;
                ns::Error error{ns::Handle()};

                if (false) {
                    std::string fileName = "mldb/plugins/jml/randomforest_kernels.metal";
                    filter_istream stream(fileName);
                    Utf8String source = /*"#line 1 \"" + fileName + "\"\n" +*/ stream.readAll();

                    CompileOptions compileOptions;
                    library = context.mtlDevice.NewLibrary(source.rawData(), compileOptions, &error);
                }
                else {
                    std::string fileName = "build/arm64/lib/randomforest_metal.metallib";
                    library = context.mtlDevice.NewLibrary(fileName.c_str(), &error);
                }

                if (error) {
                    cerr << "Error getting library" << endl;
                    cerr << "domain: " << error.GetDomain().GetCStr() << endl;
                    cerr << "description: " << error.GetLocalizedDescription().GetCStr() << endl;
                    if (error.GetLocalizedFailureReason()) {
                        cerr << "reason: " << error.GetLocalizedFailureReason().GetCStr() << endl;
                    }
                }

                ExcAssert(library);

                return library;
            };

            static const std::string cacheKey = "randomforest_kernels";
            Library library = context.getCacheEntry(cacheKey, compileLibrary);
            return library;
        };
    
        auto createDecodeRowsKernel = [getLibrary] (MetalComputeContext& context) -> std::shared_ptr<MetalComputeKernel>
        {
            auto library = getLibrary(context);
            auto result = std::make_shared<MetalComputeKernel>(&context);
            result->kernelName = "decodeRows";
            result->addDimension("r", "nr", 256);
            result->allowGridPadding();
            //result->device = context.devices[0]; // TODO deviceS
            result->addParameter("rowData", "r", "u64[rowDataLength]");
            result->addParameter("rowDataLength", "r", "u32");
            result->addParameter("weightBits", "r", "u16");
            result->addParameter("exampleNumBits", "r", "u16");
            result->addParameter("numRows", "r", "u32");
            result->addParameter("weightFormat", "r", "WeightFormat");
            result->addParameter("weightMultiplier", "r", "f32");
            result->addParameter("weightData", "r", "f32[weightDataLength]");
            result->addParameter("decodedRowsOut", "w", "f32[numRows]");
            result->addTuneable("threadsPerBlock", 256);
            result->addTuneable("blocksPerGrid", 16);
            result->setGridExpression("[blocksPerGrid]", "[threadsPerBlock]");
            result->setComputeFunction(library, "decompressRowsKernel");

            return result;
        };

        registerMetalComputeKernel("decodeRows", createDecodeRowsKernel);

        auto createTestFeatureKernel = [getLibrary] (MetalComputeContext& context) -> std::shared_ptr<MetalComputeKernel>
        {
            auto library = getLibrary(context);
            auto result = std::make_shared<MetalComputeKernel>(&context);
            result->kernelName = "testFeature";
            //result->device = ComputeDevice::host();
            result->addDimension("fidx", "naf");
            result->addDimension("rowNum", "numRows");
            
            result->addParameter("decodedRows", "r", "f32[numRows]");
            result->addParameter("numRows", "r", "u32");
            result->addParameter("bucketData", "r", "u32[bucketDataLength]");
            result->addParameter("bucketDataOffsets", "r", "u32[nf + 1]");
            result->addParameter("bucketNumbers", "r", "u32[nf + 1]");
            result->addParameter("bucketEntryBits", "r", "u32[nf]");
            result->addParameter("activeFeatureList", "r", "u32[naf]");
            result->addParameter("partitionBuckets", "rw", "W32[numBuckets]");

            result->addTuneable("maxLocalBuckets", RF_METAL_LOCAL_BUCKET_MEM.get() / sizeof(W));
            result->addTuneable("threadsPerBlock", 1024);
            result->addTuneable("blocksPerGrid", 32);

            result->addParameter("w", "w", "W[maxLocalBuckets]");
            result->addParameter("maxLocalBuckets", "r", "u32");

            result->setGridExpression("[naf,blocksPerGrid]", "[1,threadsPerBlock]");
            result->allowGridPadding();

            result->setComputeFunction(library, "testFeatureKernel");
            return result;
        };

        registerMetalComputeKernel("testFeature", createTestFeatureKernel);

        auto createGetPartitionSplitsKernel = [getLibrary] (MetalComputeContext& context) -> std::shared_ptr<MetalComputeKernel>
        {
            auto library = getLibrary(context);
            auto result = std::make_shared<MetalComputeKernel>(&context);
            result->kernelName = "getPartitionSplits";
            //result->device = ComputeDevice::host();
            result->addDimension("fidx", "naf");

            result->addParameter("treeTrainingInfo", "r", "TreeTrainingInfo[1]");
            result->addParameter("bucketNumbers", "r", "u32[nf + 1]");
            result->addParameter("activeFeatureList", "r", "u32[naf]");
            result->addParameter("featureIsOrdinal", "r", "u32[nf]");
            result->addParameter("buckets", "r", "W32[numActiveBuckets * nap]");
            result->addParameter("wAll", "r", "W32[nap]");
            result->addParameter("featurePartitionSplitsOut", "w", "PartitionSplit[nap * naf]");
            result->addParameter("treeDepthInfo", "r", "TreeDepthInfo[1]");

            result->addTuneable("numPartitionsInParallel", 1024);
            result->addTuneable("wLocalSize", RF_METAL_LOCAL_BUCKET_MEM.get() / sizeof(WIndexed));

            result->addParameter("wLocal", "w", "WIndexed[wLocalSize]");
            result->addParameter("wLocalSize", "r", "u32");
            //result->addPostConstraint("totalBuckets", "==", "readArrayElement(numActivePartitionsOut, 0)");
            //result->addPreConstraint("nap", "==", "maxNumActivePartitions");
            //result->addPostConstraint("nap", "==", "readArrayElement(treeDepthInfo, 0).numActivePartitions");

            result->setGridExpression("[1,naf,numPartitionsInParallel]", "[64,1,1]");
            
            result->setComputeFunction(library, "getPartitionSplitsKernel");
            return result;
        };

        registerMetalComputeKernel("getPartitionSplits", createGetPartitionSplitsKernel);

        auto createBestPartitionSplitKernel = [getLibrary] (MetalComputeContext & context) -> std::shared_ptr<MetalComputeKernel>
        {
            auto library = getLibrary(context);
            auto result = std::make_shared<MetalComputeKernel>(&context);
            result->kernelName = "bestPartitionSplit";
            //result->device = ComputeDevice::host();

            result->addParameter("treeTrainingInfo", "r", "TreeTrainingInfo[1]");
            result->addParameter("treeDepthInfo", "r", "TreeDepthInfo[1]");

            result->addParameter("activeFeatureList", "r", "u32[numActiveFeatures]");
            result->addParameter("featurePartitionSplits", "r", "PartitionSplit[numActivePartitions * numActiveFeatures]");
            result->addParameter("partitionIndexes", "r", "PartitionIndex[npi]");
            result->addParameter("allPartitionSplitsOut", "w", "IndexedPartitionSplit[maxPartitions]");

            //result->addPreConstraint("numActivePartitions", "==", "readArrayElement(treeDepthInfo, 0).numActivePartitions");
            //result->addPostConstraint("numActivePartitions", "==", "readArrayElement(treeDepthInfo, 0).numActivePartitions");

            result->addTuneable("numPartitionsAtOnce", 1024);
            result->setGridExpression("[numPartitionsAtOnce]", "[1]");
            result->setComputeFunction(library, "bestPartitionSplitKernel");
            return result;
        };

        registerMetalComputeKernel("bestPartitionSplit", createBestPartitionSplitKernel);

        auto createAssignPartitionNumbersKernel = [getLibrary] (MetalComputeContext & context) -> std::shared_ptr<MetalComputeKernel>
        {
            auto library = getLibrary(context);
            auto result = std::make_shared<MetalComputeKernel>(&context);
            result->kernelName = "assignPartitionNumbers";
            //result->device = ComputeDevice::host();
            result->addParameter("treeTrainingInfo", "r", "TreeTrainingInfo[1]");
            result->addParameter("treeDepthInfo", "r", "TreeDepthInfo[1]");

            result->addParameter("allPartitionSplits", "r", "IndexedPartitionSplit[np]");
            result->addParameter("partitionIndexesOut", "w", "PartitionIndex[maxActivePartitions]");
            result->addParameter("partitionInfoOut", "w", "PartitionInfo[numActivePartitions]");
            result->addParameter("smallSideIndexesOut", "w", "u8[maxActivePartitions]");
            result->addParameter("smallSideIndexToPartitionOut", "w", "u16[256]");
            result->setGridExpression("[1]", "[32]");
            result->setComputeFunction(library, "assignPartitionNumbersKernel");
            return result;
        };

        registerMetalComputeKernel("assignPartitionNumbers", createAssignPartitionNumbersKernel);

        auto createClearBucketsKernel = [getLibrary] (MetalComputeContext & context) -> std::shared_ptr<MetalComputeKernel>
        {
            auto library = getLibrary(context);
            auto result = std::make_shared<MetalComputeKernel>(&context);
            result->kernelName = "clearBuckets";
            //result->device = ComputeDevice::host();
            result->addDimension("bucket", "numActiveBuckets");
            result->addParameter("treeTrainingInfo", "r", "TreeTrainingInfo[1]");
            result->addParameter("treeDepthInfo", "r", "TreeDepthInfo[1]");
            result->addParameter("bucketsOut", "w", "W32[numActiveBuckets * numActivePartitions]");
            result->addParameter("wAllOut", "w", "W32[numActivePartitions]");
            result->addParameter("numNonZeroDirectionIndices", "w", "u32[1]");
            result->addParameter("smallSideIndexes", "r", "u8[numActivePartitions]");
            result->allowGridPadding();
            result->addTuneable("gridBlockSize", 64);
            result->addTuneable("numPartitionsAtOnce", 1024);
            result->setGridExpression("[numPartitionsAtOnce,ceilDiv(numActiveBuckets,gridBlockSize)]", "[1,gridBlockSize]");
            result->setComputeFunction(library, "clearBucketsKernel");
            return result;
        };

        registerMetalComputeKernel("clearBuckets", createClearBucketsKernel);

        auto createUpdatePartitionNumbersKernel = [getLibrary] (MetalComputeContext & context) -> std::shared_ptr<MetalComputeKernel>
        {
            auto library = getLibrary(context);
            auto result = std::make_shared<MetalComputeKernel>(&context);
            result->kernelName = "updatePartitionNumbers";
            //result->device = ComputeDevice::host();
            result->addDimension("r", "numRows");

            result->addParameter("treeTrainingInfo", "r", "TreeTrainingInfo[1]");
            result->addParameter("treeDepthInfo", "r", "TreeDepthInfo[1]");

            result->addParameter("partitions", "r", "RowPartitionInfo[numRows]");
            result->addParameter("directions", "w", "u32[(numRows+31)/32]");
            result->addParameter("numNonZeroDirectionIndices", "rw", "u32[1]");
            result->addParameter("nonZeroDirectionIndices", "w", "UpdateWorkEntry[numRows / 2 + 2]");
            result->addParameter("smallSideIndexes", "r", "u8[numActivePartitions]");
            result->addParameter("allPartitionSplits", "r", "IndexedPartitionSplit[naps]");
            result->addParameter("partitionInfo", "r", "PartitionInfo[np]");
            result->addParameter("bucketData", "r", "u32[bucketDataLength]");
            result->addParameter("bucketDataOffsets", "r", "u32[nf + 1]");
            result->addParameter("bucketNumbers", "r", "u32[nf + 1]");
            result->addParameter("bucketEntryBits", "r", "u32[nf]");
            result->addParameter("featureIsOrdinal", "r", "u32[nf]");
            result->addParameter("decodedRows", "r", "f32[numRows]");
            result->addTuneable("threadsPerBlock", 1024);
            result->addTuneable("blocksPerGrid", 96);
            result->allowGridPadding();
            result->setGridExpression("[blocksPerGrid]", "[threadsPerBlock]");
            result->setComputeFunction(library, "updatePartitionNumbersKernel");
            return result;
        };

        registerMetalComputeKernel("updatePartitionNumbers", createUpdatePartitionNumbersKernel);

        auto createUpdateBucketsKernel = [getLibrary] (MetalComputeContext & context) -> std::shared_ptr<MetalComputeKernel>
        {
            auto library = getLibrary(context);
            auto result = std::make_shared<MetalComputeKernel>(&context);
            result->kernelName = "updateBuckets";
            result->device = ComputeDevice::host();
            result->addDimension("r", "numRows");
            result->addDimension("fidx_plus_1", "naf_plus_1");

            result->addParameter("treeTrainingInfo", "r", "TreeTrainingInfo[1]");
            result->addParameter("treeDepthInfo", "r", "TreeDepthInfo[1]");

            result->addParameter("partitions", "r", "RowPartitionInfo[numRows]");
            result->addParameter("directions", "r", "u32[(numRows + 31)/32]");
            result->addParameter("numNonZeroDirectionIndices", "r", "u32[1]");
            result->addParameter("nonZeroDirectionIndices", "r", "UpdateWorkEntry[numRows / 2 + 2]");
            result->addParameter("buckets", "w", "W32[numActiveBuckets * numActivePartitions]");
            result->addParameter("wAll", "w", "W32[numActivePartitions]");
            result->addParameter("smallSideIndexes", "r", "u8[numActivePartitions]");
            result->addParameter("smallSideIndexToPartition", "r", "u16[256]");
            result->addParameter("decodedRows", "r", "f32[nr]");
            result->addParameter("bucketData", "r", "u32[bucketDataLength]");
            result->addParameter("bucketDataOffsets", "r", "u32[nf + 1]");
            result->addParameter("bucketNumbers", "r", "u32[nf + 1]");
            result->addParameter("bucketEntryBits", "r", "u32[nf]");
            result->addParameter("activeFeatureList", "r", "u32[numActiveFeatures]");
            result->addParameter("featureIsOrdinal", "r", "u32[nf]");
            result->addTuneable("maxLocalBuckets", RF_METAL_LOCAL_BUCKET_MEM.get() / sizeof(W));
            result->addTuneable("threadsPerBlock", 1024);
            result->addTuneable("blocksPerGrid", 32);
            result->addParameter("wLocal", "w", "W[maxLocalBuckets]");
            result->addParameter("maxLocalBuckets", "r", "u32");
            result->addConstraint("naf_plus_1", "==", "numActiveFeatures + 1", "help the solver");
            result->addConstraint("numActiveFeatures", "==", "naf_plus_1 - 1", "help the solver");
            result->setGridExpression("[blocksPerGrid,numActiveFeatures+1]", "[threadsPerBlock,1]");
            result->allowGridPadding();
            result->setComputeFunction(library, "updateBucketsKernel");
            return result;
        };

        registerMetalComputeKernel("updateBuckets", createUpdateBucketsKernel);

        auto createFixupBucketsKernel = [getLibrary] (MetalComputeContext & context) -> std::shared_ptr<MetalComputeKernel>
        {
            auto library = getLibrary(context);
            auto result = std::make_shared<MetalComputeKernel>(&context);
            result->kernelName = "fixupBuckets";
            result->device = ComputeDevice::host();
            result->addDimension("bucket", "numActiveBuckets");

            result->addParameter("treeTrainingInfo", "r", "TreeTrainingInfo[=1]");
            result->addParameter("treeDepthInfo", "r", "TreeDepthInfo[=1]");

            result->addParameter("buckets", "rw", "W32[numActiveBuckets * newNumPartitions]");
            result->addParameter("wAll", "rw", "W32[newNumPartitions]");
            result->addParameter("partitionInfo", "r", "PartitionInfo[np]");
            result->addParameter("smallSideIndexes", "r", "u8[newNumPartitions]");
            result->addTuneable("gridBlockSize", 64);
            result->addTuneable("numPartitionsAtOnce", 1024);
            result->allowGridPadding();
            result->setGridExpression("[numPartitionsAtOnce,ceilDiv(numActiveBuckets,gridBlockSize)]", "[1,gridBlockSize]");
            result->setComputeFunction(library, "fixupBucketsKernel");
            return result;
        };

        registerMetalComputeKernel("fixupBuckets", createFixupBucketsKernel);
    }

} registerKernels;
} // file scope

} // namespace RF
} // namespace MLDB