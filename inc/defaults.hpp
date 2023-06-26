#pragma once

#ifndef DEFAULTS_HPP
#define DEFAULTS_HPP

#include <string>

namespace DEFAULT
{
    const std::string AMQP_HOST = "amqp://localhost/";
    const std::string AVAILABLE_SOURCES_EX = "Available-Sources-Exchange";
    const std::string OBSOLETE_SOURCES_EX = "Unregister-Sources-Exchange";

    const std::string AVAILABLE_SOURCES_QUE = "Available-Sources-Queue";
    const std::string OBSOLETE_SOURCES_QUE = "Unregister-Sources-Queue";
}

#endif