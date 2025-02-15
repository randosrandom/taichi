#include "taichi/rhi/vulkan/vulkan_common.h"

#include "taichi/util/lang_util.h"
#include "taichi/rhi/vulkan/vulkan_loader.h"
#include "taichi/common/logging.h"

#if !defined(ANDROID)
#include "GLFW/glfw3.h"
#endif

namespace taichi {
namespace lang {
namespace vulkan {

VulkanLoader::VulkanLoader() {
}

bool VulkanLoader::check_vulkan_device() {
#ifdef __APPLE__
  glfwInitVulkanLoader(vkGetInstanceProcAddr);
#endif

  bool found_device_with_compute = false;

  // We create an temporary Vulkan instance to probe the Vulkan devices.
  // Otherwise, in the case of a CPU only VM with Vulkan installed, Vulkan will
  // not run as there is no GPU available, but the fallback will not happen
  // because Vulkan API is available.

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Checking Vulkan Device";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "No Engine";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;

  VkInstance instance{VK_NULL_HANDLE};
  VkResult res = vkCreateInstance(&create_info, kNoVkAllocCallbacks, &instance);

  do {
    if (res != VK_SUCCESS) {
      TI_WARN("Can not create Vulkan instance");
      break;
    }

    load_instance(instance);

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

    if (device_count == 0) {
      TI_WARN("Can not find Vulkan capable devices");
      break;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    for (int i = 0; i < devices.size(); i++) {
      const auto &physical_device = devices[i];

      uint32_t queue_family_count = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                               &queue_family_count, nullptr);
      if (queue_family_count > 0) {
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, &queue_family_count, queue_families.data());

        for (auto &queue : queue_families) {
          if (queue.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            found_device_with_compute = true;
          }
        }
      }
    }
  } while (false);

  if (instance) {
    vkDestroyInstance(instance, kNoVkAllocCallbacks);
  }

  return found_device_with_compute;
}

bool VulkanLoader::init() {
  std::call_once(init_flag_, [&]() {
    if (initialized) {
      return;
    }
#if defined(__APPLE__)
    vulkan_rt_ = std::make_unique<DynamicLoader>(runtime_lib_dir() +
                                                 "/libMoltenVK.dylib");
    PFN_vkGetInstanceProcAddr get_proc_addr =
        (PFN_vkGetInstanceProcAddr)vulkan_rt_->load_function(
            "vkGetInstanceProcAddr");

    volkInitializeCustom(get_proc_addr);
    initialized = true;
#else
    VkResult result = volkInitialize();
    initialized = result == VK_SUCCESS;
#endif
    initialized = initialized && check_vulkan_device();
  });
  return initialized;
}

void VulkanLoader::load_instance(VkInstance instance) {
  vulkan_instance_ = instance;
  volkLoadInstance(instance);
}
void VulkanLoader::load_device(VkDevice device) {
  vulkan_device_ = device;
  volkLoadDevice(device);
}

PFN_vkVoidFunction VulkanLoader::load_function(const char *name) {
  auto result =
      vkGetInstanceProcAddr(VulkanLoader::instance().vulkan_instance_, name);
  TI_WARN_IF(result == nullptr, "loaded vulkan function {} is nullptr", name);
  return result;
}

bool is_vulkan_api_available() {
  return VulkanLoader::instance().init();
}

void set_vulkan_visible_device(std::string id) {
  VulkanLoader::instance().visible_device_id = id;
}

}  // namespace vulkan
}  // namespace lang
}  // namespace taichi
