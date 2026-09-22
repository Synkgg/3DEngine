#include "Logger.h"
#include <iostream>

std::vector<LogMessage> Logger::s_Messages;

void Logger::Info(const std::string& message)
{
    s_Messages.push_back({ LogLevel::Info, message });
    std::cout << "[Info] " << message << std::endl;
}

void Logger::Warning(const std::string& message)
{
    s_Messages.push_back({ LogLevel::Warning, message });
    std::cout << "[Warning] " << message << std::endl;
}

void Logger::Error(const std::string& message)
{
    s_Messages.push_back({ LogLevel::Error, message });
    std::cerr << "[Error] " << message << std::endl;
}

void Logger::Debug(const std::string& message)
{
    s_Messages.push_back({ LogLevel::Debug, message });
    std::cout << "[Debug] " << message << std::endl;
}

const std::vector<LogMessage>& Logger::GetMessages()
{
    return s_Messages;
}

void Logger::Clear()
{
    s_Messages.clear();
}