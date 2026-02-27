#include "ssbo.h"

#include "glad.h"

namespace sh_renderer {

SSBO CreateSSBO(const void *data, size_t size) {
  GLuint id = 0;
  glCreateBuffers(1, &id);
  glNamedBufferStorage(id, size, data, GL_DYNAMIC_STORAGE_BIT);
  return SSBO{.id = id, .size = size};
}

void DestroySSBO(SSBO ssbo) {
  if (ssbo.id != 0) {
    glDeleteBuffers(1, &ssbo.id);
  }
}

void UpdateSSBO(SSBO ssbo, const void *data, size_t size) {
  if (ssbo.id == 0 || size == 0) return;
  glNamedBufferSubData(ssbo.id, 0, size, data);
}

void BindSSBO(SSBO ssbo, uint32_t bind_point) {
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bind_point, ssbo.id);
}

}  // namespace sh_renderer
