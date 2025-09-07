#pragma once
#include "Api/ApiServer.hpp"
#include <nlohmann/json.hpp>
class MainApp;

namespace ApiWiring {
    ApiServer::Callbacks MakeCallbacks(MainApp& app);
}
