#ifndef TRSC_BASIC_TARGETOPTIONS_H
#define TRSC_BASIC_TARGETOPTIONS_H

#include <string>

namespace trsc {

enum class DeviceMode { Auto, CPU, CUDA };

struct TargetOptions {
  DeviceMode Device = DeviceMode::Auto;
  std::string CudaArch = "sm_75";
  int MinimumCudaCapability = 75;
};

} // namespace trsc

#endif // TRSC_BASIC_TARGETOPTIONS_H
