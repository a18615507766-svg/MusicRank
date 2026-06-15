#pragma once

#include <memory>

class MainWindow;
class MusicDatabase;

class Application
{
public:
    Application();
    ~Application();

    int run();

private:
    std::unique_ptr<MusicDatabase> database_;
    std::unique_ptr<MainWindow> mainWindow_;
};
