> [!WARNING]
> 此库仅作为学习windows api和c++所作，非常垃圾，不太适合用于任何项目.

## SimpleLogger
简单并且折叠重复日志的日志库，仅在 windows 和 SUBSYSTEM:WINDOWS(子系统为窗口) 使用

# 快速使用
```cpp
#include "include/logger.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR szCmdLine, int iCmdShow)
{   
    LOG_DEBUG("this is Debug Level");
    LOG_INFO("this is Info Level");
    LOG_WARNING("this is Warning Level");
    LOG_ERROR("this is Error Level");
    LOG_FATAL("this is Fatal Level");
    
    LOG_DEBUG("中文");

    LOGF_DEBUG("this is Debug Level %d", 233);
    
    // 测试日志
    while (true) {
        LOG_INFO("DEBUG");
        Sleep(1000);
    }

    return 0;
}
```

# 功能
1. 支持debug/info/warning/error/fatal五种日志级别
2. 支持折叠重复日志
3. 支持日志文件输出
4. 支持日志文件自动分割
5. 支持字符串格式化(使用snprintf)
6. 支持自由配置 (自行查看头文件)