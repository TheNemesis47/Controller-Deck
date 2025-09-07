#include "utils/MainApp.hpp"

int main() {
    MainApp app{ "Source/config.json" };
    return app.run();
}
