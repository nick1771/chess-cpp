#include "Pandora/Image.h"

#include "Pandora/Windowing/Window.h"
#include "Pandora/Mathematics/Vector.h"
#include "Pandora/Graphics/Renderer.h"

int main() {
    using namespace Pandora;

    auto window = Window{};
    window.setFramebufferSize(800, 600);
    window.setTitle("Sandbox");
    window.show();

    auto renderer = Renderer{};
    renderer.initialize(window);

    auto image = Image::create(1, 1, Color8::Blue);
    auto image2 = Image::create(1, 1, Color8::Red);

    auto texture = renderer.createTexture(image);
    auto texture2 = renderer.createTexture(image2);

    while (!window.isCloseRequested()) {
        window.poll();

        for (const auto& event : window.getPendingEvents()) {
            if (event.is<WindowResizeEndEvent>()) {
                const auto& resizeEvent = event.getData<WindowResizeEndEvent>();
                renderer.resize(resizeEvent.width, resizeEvent.height);
            }
        }

        renderer.draw(10, 10, 100, 100, texture);
        renderer.draw(80, 80, 100, 100, texture2);

        renderer.display();
    }

    return 0;
}
