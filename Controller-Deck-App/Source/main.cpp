#include "utils/MainApp.hpp"

int main() {
    MainApp app("config.json"); // oppure "config.json" se lo tieni accanto all'EXE
    return app.run();
}