#include "ScreenCapture.h"

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>

namespace screencap
{
bool captureScreenRect (int x, int y, int width, int height, std::vector<uint32_t>& out)
{
    if (width <= 0 || height <= 0)
        return false;
    HDC screen = GetDC (nullptr);
    if (screen == nullptr)
        return false;
    HDC mem = CreateCompatibleDC (screen);
    BITMAPINFO bi {};
    bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height; // top-down rows
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection (screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (mem != nullptr && bmp != nullptr && bits != nullptr)
    {
        HGDIOBJ old = SelectObject (mem, bmp);
        ok = BitBlt (mem, 0, 0, width, height, screen, x, y, SRCCOPY | CAPTUREBLT) != 0;
        GdiFlush();
        if (ok)
        {
            out.resize ((size_t) width * (size_t) height);
            const auto* px = static_cast<const uint32_t*> (bits);
            for (size_t i = 0; i < out.size(); ++i)
                out[i] = 0xFF000000u | (px[i] & 0x00FFFFFFu);
        }
        SelectObject (mem, old);
    }
    if (bmp != nullptr) DeleteObject (bmp);
    if (mem != nullptr) DeleteDC (mem);
    ReleaseDC (nullptr, screen);
    return ok;
}
} // namespace screencap

#else

namespace screencap
{
bool captureScreenRect (int, int, int, int, std::vector<uint32_t>&) { return false; }
}

#endif
