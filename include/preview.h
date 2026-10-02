#ifndef CHITON_PREVIEW_H
#define CHITON_PREVIEW_H
#include "display.h"
#include "image.h"
namespace display {
using Samples = std::array<uint16_t, width * 190>;
class Preview {
    Buffer buffer{};
    std::array<uint16_t, 4096> colors{};
    std::unique_ptr<ChitonPngImage> font;
    std::unique_ptr<FontChar[]> offsets;
    void text(const std::string &msg, int x, int y, uint16_t color);
public:
    Preview();
    const Buffer &render(const Samples *samples, const std::string &ethernet,
                         const std::string &wifi, const std::string &message,
                         unsigned long seconds);
    const Buffer &startup_image(const ChitonPngImage &image);
};
}
#endif
