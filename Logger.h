#pragma once
#include <cstdio>

class Logger {
  public:
    /* 如果输入的日志级别小于或等于设置的日志级别，则记录日志 */
    template <typename ... Args>
    static void log(unsigned int logLevel, Args ... args) {
      if (logLevel <= mLogLevel) {
        std::printf(args ...);
        /* 强制输出，例如针对 Eclipse */
        std::fflush(stdout);
      }
    }

    static void setLogLevel(unsigned int inLogLevel) {
      inLogLevel <= 9 ? mLogLevel = inLogLevel : mLogLevel = 9;
    }

  private:
    static unsigned int mLogLevel;
};
