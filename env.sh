#!/bin/bash

declare -A env_vars=(
    ["RabbitMQ_Address"]="amqp://localhost/"
    ["EXCHANGE_AVAILABLE_SOURCES"]="Available-Sources-Exchange"
    ["EXCHANGE_UNREGISTER_SOURCES"]="Unregister-Sources-Exchange"
    ["AVAILABLE_SOURCES_QUEUE"]="Available-Sources-Queue"
    ["UNREGISTER_SOURCES_QUEUE"]="Unregister-Sources-Queue"
);

for var in "${!env_vars[@]}"; do 
    export "$var=${env_vars[$var]}";
    echo "$var=${env_vars[$var]}";
done