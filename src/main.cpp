#include "include/core/application.hpp"

int main(int argc, char** argv) {
    try {
        auto& app = ppsever::Application::GetInstance();
        
        if (!app.Initialize(argc, argv)) {
            return 1;
        }
        
        return app.Run();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
};