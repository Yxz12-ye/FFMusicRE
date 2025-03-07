#include"PlayerMgr.h"

PlayerMgr::PlayerMgr(){}

PlayerMgr::PlayerMgr(std::string &filePath)
{
    url=filePath;
    musicPlayer.openFromFile(url);
}

PlayerMgr::~PlayerMgr(){}

void PlayerMgr::pauseMusic()
{
    musicPlayer.pause();
}

void PlayerMgr::playMusic()
{
    musicPlayer.play();
}

void PlayerMgr::switchMusicStatus()
{
    auto temp = musicPlayer.getStatus();
    if(temp==sf::SoundSource::Status::Playing) pauseMusic();
    else if(temp==sf::SoundSource::Status::Paused||temp==sf::SoundSource::Status::Stopped) playMusic();
}

void PlayerMgr::openFromFile(std::string &filePath)
{
    musicPlayer.openFromFile(filePath);
}
