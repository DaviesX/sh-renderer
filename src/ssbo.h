#pragma once

#include <cstddef>
#include <cstdint>

namespace sh_renderer {

// TODO: Document the interface. Adjust the interface if needed.

struct SSBO {
  uint32_t id = 0;
  size_t size = 0;
};

SSBO CreateSSBO(const void *data, size_t size);

void DestroySSBO(SSBO ssbo);

void UpdateSSBO(SSBO ssbo, const void *data, size_t size);

void BindSSBO(SSBO ssbo, uint32_t bind_point);

}  // namespace sh_renderer
