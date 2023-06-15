#pragma once
#ifndef MISSING_ENVIROMENT_VARIABLE_H
#define MISSING_ENVIROMENT_VARIABLE_H

#include <exception>
#include <string>

class missing_environment_variable : public std::exception
{
    std::string message;

    public:
        missing_environment_variable(std::string_view env_variable) noexcept
        { 
            message = std::string("Environment variable ") + std::string(env_variable) + std::string(" is not set");
        };

        virtual const char* what() const noexcept override {
            return message.c_str();
        }

        virtual ~missing_environment_variable() = default;
};

#endif // MISSING_ENVIROMENT_VARIABLE_H