#include "Valkron.hpp"
#include "SampleScene.hpp"

namespace Valkron {

    class SandboxApp : public Application {
        public:
            void onInit() override {
                Sample::createTriangleEntity(*getScene(), "HelloTriangle");
            }
    };

}

int main() {
    Valkron::SandboxApp app;
    return app.run();
}