#pragma once
#include <chrono>

class Timer {
  public:
    void start();
    float stop();
  private:
    bool mRunning = false;
    std::chrono::steady_clock::time_point mStartTime{};
};
