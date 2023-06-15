#pragma once

#ifndef MY_UTILS_H
#define MY_UTILS_H

#include <optional>
#include <string>

#include "missing_environment_variable.hpp"

/**
 * Reads environment variable
 * @param variable  env variable's name  
 * @returns std::optional<std::string> if present
*/
inline std::optional<std::string> get_environment_variable(std::string_view variable)
{
    auto const& value = std::getenv( variable.begin() );

    if(value == nullptr)
        return std::nullopt;
    
    return std::string(value);
}

/**
 * Reads environment variable
 * @throws missing_environment_variable exception if variable is not set
 * @returns variable's value
*/
inline std::string get_env_or_throw(const std::string_view& var) {
    auto const& env = get_environment_variable(var);

    if(!env.has_value())
        throw missing_environment_variable(var);

    return env.value();
}

#endif // MY_UTILS_H