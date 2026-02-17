
#include "window.h"

#include <gtest/gtest.h>

namespace sh_renderer {
namespace {

TEST(WindowTest, CreateAndDestroy) {
  // Try creating a window with default settings.
  auto window = CreateWindow(800, 600, "Test Window");
  ASSERT_TRUE(window.has_value());
  EXPECT_TRUE(*window != nullptr);

  // Destroy it.
  DestroyWindow(*window);
}

TEST(WindowTest, CreateWithMSAA) {
  // Try creating a window with MSAA.
  auto window = CreateWindow(800, 600, "Test Window MSAA", 4);
  ASSERT_TRUE(window.has_value());
  EXPECT_TRUE(*window != nullptr);

  DestroyWindow(*window);
}

}  // namespace
}  // namespace sh_renderer
