#include "Pandora/Image.h"

#include "Pandora/Windowing/Window.h"
#include "Pandora/Mathematics/Vector.h"

#include "Pandora/Graphics/GraphicsDevice.h"
#include "Pandora/Graphics/SceneRenderer.h"
#include "Pandora/Graphics/Texture.h"

#include "Pandora/Graphics/Scene.h"

int main() {
    using namespace Pandora;

    auto window = Window{};
    window.setFramebufferSize(800, 600);
    window.setTitle("Sandbox");
    window.setResizeable(false);
    window.show();

    auto device = GraphicsDevice{};
    device.configure(window);

    auto renderer = SceneRenderer{ device };

    auto texture1 = Texture{ device, Image::create(1, 1, Color8::Blue) };
    auto texture2 = Texture{ device, Image::create(1, 1, Color8::Red) };
    auto texture3 = Texture{ device, Image::create(1, 1, Color8::Green) };

    auto scene = Scene{};

    auto head = Sprite{};
    head.texture = texture1;
    head.position = Vector2f{ 200.f, 200.f };
    head.scale = Vector2f{ 50.f, 50.f };

    auto body = Sprite{};
    body.position = Vector2f{ 0.f, 10.f };
    body.texture = texture2;
    body.scale = Vector2f{ 50.f, 50.f };

    auto legs = Sprite{};
    legs.position = Vector2f{ 100.f, 30.f };
    legs.texture = texture3;
    legs.scale = Vector2f{ 50.f, 50.f };

    auto camera = Camera{};
    camera.size = window.getFramebufferSize();

    while (!window.isCloseRequested()) {
        window.poll();

        for (const auto& event : window.getPendingEvents()) {
            if (event.is<WindowResizeEndEvent>()) {
                device.configure(window);

                const auto& resizeEvent = event.getData<WindowResizeEndEvent>();
                camera.size = Vector2f{ resizeEvent.width, resizeEvent.height };
            }
        }

        scene.sprites.push_back(body);
        scene.sprites.push_back(legs);
        scene.sprites.push_back(head);

        renderer.setCamera(device, camera);
        renderer.draw(device, scene);
    }

    return 0;
}
