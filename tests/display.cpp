#include "display.h"
#include "preview.h"
#include "resources.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <unistd.h>
#ifdef CHITON_SDL2
#include <SDL.h>
#endif
#define CHECK(x) do { if (!(x)) { std::cerr << "CHECK failed: " #x << " at " << __LINE__ << '\n'; std::abort(); } } while (0)
template<class F> void rejects(F fn) {
    bool rejected = false;
    try { fn(); } catch (const std::exception &) { rejected = true; }
    CHECK(rejected);
}
int main() {
    using namespace display;
    CHECK(parse_mode("auto") == Mode::Auto);
    rejects([] { parse_mode("bad"); });
    rejects([] { create(Mode::Framebuffer, "/nonexistent-chiton-fb"); });
    // Present-but-broken devices must never be hidden by auto fallback.
    rejects([] { create(Mode::Auto, "/dev/null"); });
    unsetenv("DISPLAY"); unsetenv("WAYLAND_DISPLAY"); unsetenv("SDL_VIDEODRIVER");
    CHECK(!create(Mode::Auto, "/nonexistent-chiton-fb")->visible());
    rejects([] { create(Mode::Desktop); });
    CHECK(!create(Mode::Headless)->visible());

    Buffer pixels{};
    pixels.fill(0xffff);
    fb_var_screeninfo v{};
    fb_fix_screeninfo f{};
    v.xres = width; v.yres = height; v.xoffset = 2; v.yoffset = 1;
    v.xres_virtual = width + 2; v.yres_virtual = height + 1;
    v.bits_per_pixel = 16;
    v.red = {11,5,0}; v.green = {5,6,0}; v.blue = {0,5,0};
    f.type = FB_TYPE_PACKED_PIXELS; f.visual = FB_VISUAL_TRUECOLOR;
    f.line_length = (width + 4) * 2;
    f.smem_len = f.line_length * (height + 1);
    std::vector<uint8_t> mapping(f.smem_len + 16, 0x55);
    copy_framebuffer(pixels, mapping.data(), v, f);
    CHECK(mapping[0] == 0x55);
    CHECK(mapping[f.line_length + 3] == 0x55);
    CHECK(mapping[f.line_length + 4] == 0xff);
    CHECK(mapping[f.line_length + 4 + width * 2] == 0x55);
    CHECK(mapping[f.smem_len] == 0x55);
    auto bad = f; bad.smem_len = 20;
    rejects([&] { copy_framebuffer(pixels, mapping.data(), v, bad); });
    bad = f; bad.line_length = 320;
    rejects([&] { validate_framebuffer(v, bad); });
    auto invalid = v; invalid.blue.offset = 11;
    rejects([&] { validate_framebuffer(invalid, f); });
    invalid = v; invalid.xoffset = UINT32_MAX;
    rejects([&] { validate_framebuffer(invalid, f); });

    // flexfb's nonstd=1 is accepted only for standard RGB565.
    auto flexfb = v; flexfb.nonstd = 1;
    mapping.assign(f.smem_len + 16, 0x55);
    copy_framebuffer(pixels, mapping.data(), flexfb, f);
    CHECK(mapping[f.line_length + 4] == 0xff);
    CHECK(mapping[f.line_length + 4 + width * 2] == 0x55);
    CHECK(mapping[f.smem_len] == 0x55);
    invalid = flexfb; invalid.nonstd = 2;
    rejects([&] { validate_framebuffer(invalid, f); });
    invalid = flexfb; invalid.red.offset = 0; invalid.blue.offset = 11;
    rejects([&] { validate_framebuffer(invalid, f); });
    invalid = flexfb; invalid.grayscale = 1;
    rejects([&] { validate_framebuffer(invalid, f); });
    invalid = flexfb; invalid.red.msb_right = 1;
    rejects([&] { validate_framebuffer(invalid, f); });
    rejects([&] { copy_framebuffer(pixels, mapping.data(), flexfb, bad); });

    v.bits_per_pixel = 32;
    v.red = {16,8,0}; v.green = {8,8,0}; v.blue = {0,8,0}; v.transp = {24,8,0};
    f.line_length = (width + 4) * 4; f.smem_len = f.line_length * (height + 1);
    mapping.assign(f.smem_len + 16, 0x55);
    pixels.fill(0xf800);
    copy_framebuffer(pixels, mapping.data(), v, f);
    uint32_t value;
    memcpy(&value, mapping.data() + f.line_length + 8, sizeof(value));
    CHECK(value == 0xffff0000);
    CHECK(mapping[f.smem_len] == 0x55);
    invalid = v; invalid.nonstd = 1;
    rejects([&] { validate_framebuffer(invalid, f); });

    setenv("CHITON_ASSET_DIR", "/nonexistent-chiton-assets", 1);
    CHECK(resources::asset("small_font.png").empty());
    Preview preview;
    Samples samples{};
    samples[0] = 0; samples[1] = 4095; samples[2] = 65535;
    const auto &image = preview.render(&samples, std::string(500, 'X'), "wifi", "status\xff", 61);
    CHECK(image[0] == 0); CHECK(image[1] == 0xffff); CHECK(image[2] == 0xffff);
    CHECK(image[190 * width] == 0);
    CHECK(!read_png_file("/nonexistent-chiton.png", true));
    char temp_dir[] = "/tmp/chiton-display-XXXXXX";
    CHECK(mkdtemp(temp_dir));
    setenv("CHITON_RESOURCE_DIR", temp_dir, 1);
    CHECK(resources::find("files/app.html").empty()); // Never fall back to another release.
    const std::string resource_path = std::string(temp_dir) + "/bundle-only.txt";
    { std::ofstream resource(resource_path); resource << "versioned resource"; }
    CHECK(resources::find("bundle-only.txt") == resource_path);
    unlink(resource_path.c_str());
    unsetenv("CHITON_RESOURCE_DIR");
    std::string atlas_path = std::string(temp_dir) + "/small_font.png";
    {
        std::ofstream corrupt(atlas_path);
        corrupt << "not a PNG";
    }
    CHECK(!read_png_file(atlas_path.c_str(), true));
    png_image atlas{};
    atlas.version = PNG_IMAGE_VERSION;
    atlas.width = 180; atlas.height = 40; atlas.format = PNG_FORMAT_RGBA;
    std::vector<uint8_t> atlas_pixels(PNG_IMAGE_SIZE(atlas), 255);
    CHECK(png_image_write_to_file(&atlas, atlas_path.c_str(), false, atlas_pixels.data(), 0, nullptr));
    setenv("CHITON_ASSET_DIR", temp_dir, 1);
    {
        Preview atlas_preview;
        atlas_preview.render(&samples, std::string(1000, 'X') + "\\xff", "wifi", "status", 0);
        auto image_without_rows = read_png_file(atlas_path.c_str(), false);
        CHECK(image_without_rows && !image_without_rows->row_pointers);
    }
    unlink(atlas_path.c_str());
    rmdir(temp_dir);
    // Oversized startup images are clipped rather than copied past the buffer.
    ChitonPngImage startup;
    startup.width = 640; startup.height = 480;
    startup.image = static_cast<u16 *>(calloc(640 * 480, sizeof(u16)));
    startup.image[0] = 0x1234;
    CHECK(preview.startup_image(startup)[20 * width] == 0x1234);
#ifdef CHITON_SDL2
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    for (int i = 0; i < 3; ++i) {
        auto desktop = create(Mode::Desktop);
        desktop->present(image);
        CHECK(desktop->process_events());
        SDL_Event event{};
        if (i == 0) { event.type = SDL_KEYDOWN; event.key.keysym.sym = SDLK_ESCAPE; }
        else if (i == 1) { event.type = SDL_WINDOWEVENT; event.window.event = SDL_WINDOWEVENT_CLOSE; }
        else event.type = SDL_QUIT;
        CHECK(SDL_PushEvent(&event) == 1);
        CHECK(!desktop->process_events());
    }
#endif
    std::cout << "display tests passed\n";
}
