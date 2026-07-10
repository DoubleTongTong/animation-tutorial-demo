#include <memory>
#include "Window.h"
#include "Logger.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char *argv[]) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif

  std::unique_ptr<Window> w = std::make_unique<Window>();

  if (!w->init(640, 480, "Test Window")) {
    Logger::log(1, "%s error: Window init error\n", __FUNCTION__);
    return -1;
  }

  w->mainLoop();
  w->cleanup();

  return 0;
}
