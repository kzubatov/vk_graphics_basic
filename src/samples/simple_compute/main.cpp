#include "simple_compute.h"
#include <vector>
#include <chrono>
#include <random>
#include <ratio>
#include <iostream>

int main()
{
  constexpr int LENGTH = (1024 * 64 - 1) * 32;
  constexpr int VULKAN_DEVICE_ID = 0;

  std::shared_ptr<ICompute> app = std::make_unique<SimpleCompute>(LENGTH);
  if(app == nullptr)
  {
    std::cout << "Can't create render of specified type" << std::endl;
    return 1;
  }

  app->InitVulkan(nullptr, 0, VULKAN_DEVICE_ID);
  app->Execute();

  return 0;
}
