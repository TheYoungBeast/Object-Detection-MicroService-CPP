#pragma once

#ifndef DEFAULTS_HPP
#define DEFAULTS_HPP

#include <string>

namespace DEFAULT
{
    const std::string AMQP_HOST = "amqp://localhost/";
    const std::string AVAILABLE_SOURCES_EX = "EXCHANGE_AVAILABLE_SOURCES";
    const std::string OBSOLETE_SOURCES_EX = "EXCHANGE_UNREGISTER_SOURCES";

    const std::string AVAILABLE_SOURCES_QUE = "AVAILABLE_SOURCES_QUEUE";
    const std::string OBSOLETE_SOURCES_QUE = "UNREGISTER_SOURCES_QUEUE";
}

#endif