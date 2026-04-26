#include "app-window.h"
#include "app_controller.h"

int main()
{
    auto window = AppWindow::create();
    AppController controller(window);
    controller.initialize();
    window->run();
    return 0;
}
