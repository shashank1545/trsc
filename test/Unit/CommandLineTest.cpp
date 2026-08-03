#include "trsc/Basic/CommandLine.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

bool parse(std::vector<std::string> arguments, trsc::CompilerOptions &options) {
  std::vector<char *> argv;
  argv.reserve(arguments.size());
  for (std::string &argument : arguments)
    argv.push_back(argument.data());
  return trsc::parseCommandLine(static_cast<int>(argv.size()), argv.data(),
                                options);
}

TEST(CommandLineTest, DefaultsToAutomaticSm75Target) {
  trsc::CompilerOptions options;
  ASSERT_TRUE(parse({"trsc", "input.rs"}, options));
  EXPECT_EQ(options.Target.Device, trsc::DeviceMode::Auto);
  EXPECT_EQ(options.Target.CudaArch, "sm_75");
  EXPECT_EQ(options.Target.MinimumCudaCapability, 75);
}

TEST(CommandLineTest, AcceptsExplicitDeviceModes) {
  trsc::CompilerOptions cpu;
  ASSERT_TRUE(parse({"trsc", "--device=cpu", "input.rs"}, cpu));
  EXPECT_EQ(cpu.Target.Device, trsc::DeviceMode::CPU);

  trsc::CompilerOptions cuda;
  ASSERT_TRUE(parse({"trsc", "--device=cuda", "input.rs"}, cuda));
  EXPECT_EQ(cuda.Target.Device, trsc::DeviceMode::CUDA);
}

TEST(CommandLineTest, AcceptsModernCudaArchitecture) {
  trsc::CompilerOptions options;
  ASSERT_TRUE(parse({"trsc", "--cuda-arch=sm_80", "input.rs"}, options));
  EXPECT_EQ(options.Target.CudaArch, "sm_80");
  EXPECT_EQ(options.Target.MinimumCudaCapability, 80);
}

TEST(CommandLineTest, RejectsUnknownDeviceMode) {
  trsc::CompilerOptions options;
  testing::internal::CaptureStderr();
  EXPECT_FALSE(parse({"trsc", "--device=rocm", "input.rs"}, options));
  EXPECT_NE(testing::internal::GetCapturedStderr().find("Invalid --device"),
            std::string::npos);
}

TEST(CommandLineTest, RejectsPreTuringCudaArchitecture) {
  trsc::CompilerOptions options;
  testing::internal::CaptureStderr();
  EXPECT_FALSE(parse({"trsc", "--cuda-arch=sm_70", "input.rs"}, options));
  EXPECT_NE(testing::internal::GetCapturedStderr().find("minimum is sm_75"),
            std::string::npos);
}

TEST(CommandLineTest, RejectsMalformedCudaArchitecture) {
  trsc::CompilerOptions options;
  testing::internal::CaptureStderr();
  EXPECT_FALSE(parse({"trsc", "--cuda-arch=native", "input.rs"}, options));
  EXPECT_NE(testing::internal::GetCapturedStderr().find("expected sm_NN"),
            std::string::npos);
}

} // namespace
