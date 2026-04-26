#include "app-window.h"
#include "app_controller.h"

#include <cstdlib>

int main()
{
#ifdef _WIN32
    _putenv_s("SLINT_BACKEND", "winit-software");
    _putenv_s("SLINT_SOFTWARE_RENDERER_PARLEY_DISABLED", "1");
#else
    setenv("SLINT_BACKEND", "winit-software", 1);
    setenv("SLINT_SOFTWARE_RENDERER_PARLEY_DISABLED", "1", 1);
#endif

    auto window = AppWindow::create();
    AppController controller(window);
    controller.initialize();
    window->run();
    return 0;
}
