#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>

#include "VkBootstrap.h"
#include <chrono>
#include <iostream>
#include <thread>

#include "vk_images.h"
#include "vk_initializers.h"

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

void VulkanEngine::init() {
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    auto window_flags = SDL_WINDOW_VULKAN;

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _window_extent.width,
        _window_extent.height,
        window_flags);
    init_vulkan();

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::cleanup() {
    if (_isInitialized) {
        vkDeviceWaitIdle(_device);

        destroy_swapchain();

        for (const auto frame_data : _frame_data) {
            vkDestroyFence(_device, frame_data.fence, nullptr);
            vkDestroySemaphore(_device, frame_data.render_semaphore, nullptr);
            vkDestroySemaphore(_device, frame_data.swapchain_semaphore, nullptr);
            vkDestroyCommandPool(_device, frame_data.command_pool, nullptr);
        }

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);
        vkb::destroy_debug_utils_messenger(_instance, _debugMessengerEXT, nullptr);
        vkDestroyInstance(_instance, nullptr);

        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw() {
    const auto& current_frame_data = get_current_frame();

    VK_CHECK(vkWaitForFences(_device, 1, &current_frame_data.fence,true, 1000000000));
    VK_CHECK(vkResetFences(_device, 1, &current_frame_data.fence));

    uint32_t swapchain_image_index = UINT32_MAX;
    VK_CHECK(
        vkAcquireNextImageKHR(_device,
            _swapchain,
            1000000000,
            current_frame_data.swapchain_semaphore,
            nullptr,
            &swapchain_image_index));

    auto command_buffer = current_frame_data.command_buffer;
    VK_CHECK(vkResetCommandBuffer(command_buffer, 0));

    auto command_buffer_begin_info = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));

    auto swapchain_image = _swapchain_images[swapchain_image_index];
    vkutil::transition_image(command_buffer, swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    VkClearColorValue clear_color_value = {};
    float             flash             = std::abs(std::sin(static_cast<float>(_frame_number) / 120.f));
    clear_color_value                   = {{0.0f, 0.0f, flash, 1.0f}};

    VkImageSubresourceRange clear_range = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdClearColorImage(command_buffer,
                         swapchain_image,
                         VK_IMAGE_LAYOUT_GENERAL,
                         &clear_color_value,
                         1,
                         &clear_range);

    vkutil::transition_image(command_buffer,
                             swapchain_image,
                             VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(command_buffer));

    VkCommandBufferSubmitInfo command_buffer_submit_info = vkinit::command_buffer_submit_info(command_buffer);

    VkSemaphoreSubmitInfo semaphore_signal_submit_info = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        current_frame_data.render_semaphore);
    VkSemaphoreSubmitInfo semaphore_wait_submit_info = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        current_frame_data.swapchain_semaphore);

    VkSubmitInfo2 submit_info = vkinit::submit_info(&command_buffer_submit_info,
                                                    &semaphore_signal_submit_info,
                                                    &semaphore_wait_submit_info);

    VK_CHECK(vkQueueSubmit2(_present_queue, 1, &submit_info, current_frame_data.fence));

    VkPresentInfoKHR present_info   = {};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext              = nullptr;
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &_swapchain;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &current_frame_data.render_semaphore;
    present_info.pImageIndices      = &swapchain_image_index;

    VK_CHECK(vkQueuePresentKHR(_present_queue, &present_info));

    _frame_number++;
}

void VulkanEngine::run() {
    SDL_Event e;
    bool      bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}

void VulkanEngine::init_vulkan() {
    vkb::InstanceBuilder instance_builder;

    auto instance_return = instance_builder
                           .set_app_name("Vulkan Engine")
                           .set_engine_name("Vulkan Engine")
                           .require_api_version(1, 3, 0)
                           .request_validation_layers(true)
                           .use_default_debug_messenger()
                           .build();

    vkb::Instance vkb_instance = instance_return.value();

    _instance          = vkb_instance.instance;
    _debugMessengerEXT = vkb_instance.debug_messenger;

    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    VkPhysicalDeviceVulkan13Features device_features13{};
    device_features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    device_features13.synchronization2 = true;
    device_features13.dynamicRendering = true;

    VkPhysicalDeviceVulkan12Features device_features12{};
    device_features12.sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    device_features12.bufferDeviceAddress = true;
    device_features12.descriptorIndexing  = true;

    vkb::PhysicalDeviceSelector selector{vkb_instance};
    vkb::PhysicalDevice         vkb_physical_device = selector
                                              .set_minimum_version(1, 3)
                                              .set_required_features_13(device_features13)
                                              .set_required_features_12(device_features12)
                                              .set_surface(_surface)
                                              .select()
                                              .value();

    vkb::DeviceBuilder deviceBuilder{vkb_physical_device};
    vkb::Device        vkb_device = deviceBuilder.build().value();

    _physical_device = vkb_device.physical_device;
    _device          = vkb_device.device;

    create_swapchain(_window_extent.width, _window_extent.height);

    _present_queue      = vkb_device.get_queue(vkb::QueueType::graphics).value();
    _queue_family_index = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    create_commands();
    create_sync_structures();
}

void VulkanEngine::create_commands() {
    VkCommandPoolCreateInfo command_pool_ci{};
    command_pool_ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_ci.pNext            = nullptr;
    command_pool_ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_ci.queueFamilyIndex = _queue_family_index;

    for (auto& current_frame_data : _frame_data) {
        VkCommandPool command_pool;
        VK_CHECK(vkCreateCommandPool(_device, &command_pool_ci, nullptr, &command_pool));
        current_frame_data.command_pool = command_pool;

        VkCommandBufferAllocateInfo command_buffer_allocate_info{};
        command_buffer_allocate_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.pNext              = nullptr;
        command_buffer_allocate_info.commandBufferCount = 1;
        command_buffer_allocate_info.commandPool        = command_pool;
        command_buffer_allocate_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VkCommandBuffer command_buffer;
        VK_CHECK(vkAllocateCommandBuffers(_device, &command_buffer_allocate_info, &command_buffer));

        current_frame_data.command_buffer = command_buffer;
    }
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{_physical_device, _device, _surface};
    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;

    auto surface_format = VkSurfaceFormatKHR{
        .format = _swapchainImageFormat,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    };
    vkb::Swapchain swapchain = swapchainBuilder.set_desired_format(surface_format)
                                               .set_desired_extent(width, height)
                                               .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                               .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                               .set_required_min_image_count(2)
                                               .build()
                                               .value();

    _swapchain_extent      = swapchain.extent;
    _swapchain             = swapchain.swapchain;
    _swapchain_images      = swapchain.get_images().value();
    _swapchain_image_views = swapchain.get_image_views().value();
}

void VulkanEngine::destroy_swapchain() const {
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    for (const auto swapchain_image_view : _swapchain_image_views) {
        vkDestroyImageView(_device, swapchain_image_view, nullptr);
    }
}

void VulkanEngine::create_sync_structures() {
    auto fence_ci     = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    auto semaphore_ci = vkinit::semaphore_create_info();

    for (auto& current_frame_data : _frame_data) {
        VkFence     fence;
        VkSemaphore render_semaphore;
        VkSemaphore swapchain_semaphore;

        VK_CHECK(vkCreateFence(_device, &fence_ci, nullptr, &fence));
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_ci, nullptr, &render_semaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_ci, nullptr, &swapchain_semaphore));

        current_frame_data.fence               = fence;
        current_frame_data.render_semaphore    = render_semaphore;
        current_frame_data.swapchain_semaphore = swapchain_semaphore;
    }
}

const FrameData& VulkanEngine::get_current_frame() const {
    return _frame_data[_frame_number % FRAME_OVERLAP];
}
