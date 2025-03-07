#include <thread>
#include <future>
#include <memory>
#include "app-window.h"
#include "PlayerMgr.h"
#include <conio.h>
#include "FileMgr.h"

#define DEBUG
#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )
//����Ĵ�����Ϊ�����ο���̨ The above code is to block the console

std::shared_ptr<PlayerMgr> g_musicPlayer;

int main(int argc, char *argv[])
{
    std::thread PlayerThread([]{
        std::string test = "C:\\Users\\30789\\Music\\test.mp3";
        auto player = std::make_shared<PlayerMgr>(test);
        g_musicPlayer = player;//�����������̰߳�ȫ������ There is almost no thread safety issue
        while (true) {std::this_thread::sleep_for(std::chrono::seconds(1000));};
    });
    PlayerThread.detach();


    //������UI����(Slint) Below is the UI part (Slint)
    auto ui = MainWindow::create();
    ui->on_play([]{//Play Button
        g_musicPlayer->switchMusicStatus();
    });
    ui->on_open([]{//Open File Button
        g_musicPlayer->pauseMusic();
        std::string temp_url;
        std::thread FileThread([&temp_url]{
            temp_url = OpenFileDialog();
        });
        FileThread.join();//�ᵼ��ѡ���ļ�ʱUI����,��������������CPUռ��(�����˼·,�����Ժ�����) It will cause the UI to freeze when selecting files, but it can significantly reduce CPU usage (this is an idea, but I will fix it later)
        g_musicPlayer->openFromFile(temp_url);
    });
    ui->run();
    
    return 0;
}
