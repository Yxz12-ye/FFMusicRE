#include<filesystem>
#include<SFML/Audio.hpp>

class PlayerMgr
{
private:
    std::filesystem::path url;
    sf::Music musicPlayer;
public:
    PlayerMgr();
    PlayerMgr(std::string& filePath);
    ~PlayerMgr();
    void pauseMusic();
    void playMusic();
    void switchMusicStatus();
    void openFromFile(std::string& filePath);
};