#pragma once

#include <vk_types.h>

struct FrameData {
    VkCommandPool   command_pool;
    VkCommandBuffer command_buffer;
    VkSemaphore     render_semaphore;
    VkSemaphore     swapchain_semaphore;
    VkFence         fence;
};

constexpr uint32_t FRAME_OVERLAP = 2;

class VulkanEngine final {
public:
    bool       _isInitialized{false};
    int        _frame_number{0};
    bool       stop_rendering{false};
    VkExtent2D _window_extent{1700, 900};

    struct SDL_Window* _window{nullptr};

    static VulkanEngine& Get();

    //initializes everything in the engine
    void init();

    //shuts down the engine
    void cleanup();

    //draw loop
    void draw();

    //run main loop
    void run();

private:
    VkInstance               _instance{nullptr};
    VkDebugUtilsMessengerEXT _debugMessengerEXT{nullptr};
    VkPhysicalDevice         _physical_device{nullptr};
    VkDevice                 _device{nullptr};
    VkSurfaceKHR             _surface{nullptr};
    VkSwapchainKHR           _swapchain{nullptr};
    VkFormat                 _swapchainImageFormat{VK_FORMAT_UNDEFINED};
    std::vector<VkImage>     _swapchain_images{VK_NULL_HANDLE};
    std::vector<VkImageView> _swapchain_image_views{VK_NULL_HANDLE};
    VkExtent2D               _swapchain_extent{};
    VkQueue                  _present_queue{nullptr};
    uint32_t                 _queue_family_index{INT_MAX};

    FrameData _frame_data[FRAME_OVERLAP] = {};

    void                           init_vulkan();
    void                           create_commands();
    void                           create_swapchain(uint32_t width, uint32_t height);
    void                           destroy_swapchain() const;
    void                           create_sync_structures();
    [[nodiscard]] const FrameData& get_current_frame() const;
};
