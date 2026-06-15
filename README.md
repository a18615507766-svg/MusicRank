# MusicRank 歌曲排行榜

## 运行

双击 `MusicRank.exe`。首次启动会在 `data/musicrank.db` 创建本地数据库，并导入 104 首歌曲。

## 主要功能

- 按歌名、演唱者、作词、作曲和专辑搜索
- 按热度、播放量、点赞、投币、发行日期等排序
- 点赞、踩、投币和播放量统计
- 新建歌单、添加歌曲和歌单筛选
- 新增、编辑、删除歌曲信息
- 播放本地 MP3，支持暂停、进度、音量、上一首和下一首

## 音频目录

歌曲文件放在 `media/` 目录中。出于文件大小和版权考虑，GitHub 仓库不包含 MP3 文件。删除歌曲记录不会删除 MP3 文件。

## 源码构建

在项目根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

项目使用 C++17、Qt 6.8.3、SQLite、Qt Multimedia 和 CMake。
