#pragma once

#include <string>
#include <vector>

enum class LogLevel
{
    Info,
    Warning,
    Error,
    Debug
};

struct LogMessage
{
    LogLevel level;
    std::string message;
};

class Logger
{
public:
    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);
    static void Debug(const std::string& message);

    static const std::vector<LogMessage>& GetMessages();

    static void Clear();

private:
    static std::vector<LogMessage> s_Messages;
};