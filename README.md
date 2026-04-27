# FFMusicRE

FFMusicRE 是一个基于 C++20、[Slint](https://slint.dev/)、SFML 和 TagLib 的本地音乐播放器原型。  
当前版本重点放在 Windows 桌面端体验：本地音频播放、播放列表管理、专辑封面提取、歌词同步、背景图、自定义窗口图标和会话恢复。

## 功能

- 支持打开单个音频文件或整个文件夹
- 支持的音频扩展名：`mp3`、`flac`、`ogg`、`wav`、`aif`、`aiff`
- 使用 TagLib 读取标题、艺术家、专辑、码率、采样率等元数据
- 使用 SFML 播放音频
- 自动提取音频内嵌专辑封面
- 自动读取同名 `.lrc` 歌词文件
- 支持顺序播放、随机播放、单曲循环
- 支持播放进度拖动和音量调节
- 支持播放列表右键删除歌曲
- 支持关闭时保存会话，启动时恢复队列和音量
- 支持从程序目录加载背景图
- 支持 Windows 应用图标和窗口图标

## 当前交互

- 左键 `打开文件`：向当前播放列表末尾追加歌曲
- 右键 `打开文件`：清空当前播放列表
- 左键列表歌曲：立即切换并播放该歌曲
- 右键列表歌曲：从当前队列删除该歌曲
- 左键 `打开文件加`：扫描并载入整个文件夹中的音频

## 背景图

程序启动时会优先在可执行文件所在目录查找以下文件：

- `background.png`
- `background.jpg`
- `background.jpeg`

说明：

- 图片按 `cover` 方式显示，不会拉伸变形，比例不符时会裁切
- 如果背景图存在，主壳层会自动切为透明，让背景图透出来
- 图片解码走 SFML，文件扩展名写错时通常也能按实际内容识别

## 会话恢复

关闭程序时会保存当前会话，启动时自动恢复：

- 播放列表内容
- 音量

Windows 默认保存位置：

- `%LOCALAPPDATA%\FFMusicRE\history.txt`

## 依赖

- CMake 3.21+
- C++20 编译器
- [SFML](https://www.sfml-dev.org/)
- [TagLib](https://taglib.org/)
- [Slint](https://slint.dev/)
- vcpkg

项目当前通过 `vcpkg.json` 声明依赖，仓库内也包含 `3rdparty/slint`。

## 获取源码

如果你是首次拉取仓库，先初始化子模块：

```powershell
git submodule update --init --recursive
```

## 构建

### Windows MSVC + Ninja

```powershell
cmake --preset win-msvc-ninja-debug
cmake --build --preset win-msvc-ninja-debug
```

Release 版本：

```powershell
cmake --preset win-msvc-ninja-release
cmake --build --preset win-msvc-ninja-release
```

生成的可执行文件通常位于：

- `build/win-msvc-ninja-debug/bin/FFMusicRE.exe`
- `build/win-msvc-ninja-release/bin/FFMusicRE.exe`

### 其他预设

仓库里还提供了这些 CMake Preset：

- `win-clang-debug`
- `win-clang-release`
- `win-clang-cl-debug`
- `win-clang-cl-release`
- `win-msvc-vs-debug`
- `win-msvc-vs-release`
- `linux-gcc-debug`
- `linux-gcc-release`
- `linux-clang-debug`
- `macos-clang-debug`
- `macos-clang-release`

可用预设取决于本机工具链和平台。

## 项目结构

- [`src`](/D:/Code/Cpp/FFMusicRE/src)：C++ 源码
- [`ui`](/D:/Code/Cpp/FFMusicRE/ui)：Slint 界面、图标和图片资源
- [`cmake`](/D:/Code/Cpp/FFMusicRE/cmake)：CMake 辅助脚本
- [`3rdparty/slint`](/D:/Code/Cpp/FFMusicRE/3rdparty/slint)：Slint 子模块

几个关键文件：

- [`src/app_controller.cpp`](/D:/Code/Cpp/FFMusicRE/src/app_controller.cpp)：UI 与播放逻辑的主控制器
- [`src/audio_player.cpp`](/D:/Code/Cpp/FFMusicRE/src/audio_player.cpp)：SFML 音频播放封装
- [`src/track.cpp`](/D:/Code/Cpp/FFMusicRE/src/track.cpp)：音频扫描与元数据读取
- [`ui/app-window.slint`](/D:/Code/Cpp/FFMusicRE/ui/app-window.slint)：主界面

## 平台说明

- Windows 体验目前最完整
- 文件选择器当前使用 Windows 原生 COM 对话框
- 非 Windows 平台下文件对话框仍是占位实现，功能完整性不如 Windows

## 已知说明

- 这是一个本地播放器项目，不包含在线流媒体能力
- 会话恢复当前只恢复队列和音量，不恢复上次播放位置
- 背景图和历史记录属于运行时数据，不会自动打包进程序

## License

仓库当前未单独声明顶层许可证文件。  
`3rdparty/slint` 目录遵循其各自上游许可证与授权条款。
