#include <string.h>
#ifdef JUCE_LINUX
#include <X11/Xlib.h>
#include <X11/Xutil.h>
namespace e47 {
static Display* display = NULL;

int getScreenShotData(unsigned char** buffer, int x, int y, unsigned int width, unsigned int height) {
    int ret = 0;
    if (!display) {
        display = XOpenDisplay(NULL);
    }

    if (!display) {
        // MSG("Fatal: Failed to open X display");
        return -1;
    }

    Window win = DefaultRootWindow(display);
    XImage* img = XGetImage(display, win, x, y, width, height, AllPlanes, ZPixmap);
    memcpy(*buffer, img->data, width * height * 4);
    XDestroyImage(img);
    return ret;
}

void closeDisplayHandle() {
    if (display) {
        XCloseDisplay(display);
    }
}

}  // namespace e47

#endif
