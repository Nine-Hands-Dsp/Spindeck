#include "NativeWindow.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>

namespace nh
{
bool hostWindowRects (void* handle, juce::Rectangle<int>& rootPhys, juce::Rectangle<int>& selfPhys)
{
    auto h = (HWND) handle;
    if (h == nullptr) return false;
    HWND root = GetAncestor (h, GA_ROOT);
    RECT rr {}, rs {};
    if (root == nullptr || ! GetWindowRect (root, &rr) || ! GetWindowRect (h, &rs)) return false;
    rootPhys = { (int) rr.left, (int) rr.top, (int) (rr.right - rr.left), (int) (rr.bottom - rr.top) };
    selfPhys = { (int) rs.left, (int) rs.top, (int) (rs.right - rs.left), (int) (rs.bottom - rs.top) };
    return true;
}

bool moveHostWindow (void* handle, int x, int y)
{
    auto h = (HWND) handle;
    if (h == nullptr) return false;
    HWND root = GetAncestor (h, GA_ROOT);
    if (root == nullptr || IsZoomed (root)) return false;
    return SetWindowPos (root, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != 0;
}
}
#else
namespace nh
{
bool hostWindowRects (void*, juce::Rectangle<int>&, juce::Rectangle<int>&) { return false; }
bool moveHostWindow (void*, int, int) { return false; }
}
#endif
