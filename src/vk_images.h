
#pragma once

namespace vkutil {
void transition_image(VkCommandBuffer command_buffer,
                      VkImage         image,
                      VkImageLayout   src_image_layout,
                      VkImageLayout   dst_image_layout);
};
