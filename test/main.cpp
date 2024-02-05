#include <hip/hip_runtime.h>
#include <hip/device_functions.h>
#include <hip/hip_ext.h>
#include <hip/math_functions.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
#include <string>
#include <numeric>
#include "../Utils/KernelArguments.hpp"
#include "../Utils/Math.hpp"
#include "../Utils/BufferUtils.hpp"

template<typename DType>
void cpuSoftmax(DType *m, DType *a, DType scale, std::uint32_t numRows, std::uint32_t numCols) {
    for (std::uint32_t i = 0; i < numRows * numCols; ++i)
        m[0] = std::max(m[0], std::abs(a[i]));
}

std::uint32_t getNumWorkgroups(std::size_t m, std::size_t n, std::size_t tileM, std::size_t tileN) {
    return ((m / tileM) + !!(m % tileM));
}

hipError_t launchASMKernel(hipFunction_t func, float *dst, float* dstD, float *src, float *scale, std::uint32_t m, std::uint32_t n, bool sync = true) {
    KernelArguments args;
    args.append(dst);
    args.append(dstD);
    args.append(src);
    args.append(scale);
    args.append(m * n);
    args.applyAlignment();
    std::size_t argsSize = args.size();
    void *launchArgs[] = {
        HIP_LAUNCH_PARAM_BUFFER_POINTER,
        args.buffer(),
        HIP_LAUNCH_PARAM_BUFFER_SIZE,
        &argsSize,
        HIP_LAUNCH_PARAM_END
    };

    hipEvent_t beg, end;
    auto err = hipEventCreate(&beg);
    err = hipEventCreate(&end);

    err = hipEventRecord(beg);
    const auto numWorkgroups = getNumWorkgroups(m, n, 1, 256);

    // for (size_t i = 0; i < numRuns; ++i) {
        err = hipExtModuleLaunchKernel(func, numWorkgroups * 256, 1, 1, 256, 1, 1, 256 * sizeof(float), nullptr, nullptr, launchArgs);
    // }

    err = hipEventRecord(end);

    if (sync) {
        err = hipEventSynchronize(end);
        err = hipDeviceSynchronize();
    }

    float dur{};
    err = hipEventElapsedTime(&dur, beg, end);
    // std::cout << "ASM kernel time: " << std::to_string(dur / numRuns) << " ms\n";
    // std::cout << "Perf: " << numRuns * m * n * 2 * sizeof(float) * 1e3 / std::pow(1024.f, 3) / dur << " GB/s\n";
    return err;
}

hipError_t prepareASMKernel(const std::string &funcName, const std::string &coPath, hipModule_t *module, hipFunction_t *func) {
    auto err = hipModuleLoad(module, coPath.c_str());
    err = hipModuleGetFunction(func, *module, funcName.c_str());
    return err;
}

int main(int argc, char **argv) {
    hipDevice_t dev{};
    auto err = hipDeviceGet(&dev, 0);

    const std::string coPath("A_S_S_256_4_gfx942.co");
    const std::uint32_t m(256);
    const std::uint32_t n(4);
    const std::uint32_t numElements = m * n;

    float *gpuMem{};
    float *gpuMemScale{};
    std::vector<float> cpuMem(numElements, 0.8);
    std::vector<float> cpuMemScale(1, 0.5);
    // randomize(begin(cpuMem), end(cpuMem));
    err = hipMalloc(&gpuMem, sizeof(float) * numElements);
    err = hipMalloc(&gpuMemScale, sizeof(float));
    err = hipMemcpyHtoD(gpuMem, cpuMem.data(), cpuMem.size() * sizeof(float));
    err = hipMemcpyHtoD(gpuMemScale, cpuMemScale.data(), sizeof(float) * 1);
    float *hipResult{};
    err = hipMalloc(&hipResult, sizeof(float) * 1);
    err = hipMemset(hipResult, 0, sizeof(float) * 1);
    float *hipResultD{};
    err = hipMalloc(&hipResultD, numElements * sizeof(float));
    err = hipMemset(hipResultD, 0, numElements * sizeof(float));

    hipModule_t module{};
    hipFunction_t func{};
    err = prepareASMKernel("AMax_Ti_S_To_S_W_256_C_4", coPath, &module, &func);
    err = launchASMKernel(func, hipResult, hipResultD, gpuMem, gpuMemScale, m, n);

    std::vector<float> asmResult(1, 0.f);
    err = hipMemcpyDtoH(asmResult.data(), hipResult, 1 * sizeof(float));
    std::vector<float> asmResultD(numElements);
    err = hipMemcpyDtoH(asmResultD.data(), hipResultD, numElements * sizeof(float));

    //for (int i = 0; i < m*n; i++)
    //    std::cout << asmResultD[i] << " ";
    //std::cout << std::endl;
    std::cout << asmResultD[0] << std::endl;

    // for (int i = 0; i < 256; i++) {
    //     for (int j = 0; j < 4; j++)
    //         std::cout << asmResultD[i] << " ";
    //     std::cout << std::endl;
    // }

    // std::vector<float> cpuRef(1, 0.f);
    // cpuSoftmax<float>(cpuRef.data(), cpuMem.data(), scale, m, n);

    // for (std::size_t i = 0; i < 1; ++i) {
    //     if (!almostEqual(asmResult[i], cpuRef[i])) {
    //         std::cout << "ASM kernel vs CPU mismatched!!\n";
    //         std::cout << "Idx: " << i << '\n';
    //         std::cout << asmResult[i] << " vs " << cpuRef[i] << '\n';
    //         std::cout << "Diff: " << std::abs(asmResult[i] - cpuRef[i]) << '\n';
    //         return EXIT_FAILURE;
    //     }
    // }

    err = hipFree(gpuMem);
    err = hipModuleUnload(module);
    return 0;
}
