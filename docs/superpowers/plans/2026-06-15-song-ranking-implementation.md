# 歌曲排行榜桌面应用实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建一个可在 Windows 10/11 运行的 C++17、Qt 6 本地歌曲排行榜，内置 104 首歌曲资料与 MP3，支持检索、分类歌单、互动排行、管理和播放。

**Architecture:** 使用 Qt Widgets 构建界面，SQLite 保存歌曲与互动数据，Qt Multimedia 播放相对路径音频。领域模型、仓储、服务、播放器和 UI 分离；核心业务使用 Qt Test 按测试驱动方式实现。

**Tech Stack:** C++17、Qt 6.8.3 Widgets/Sql/Multimedia/Test、SQLite、CMake、Ninja、MinGW 13.1、PowerShell

---

## 文件结构

```text
CMakeLists.txt                         顶层构建、测试和资源复制
cmake/Deploy.cmake                     Windows 部署脚本
src/main.cpp                           应用入口
src/app/Application.{h,cpp}            数据库初始化和主窗口装配
src/domain/Song.h                      歌曲领域模型和排序枚举
src/domain/Playlist.h                  歌单领域模型
src/database/Database.{h,cpp}          SQLite 连接、迁移和事务
src/database/Schema.h                  版本化建表 SQL
src/repositories/SongRepository.{h,cpp} 歌曲查询、搜索和排序
src/repositories/PlaylistRepository.*  歌单持久化
src/services/RankingService.{h,cpp}    热度计算和排序参数
src/services/ReactionService.{h,cpp}   点赞、踩、投币规则
src/services/PlaylistService.{h,cpp}   歌单业务规则
src/services/ImportService.{h,cpp}     文件名解析、冲突和批量导入
src/player/PlayerController.{h,cpp}    Qt Multimedia 播放控制
src/ui/MainWindow.{h,cpp}              主框架和页面切换
src/ui/SongTableModel.{h,cpp}          排行榜表格模型
src/ui/RankingPage.{h,cpp}             搜索、排序和歌曲详情
src/ui/PlaylistPage.{h,cpp}            歌单与歌单内排序
src/ui/CategoryPage.{h,cpp}            风格、作者、演唱者和语言分类
src/ui/SongEditorDialog.{h,cpp}        新增与编辑歌曲
src/ui/ImportDialog.{h,cpp}            批量导入与冲突确认
src/ui/PlayerBar.{h,cpp}               底部播放器
resources/app.qrc                       样式和图标资源
resources/style.qss                     中文浅色主题
data/song_metadata.csv                  104 首歌曲资料与来源
data/schema.sql                         可审阅的数据库结构副本
tools/bootstrap-toolchain.ps1           安装/定位 Qt 6 工具链
tools/build.ps1                         配置、构建和测试
tools/inventory-media.ps1               生成 MP3 清单
tools/copy-media.ps1                    复制并校验 104 个 MP3
tools/validate-metadata.ps1             校验 CSV 完整性与路径
tests/TestDatabase.cpp                  数据库初始化测试
tests/TestSongRepository.cpp            搜索和排序测试
tests/TestReactionService.cpp           互动测试
tests/TestPlaylistService.cpp           歌单测试
tests/TestImportService.cpp             导入解析和事务测试
tests/TestPlayerController.cpp          播放计数状态测试
tests/TestSongTableModel.cpp             表格映射测试
tests/TestMainWindow.cpp                 主窗口装配测试
tests/TestSongEditorDialog.cpp           歌曲表单校验测试
tests/TestPlayerBar.cpp                  播放器控件测试
README.md                               构建、运行、维护和导入说明
```

### Task 1: 固定 Qt 6 工具链

**Files:**
- Create: `tools/bootstrap-toolchain.ps1`
- Create: `tools/build.ps1`
- Create: `CMakePresets.json`
- Create: `CMakeLists.txt`

- [ ] **Step 1: 写工具链自检脚本并验证失败**

`tools/bootstrap-toolchain.ps1` 先检查固定目录：

```powershell
$ErrorActionPreference = "Stop"
$Root = Join-Path $PSScriptRoot "..\.tools"
$QtRoot = Join-Path $Root "Qt\6.8.3\mingw_64"
$CMake = Join-Path $Root "python\Scripts\cmake.exe"
$Ninja = Join-Path $Root "python\Scripts\ninja.exe"
$Gxx = Join-Path $Root "Qt\Tools\mingw1310_64\bin\g++.exe"

$missing = @($QtRoot, $CMake, $Ninja, $Gxx) | Where-Object { -not (Test-Path $_) }
if ($missing.Count -gt 0) {
    throw "Qt 6 toolchain missing: $($missing -join ', ')"
}

Write-Host "QtRoot=$QtRoot"
Write-Host "CMake=$CMake"
Write-Host "Ninja=$Ninja"
Write-Host "Gxx=$Gxx"
```

Run:

```powershell
powershell -ExecutionPolicy Bypass -File tools/bootstrap-toolchain.ps1
```

Expected: FAIL，列出 `.tools` 下缺失的 Qt、CMake、Ninja 和 MinGW。

- [ ] **Step 2: 在脚本中加入可重复安装逻辑**

在缺失检查之前加入：

```powershell
$Python = "C:\Users\35862\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
if (-not (Test-Path $Python)) { throw "Bundled Python not found: $Python" }

if (-not (Test-Path (Join-Path $Root "python"))) {
    & $Python -m venv (Join-Path $Root "python")
}
$VenvPython = Join-Path $Root "python\Scripts\python.exe"
& $VenvPython -m pip install --disable-pip-version-check "aqtinstall==3.3.0" "cmake==3.31.6" "ninja==1.11.1.3"

if (-not (Test-Path $QtRoot)) {
    & $VenvPython -m aqt install-qt windows desktop 6.8.3 win64_mingw -O (Join-Path $Root "Qt") -m qtmultimedia
}
if (-not (Test-Path $Gxx)) {
    & $VenvPython -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O (Join-Path $Root "Qt")
}
```

`CMakePresets.json`：

```json
{
  "version": 6,
  "configurePresets": [{
    "name": "windows-debug",
    "generator": "Ninja",
    "binaryDir": "${sourceDir}/build/debug",
    "cacheVariables": {
      "CMAKE_BUILD_TYPE": "Debug",
      "CMAKE_PREFIX_PATH": "${sourceDir}/.tools/Qt/6.8.3/mingw_64",
      "CMAKE_CXX_COMPILER": "${sourceDir}/.tools/Qt/Tools/mingw1310_64/bin/g++.exe",
      "CMAKE_MAKE_PROGRAM": "${sourceDir}/.tools/python/Scripts/ninja.exe"
    }
  }],
  "buildPresets": [{"name": "windows-debug", "configurePreset": "windows-debug"}],
  "testPresets": [{"name": "windows-debug", "configurePreset": "windows-debug", "output": {"outputOnFailure": true}}]
}
```

`tools/build.ps1`：

```powershell
$ErrorActionPreference = "Stop"
& "$PSScriptRoot\bootstrap-toolchain.ps1"
$CMake = "$PSScriptRoot\..\.tools\python\Scripts\cmake.exe"
& $CMake --preset windows-debug
& $CMake --build --preset windows-debug
& $CMake --build --preset windows-debug --target test
```

- [ ] **Step 3: 创建最小 CMake 项目**

```cmake
cmake_minimum_required(VERSION 3.24)
project(MusicRank VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
enable_testing()
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets Sql Multimedia Test)
add_subdirectory(src)
add_subdirectory(tests)
```

- [ ] **Step 4: 运行工具链与配置验证**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File tools/bootstrap-toolchain.ps1
.\.tools\python\Scripts\cmake.exe --preset windows-debug
```

Expected: Qt 版本为 6.8.3，CMake 配置成功并生成 `build/debug`。

- [ ] **Step 5: 提交**

```powershell
git add CMakeLists.txt CMakePresets.json tools
git commit -m "build: bootstrap Qt 6 toolchain"
```

### Task 2: 建立应用骨架和领域模型

**Files:**
- Create: `src/CMakeLists.txt`
- Create: `src/main.cpp`
- Create: `src/app/Application.h`
- Create: `src/app/Application.cpp`
- Create: `src/domain/Song.h`
- Create: `src/domain/Playlist.h`
- Create: `tests/CMakeLists.txt`
- Create: `tests/TestSongDomain.cpp`

- [ ] **Step 1: 写歌曲模型失败测试**

```cpp
void TestSongDomain::normalizesDisplayArtists()
{
    Song song;
    song.performers = {"周杰伦", "费玉清"};
    QCOMPARE(song.performerText(), QString("周杰伦、费玉清"));
}

void TestSongDomain::rejectsNegativeCounters()
{
    Song song;
    song.playCount = -1;
    QVERIFY(!song.isValid());
}
```

Run:

```powershell
.\.tools\python\Scripts\cmake.exe --build --preset windows-debug
.\build\debug\tests\TestSongDomain.exe
```

Expected: FAIL，因为 `Song` 尚不存在。

- [ ] **Step 2: 实现最小领域模型**

`Song.h` 定义：

```cpp
struct Song {
    qint64 id = 0;
    QString title;
    QStringList performers;
    QString lyricist;
    QString composer;
    QString album;
    QDate releaseDate;
    QString genre;
    QString language;
    qint64 durationMs = 0;
    QString audioPath;
    qint64 playCount = 0;
    qint64 likeCount = 0;
    qint64 dislikeCount = 0;
    qint64 coinCount = 0;

    QString performerText() const { return performers.join(QStringLiteral("、")); }
    bool isValid() const {
        return !title.trimmed().isEmpty()
            && !performers.isEmpty()
            && !audioPath.trimmed().isEmpty()
            && durationMs >= 0 && playCount >= 0 && likeCount >= 0
            && dislikeCount >= 0 && coinCount >= 0;
    }
};

enum class SongSort {
    Heat, PlayCount, LikeCount, DislikeCount, CoinCount,
    ReleaseDate, Title, Performer
};
enum class SortDirection { Ascending, Descending };
```

`Playlist.h` 定义 `id`、`name`、`description`、`isSystem` 和 `createdAt`。

- [ ] **Step 3: 创建应用入口**

`main.cpp` 创建 `QApplication`，设置应用名和组织名，调用 `Application::run()`。`Application::run()` 当前只创建空 `QMainWindow`。

- [ ] **Step 4: 运行测试**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File tools/build.ps1
```

Expected: `TestSongDomain` PASS，`MusicRank.exe` 构建成功。

- [ ] **Step 5: 提交**

```powershell
git add src tests
git commit -m "feat: add application skeleton and song domain"
```

### Task 3: 实现数据库迁移和首次初始化

**Files:**
- Create: `src/database/Schema.h`
- Create: `src/database/Database.h`
- Create: `src/database/Database.cpp`
- Create: `data/schema.sql`
- Create: `tests/TestDatabase.cpp`

- [ ] **Step 1: 写首次初始化失败测试**

```cpp
void TestDatabase::createsVersionOneSchema()
{
    QTemporaryDir dir;
    Database db(dir.filePath("music.db"));
    QVERIFY(db.open());
    QVERIFY(db.migrate());
    QCOMPARE(db.schemaVersion(), 1);
    QVERIFY(db.tableExists("songs"));
    QVERIFY(db.tableExists("playlists"));
    QVERIFY(db.tableExists("song_reactions"));
}

void TestDatabase::migrationIsIdempotent()
{
    QTemporaryDir dir;
    Database db(dir.filePath("music.db"));
    QVERIFY(db.open());
    QVERIFY(db.migrate());
    QVERIFY(db.migrate());
    QCOMPARE(db.schemaVersion(), 1);
}
```

Expected: FAIL，因为数据库类不存在。

- [ ] **Step 2: 实现版本 1 Schema**

`Schema.h` 保存单个事务内执行的 SQL，核心约束：

```sql
CREATE TABLE songs (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  title TEXT NOT NULL,
  performers TEXT NOT NULL,
  lyricist TEXT NOT NULL DEFAULT '',
  composer TEXT NOT NULL DEFAULT '',
  album TEXT NOT NULL DEFAULT '',
  release_date TEXT,
  genre TEXT NOT NULL DEFAULT '',
  language TEXT NOT NULL DEFAULT '',
  duration_ms INTEGER NOT NULL DEFAULT 0 CHECK(duration_ms >= 0),
  audio_path TEXT NOT NULL,
  play_count INTEGER NOT NULL DEFAULT 0 CHECK(play_count >= 0),
  like_count INTEGER NOT NULL DEFAULT 0 CHECK(like_count >= 0),
  dislike_count INTEGER NOT NULL DEFAULT 0 CHECK(dislike_count >= 0),
  coin_count INTEGER NOT NULL DEFAULT 0 CHECK(coin_count >= 0),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
);
CREATE INDEX idx_songs_title ON songs(title);
CREATE INDEX idx_songs_performers ON songs(performers);
```

同时创建 `playlists`、`playlist_songs`、`song_reactions`、`coin_events`、`play_events`、`metadata_sources`、`app_settings`，并插入 `schema_version=1`。

- [ ] **Step 3: 实现 Database API**

公开 `open()`、`migrate()`、`schemaVersion()`、`tableExists()`、`connection()` 和 `lastError()`。连接名使用数据库路径 SHA-256 前 12 位，析构时关闭并移除连接。

- [ ] **Step 4: 运行数据库测试**

Run:

```powershell
.\build\debug\tests\TestDatabase.exe
```

Expected: 所有数据库测试 PASS。

- [ ] **Step 5: 提交**

```powershell
git add src/database data/schema.sql tests/TestDatabase.cpp tests/CMakeLists.txt src/CMakeLists.txt
git commit -m "feat: add versioned SQLite schema"
```

### Task 4: 实现歌曲仓储、搜索和排序

**Files:**
- Create: `src/repositories/SongRepository.h`
- Create: `src/repositories/SongRepository.cpp`
- Create: `src/services/RankingService.h`
- Create: `src/services/RankingService.cpp`
- Create: `tests/TestSongRepository.cpp`

- [ ] **Step 1: 写搜索与排序失败测试**

测试插入“晴天 / 周杰伦 / 叶惠美 / 流行”和“Lemon / 米津玄師 / STRAY SHEEP / J-Pop”，然后断言：

```cpp
QCOMPARE(repo.search("周杰伦", SongSort::Title, SortDirection::Ascending).size(), 1);
QCOMPARE(repo.search("叶惠美", SongSort::Title, SortDirection::Ascending).first().title, QString("晴天"));
QCOMPARE(repo.search("米津", SongSort::Title, SortDirection::Ascending).first().title, QString("Lemon"));
QCOMPARE(repo.list(SongSort::PlayCount, SortDirection::Descending).first().title, QString("Lemon"));
```

Expected: FAIL，因为仓储不存在。

- [ ] **Step 2: 实现参数化搜索**

`SongRepository::search()` 使用一个绑定值匹配：

```sql
WHERE title LIKE :term ESCAPE '\'
   OR performers LIKE :term ESCAPE '\'
   OR lyricist LIKE :term ESCAPE '\'
   OR composer LIKE :term ESCAPE '\'
   OR album LIKE :term ESCAPE '\'
```

排序列仅通过 `SongSort` 到固定 SQL 片段的映射选择，禁止把界面字符串拼入 SQL。

- [ ] **Step 3: 实现热度表达式**

`RankingService::heat(const Song&)`：

```cpp
return song.playCount
     + song.likeCount * 20
     + song.coinCount * 30
     - song.dislikeCount * 10;
```

SQL 排序使用相同表达式，增加 `id ASC` 作为稳定次序。

- [ ] **Step 4: 运行测试**

Run:

```powershell
.\build\debug\tests\TestSongRepository.exe
```

Expected: 搜索、八类排序及热度测试全部 PASS。

- [ ] **Step 5: 提交**

```powershell
git add src/repositories src/services/RankingService.* tests/TestSongRepository.cpp
git commit -m "feat: add song search and ranking repository"
```

### Task 5: 实现点赞、踩和投币

**Files:**
- Create: `src/services/ReactionService.h`
- Create: `src/services/ReactionService.cpp`
- Create: `tests/TestReactionService.cpp`

- [ ] **Step 1: 写状态机失败测试**

```cpp
service.toggleLike(songId);
QCOMPARE(service.reaction(songId), Reaction::Like);
QCOMPARE(repo.find(songId)->likeCount, 1);

service.toggleDislike(songId);
QCOMPARE(service.reaction(songId), Reaction::Dislike);
QCOMPARE(repo.find(songId)->likeCount, 0);
QCOMPARE(repo.find(songId)->dislikeCount, 1);

service.toggleDislike(songId);
QCOMPARE(service.reaction(songId), Reaction::None);
QCOMPARE(repo.find(songId)->dislikeCount, 0);

service.addCoin(songId);
service.addCoin(songId);
QCOMPARE(repo.find(songId)->coinCount, 2);
```

Expected: FAIL。

- [ ] **Step 2: 用事务实现互动**

`toggleLike()` 和 `toggleDislike()` 在一个事务中读取旧状态、更新 `song_reactions`、增减统计列。所有减法使用 `MAX(column - 1, 0)`。`addCoin()` 插入 `coin_events` 并将 `coin_count` 加一。

- [ ] **Step 3: 增加回滚测试**

删除目标歌曲后调用 `addCoin()`，断言返回 `false`，且 `coin_events` 没有孤立记录。

- [ ] **Step 4: 运行测试**

Run:

```powershell
.\build\debug\tests\TestReactionService.exe
```

Expected: 状态互斥、取消、重复投币、持久化和失败回滚全部 PASS。

- [ ] **Step 5: 提交**

```powershell
git add src/services/ReactionService.* tests/TestReactionService.cpp
git commit -m "feat: add persistent song reactions"
```

### Task 6: 实现歌单与分类查询

**Files:**
- Create: `src/repositories/PlaylistRepository.h`
- Create: `src/repositories/PlaylistRepository.cpp`
- Create: `src/services/PlaylistService.h`
- Create: `src/services/PlaylistService.cpp`
- Create: `tests/TestPlaylistService.cpp`

- [ ] **Step 1: 写歌单规则失败测试**

覆盖新建、重命名、添加、重复添加、移除、删除普通歌单，以及禁止删除“全部歌曲”。

```cpp
auto id = service.create("通勤", "上班路上");
QVERIFY(service.addSong(id, songId));
QVERIFY(!service.addSong(id, songId));
QCOMPARE(service.songs(id, SongSort::Title, SortDirection::Ascending).size(), 1);
QVERIFY(service.removeSong(id, songId));
QVERIFY(service.remove(id));
QVERIFY(!service.remove(service.allSongsPlaylistId()));
```

- [ ] **Step 2: 初始化系统歌单**

首次迁移后创建：全部歌曲、华语歌曲、日语歌曲、英语歌曲、游戏音乐、我的收藏。系统歌单带 `is_system=1`；“全部歌曲”和语言分类使用动态查询，不复制 `playlist_songs`。

- [ ] **Step 3: 实现分类聚合**

提供 `genres()`、`performers()`、`lyricists()`、`composers()`、`languages()`，以及 `songsByCategory(field, value, sort, direction)`。分类字段同样使用枚举映射，不能接收任意列名。

- [ ] **Step 4: 运行测试**

Run:

```powershell
.\build\debug\tests\TestPlaylistService.exe
```

Expected: 歌单 CRUD、系统歌单保护、分类和歌单内排序全部 PASS。

- [ ] **Step 5: 提交**

```powershell
git add src/repositories/PlaylistRepository.* src/services/PlaylistService.* tests/TestPlaylistService.cpp
git commit -m "feat: add playlists and category browsing"
```

### Task 7: 实现 MP3 文件名解析和批量导入

**Files:**
- Create: `src/services/ImportService.h`
- Create: `src/services/ImportService.cpp`
- Create: `tests/TestImportService.cpp`

- [ ] **Step 1: 写文件名解析失败测试**

```cpp
auto parsed = ImportService::parseFileName("晴天 - 周杰伦.mp3");
QCOMPARE(parsed.title, QString("晴天"));
QCOMPARE(parsed.performers, QStringList{"周杰伦"});

parsed = ImportService::parseFileName("BITE! 咬合力 - 雷雨心、三Z-STUDIO、HOYO-MiX.mp3");
QCOMPARE(parsed.performers.size(), 3);

parsed = ImportService::parseFileName("We Don't Talk Anymore (Mr. Collipark Remix) - Charlie Puth、Selena Gomez.mp3");
QCOMPARE(parsed.title, QString("We Don't Talk Anymore (Mr. Collipark Remix)"));
```

最后一个 `" - "` 作为标题与演唱者分隔符。

- [ ] **Step 2: 实现冲突检测**

规范化键为 `title.trimmed().toCaseFolded() + '\n' + performers.join("、").toCaseFolded()`。导入结果使用 `Imported`、`Duplicate`、`NeedsReview`、`Failed` 四种状态。

- [ ] **Step 3: 实现整批事务**

`importFiles()` 先解析全部文件，存在 `Failed` 时不写数据库；全部可处理时在单个事务写入。重复歌曲由调用方传入 `Skip` 或 `ImportAnyway` 决策。

- [ ] **Step 4: 运行测试**

Run:

```powershell
.\build\debug\tests\TestImportService.exe
```

Expected: 中文、多演唱者、标题含连字符、重复检测和整批回滚全部 PASS。

- [ ] **Step 5: 提交**

```powershell
git add src/services/ImportService.* tests/TestImportService.cpp
git commit -m "feat: add transactional MP3 import"
```

### Task 8: 实现播放器和播放量会话规则

**Files:**
- Create: `src/player/PlayerController.h`
- Create: `src/player/PlayerController.cpp`
- Create: `tests/TestPlayerController.cpp`

- [ ] **Step 1: 写播放计数状态失败测试**

将媒体输出封装为 `IMediaBackend`，测试使用同步假后端：

```cpp
controller.play(song);
backend.emitPlaying();
QCOMPARE(repo.find(song.id)->playCount, 1);

controller.pause();
controller.resume();
backend.emitPlaying();
QCOMPARE(repo.find(song.id)->playCount, 1);

controller.play(song);
backend.emitPlaying();
QCOMPARE(repo.find(song.id)->playCount, 2);
```

- [ ] **Step 2: 实现 Qt 媒体后端**

生产后端组合 `QMediaPlayer` 和 `QAudioOutput`，转发 `playbackStateChanged`、`positionChanged`、`durationChanged`、`mediaStatusChanged` 和 `errorOccurred`。

- [ ] **Step 3: 实现队列与相对路径**

`PlayerController` 接收应用根目录，使用 `QDir(appRoot).filePath(song.audioPath)`。支持 play/pause/resume、seek、volume、previous、next、顺序播放和列表循环。

- [ ] **Step 4: 运行测试**

Run:

```powershell
.\build\debug\tests\TestPlayerController.exe
```

Expected: 会话计数、暂停恢复、队列边界、缺失路径和循环测试全部 PASS。

- [ ] **Step 5: 提交**

```powershell
git add src/player tests/TestPlayerController.cpp
git commit -m "feat: add multimedia playback controller"
```

### Task 9: 实现排行榜表格模型

**Files:**
- Create: `src/ui/SongTableModel.h`
- Create: `src/ui/SongTableModel.cpp`
- Create: `tests/TestSongTableModel.cpp`

- [ ] **Step 1: 写列映射失败测试**

断言 9 列依次为名次、歌名、演唱者、风格、发行日期、播放量、点赞、踩、投币，并验证第二行名次显示 2。

- [ ] **Step 2: 实现只读 QAbstractTableModel**

`SongTableModel::setSongs()` 使用 `beginResetModel/endResetModel`。`data()` 对发行日期使用 `yyyy-MM-dd`，空日期显示 `未知`；计数使用本地化数字。

- [ ] **Step 3: 实现行到歌曲 ID 的稳定映射**

提供 `songIdAt(row)` 和 `songAt(row)`，越界返回 `0` 或 `std::nullopt`。

- [ ] **Step 4: 运行测试并提交**

```powershell
.\build\debug\tests\TestSongTableModel.exe
git add src/ui/SongTableModel.* tests/TestSongTableModel.cpp
git commit -m "feat: add ranking table model"
```

### Task 10: 实现排行榜主界面与互动详情

**Files:**
- Create: `src/ui/MainWindow.h`
- Create: `src/ui/MainWindow.cpp`
- Create: `src/ui/RankingPage.h`
- Create: `src/ui/RankingPage.cpp`
- Create: `resources/app.qrc`
- Create: `resources/style.qss`

- [ ] **Step 1: 写 UI 装配冒烟测试**

在 `tests/TestMainWindow.cpp` 使用对象名断言：

```cpp
MainWindow window(dependencies);
QVERIFY(window.findChild<QLineEdit*>("globalSearch"));
QVERIFY(window.findChild<QTableView*>("rankingTable"));
QVERIFY(window.findChild<QPushButton*>("likeButton"));
QVERIFY(window.findChild<QPushButton*>("dislikeButton"));
QVERIFY(window.findChild<QPushButton*>("coinButton"));
QVERIFY(window.findChild<QWidget*>("playerBar"));
```

- [ ] **Step 2: 实现排行榜优先布局**

左侧 `QListWidget` 导航，顶部搜索和 `QComboBox` 排序，中间 `QTableView`，右侧详情 `QFrame`，底部 `PlayerBar` 占位。窗口最小尺寸 1180×720。

- [ ] **Step 3: 连接搜索、排序和互动**

搜索使用 250ms 单次 `QTimer` 防抖；排序组合框映射到 `SongSort`；表格选中更新详情；互动成功后重新加载当前查询并保持歌曲选中。

- [ ] **Step 4: 应用浅色主题**

`style.qss` 定义紫色主色 `#6558D3`、背景 `#F6F7FB`、卡片白色、明确 hover/focus/disabled 状态。互动按钮同时显示“点赞/踩/投币”文字，不只显示图标。

- [ ] **Step 5: 构建、运行冒烟测试并提交**

```powershell
.\.tools\python\Scripts\cmake.exe --build --preset windows-debug
.\build\debug\tests\TestMainWindow.exe
git add src/ui resources tests/TestMainWindow.cpp
git commit -m "feat: add ranking-first main window"
```

### Task 11: 实现歌单、分类和歌曲管理界面

**Files:**
- Create: `src/ui/PlaylistPage.h`
- Create: `src/ui/PlaylistPage.cpp`
- Create: `src/ui/CategoryPage.h`
- Create: `src/ui/CategoryPage.cpp`
- Create: `src/ui/SongEditorDialog.h`
- Create: `src/ui/SongEditorDialog.cpp`
- Create: `src/ui/ImportDialog.h`
- Create: `src/ui/ImportDialog.cpp`

- [ ] **Step 1: 写表单校验失败测试**

```cpp
SongEditorDialog dialog;
dialog.setTitle("");
dialog.setPerformers("周杰伦");
dialog.setAudioPath("media/qingtian.mp3");
QVERIFY(!dialog.isFormValid());

dialog.setTitle("晴天");
QVERIFY(dialog.isFormValid());
```

- [ ] **Step 2: 实现歌单页**

左侧歌单列表，右侧复用 `SongTableModel`；提供新建、重命名、删除、添加歌曲和移除歌曲。系统歌单按钮按规则禁用。

- [ ] **Step 3: 实现分类页**

顶部分类维度下拉框，左侧分类值，右侧歌曲表格；进入分类后保留搜索和排序控件。

- [ ] **Step 4: 实现歌曲编辑和导入窗口**

歌曲编辑窗口覆盖规格中的全部字段，使用 `QDateEdit`、`QFileDialog` 和即时校验。导入窗口展示文件、解析标题、演唱者、状态和错误；存在冲突时要求选择跳过或继续。

- [ ] **Step 5: 运行 UI 测试并提交**

```powershell
.\build\debug\tests\TestSongEditorDialog.exe
git add src/ui tests/TestSongEditorDialog.cpp
git commit -m "feat: add playlists categories and song management UI"
```

### Task 12: 实现底部播放器界面

**Files:**
- Create: `src/ui/PlayerBar.h`
- Create: `src/ui/PlayerBar.cpp`
- Modify: `src/ui/MainWindow.cpp`
- Create: `tests/TestPlayerBar.cpp`

- [ ] **Step 1: 写控件与格式失败测试**

验证存在上一首、播放/暂停、下一首、进度、音量和模式控件；`formatDuration(134000)` 返回 `02:14`。

- [ ] **Step 2: 实现 PlayerBar**

将按钮连接到 `PlayerController`；拖动进度时暂停位置刷新，释放后 seek；音量范围 0–100；错误通过可关闭提示条显示。

- [ ] **Step 3: 更新 MainWindow**

将 `PlayerBar` 放在中央布局底部，双击排行榜、歌单和分类表格时构建当前列表播放队列，并从双击歌曲开始。

- [ ] **Step 4: 运行测试并提交**

```powershell
.\build\debug\tests\TestPlayerBar.exe
git add src/ui/PlayerBar.* src/ui/MainWindow.cpp tests/TestPlayerBar.cpp
git commit -m "feat: add persistent player bar"
```

### Task 13: 建立 104 首真实歌曲资料和媒体清单

**Files:**
- Create: `tools/inventory-media.ps1`
- Create: `tools/validate-metadata.ps1`
- Create: `data/song_metadata.csv`
- Create: `data/media_inventory.csv`

- [ ] **Step 1: 生成不可变媒体清单**

`inventory-media.ps1` 接收两个源目录，为每个 MP3 输出：

```text
source_path,file_name,size_bytes,sha256
```

Run:

```powershell
powershell -ExecutionPolicy Bypass -File tools/inventory-media.ps1 `
  -SourceA "C:\Users\35862\Documents\xwechat_files\wxid_acc3tw47kahp32_2e62\msg\file\2026-06\鸽鸽" `
  -SourceB "C:\Users\35862\Desktop\tingge" `
  -Output data/media_inventory.csv
```

Expected: 104 条数据记录、104 个不同 SHA-256。

- [ ] **Step 2: 建立元数据 CSV**

固定列：

```text
title,performers,lyricist,composer,album,release_date,genre,language,duration_ms,audio_path,source_url,verified_at
```

从文件名生成 104 行基础数据，`audio_path` 使用 `media/<原文件名>`。逐首查阅官方艺人、唱片公司、游戏音乐官方页面或 MusicBrainz；记录实际来源 URL 和 `2026-06-15` 之后的核对日期。无法可靠确认的作词、作曲、专辑或发行日期保留空值。

- [ ] **Step 3: 实现严格校验**

`validate-metadata.ps1` 检查：

```powershell
if ($rows.Count -ne 104) { throw "Expected 104 songs, got $($rows.Count)" }
if (($rows | Where-Object { [string]::IsNullOrWhiteSpace($_.title) }).Count) { throw "Missing title" }
if (($rows | Where-Object { [string]::IsNullOrWhiteSpace($_.performers) }).Count) { throw "Missing performers" }
if (($rows | Where-Object { $_.audio_path -notlike 'media/*' }).Count) { throw "Invalid audio path" }
if (($rows | Group-Object title,performers | Where-Object Count -gt 1).Count) { throw "Duplicate title/performers" }
```

同时校验非空日期为 `yyyy-MM-dd`、时长非负、语言和风格非空、每行有来源 URL。

- [ ] **Step 4: 人工抽查**

抽查至少 20 首，必须包含周杰伦、G.E.M.邓紫棋、YOASOBI、米津玄師、Taylor Swift、Justin Bieber、HOYO-MiX 和鸣潮音乐。将发现的冲突直接修正到 CSV 并保留更可信来源。

- [ ] **Step 5: 运行校验并提交**

```powershell
powershell -ExecutionPolicy Bypass -File tools/validate-metadata.ps1
git add tools/inventory-media.ps1 tools/validate-metadata.ps1 data/song_metadata.csv data/media_inventory.csv
git commit -m "data: add verified 104-song catalog"
```

### Task 14: 首次导入、媒体复制、部署和验收

**Files:**
- Create: `src/database/SeedData.h`
- Create: `src/database/SeedData.cpp`
- Create: `tools/copy-media.ps1`
- Create: `cmake/Deploy.cmake`
- Create: `README.md`
- Modify: `src/app/Application.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写种子数据失败测试**

```cpp
QVERIFY(seedData.importCsv(":/data/song_metadata.csv"));
QCOMPARE(songRepository.count(), 104);
QVERIFY(seedData.importCsv(":/data/song_metadata.csv"));
QCOMPARE(songRepository.count(), 104);
QCOMPARE(playlistRepository.systemPlaylistCount(), 6);
```

- [ ] **Step 2: 实现幂等首次导入**

CSV 作为 Qt 资源嵌入。仅在 `app_settings.seed_version < 1` 时导入；在单个事务插入 104 首歌曲、来源和系统歌单，成功后写 `seed_version=1`。

- [ ] **Step 3: 复制并校验媒体**

`copy-media.ps1` 根据 `data/media_inventory.csv` 复制到 `outputs/MusicRank/media/`，复制后重新计算 SHA-256；数量不是 104 或任一哈希不一致时失败。

- [ ] **Step 4: 构建 Release 与部署**

在 `CMakePresets.json` 增加：

```json
{
  "configurePresets": [{
    "name": "windows-release",
    "inherits": "windows-debug",
    "binaryDir": "${sourceDir}/build/release",
    "cacheVariables": {"CMAKE_BUILD_TYPE": "Release"}
  }],
  "buildPresets": [{
    "name": "windows-release",
    "configurePreset": "windows-release"
  }],
  "testPresets": [{
    "name": "windows-release",
    "configurePreset": "windows-release",
    "output": {"outputOnFailure": true}
  }]
}
```

将这些条目合并到已有同名数组，不建立第二组重复键。构建后运行 Qt 6.8.3 的 `windeployqt.exe`：

```powershell
& ".\.tools\Qt\6.8.3\mingw_64\bin\windeployqt.exe" `
  --release --no-translations --dir "outputs\MusicRank" `
  "build\release\src\MusicRank.exe"
```

复制 `MusicRank.exe`、`README.md` 和媒体目录。首次运行数据库放在可执行文件旁的 `data/musicrank.db`，若目录不可写则回退到 `QStandardPaths::AppDataLocation`。

- [ ] **Step 5: 全量自动验证**

Run:

```powershell
.\.tools\python\Scripts\cmake.exe --preset windows-release
.\.tools\python\Scripts\cmake.exe --build --preset windows-release
.\.tools\python\Scripts\ctest.exe --preset windows-release
powershell -ExecutionPolicy Bypass -File tools/validate-metadata.ps1
powershell -ExecutionPolicy Bypass -File tools/copy-media.ps1
```

Expected: 所有测试 PASS；元数据 104 行；输出媒体 104 个且哈希一致。

- [ ] **Step 6: 人工验收**

依次验证：

1. 启动后排行榜显示 104 首。
2. 搜索“周杰伦”“米津玄師”“HOYO-MiX”均返回结果。
3. 点击点赞后切换为踩，点赞数回退且踩数增加。
4. 连续投币两次，重启后投币数保留。
5. 按播放量、点赞、投币和热度排序。
6. 创建歌单、添加歌曲、歌单内排序、删除歌单。
7. 按风格、作词/作曲者、演唱者和语言分类。
8. 随机播放 10 首，测试暂停、seek、音量、上一首和下一首。
9. 临时移走一个 MP3，确认错误可见且程序不崩溃。
10. 新增、编辑、删除歌曲记录并批量导入测试文件夹。

- [ ] **Step 7: 编写说明并最终提交**

README 记录工具链安装、Debug/Release 构建、数据库位置、媒体目录规则、CSV 字段、导入命名规则和常见播放错误。

```powershell
git add CMakeLists.txt CMakePresets.json cmake src data tools README.md
git commit -m "feat: deliver MusicRank desktop application"
git status --short
```

Expected: 工作区干净，`outputs/MusicRank/` 包含可运行程序、依赖、数据库初始化能力和 104 个 MP3。
