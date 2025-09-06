#pragma once
#include "Api/ApiServer.hpp"
class MainApp;

namespace ApiWiring {
    ApiServer::Callbacks MakeCallbacks(MainApp& app);
}
