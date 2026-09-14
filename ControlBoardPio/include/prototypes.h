// Forward declarations PlatformIO's .ino prototype generator misses
// (Arduino IDE's ctags pass generates these automatically).
#pragma once

#ifdef __cplusplus
#include <string>

void handleCommand(const std::string& cmd);
#endif
