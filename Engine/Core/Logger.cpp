#include "Logger.h"

std::vector<LogMessage> Logger::s_Messages;

void Logger::Info(const std::string& message)
{
    s_Messages.push_back({
        LogLevel::Info,
        message
        });
}

void Logger::Warning(const std::string& message)
{
    s_Messages.push_back({
        LogLevel::Warning,
        message
        });
}

void Logger::Error(const std::string& message)
{
    s_Messages.push_back({
        LogLevel::Error,
        message
        });
}

void Logger::Debug(const std::string& message)
{
    s_Messages.push_back({
        LogLevel::Debug,
        message
        });
}

const std::vector<LogMessage>& Logger::GetMessages()
{
    return s_Messages;
}

void Logger::Clear()
{
    s_Messages.clear();
}