#pragma once
// Captures what is really on screen (Windows: GDI BitBlt from the desktop, including layered and
// DirectComposition content). Used by the host test to prove the plug-in editor is not blank or
// transparent. Returns false on platforms without an implementation.

#include <cstdint>
#include <vector>

namespace screencap
{
// Physical-pixel rectangle; out receives width*height pixels as 0xAARRGGBB (alpha = 255).
bool captureScreenRect (int x, int y, int width, int height, std::vector<uint32_t>& out);
}
