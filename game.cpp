#ifndef DEBUG
#define DEBUG
#endif

#include "platform/window.hpp"

int main(int argc, char* argv[])
{
    GLWindow win;
    int res = win.init(800,600,"test");
    win.run();
    return res;
}