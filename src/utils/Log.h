#pragma once

struct ILog
{
    virtual ~ILog() {}
    virtual void AddLog(const char* fmt, ...) = 0;
};

extern ILog* g_log;

#define SE_LOG(...) (g_log && (g_log->AddLog(__VA_ARGS__), true))