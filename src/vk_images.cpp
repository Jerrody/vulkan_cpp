#include <vk_images.h>

#include "vk_initializers.h"

void vkutil::transition_image(VkCommandBuffer command_buffer,
                              VkImage         image,
                              VkImageLayout   src_image_layout,
                              VkImageLayout   dst_image_layout) {
    VkImageMemoryBarrier2 image_memory_barrier = {};
    image_memory_barrier.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    image_memory_barrier.pNext                 = nullptr;

    image_memory_barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_memory_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    image_memory_barrier.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_memory_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    image_memory_barrier.oldLayout = src_image_layout;
    image_memory_barrier.newLayout = dst_image_layout;

    VkImageAspectFlags image_aspect_flags;
    if (dst_image_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
        image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        image_aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    image_memory_barrier.subresourceRange = vkinit::image_subresource_range(image_aspect_flags);
    image_memory_barrier.image            = image;

    VkDependencyInfo dependency_info        = {};
    dependency_info.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.pNext                   = nullptr;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers    = &image_memory_barrier;

    vkCmdPipelineBarrier2(command_buffer, &dependency_info);
}
