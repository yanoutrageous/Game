# 设置菜单完善 — 显示 + 音频 (2026-06-24)

## 范围
把占位的「设置」面板(当前只有作弊模式开关)补成可用设置:
- **显示**: 显示模式(全屏/窗口/无边框) · 分辨率 · 垂直同步。
- **音频**: 接入现有 BGM(`assets/audio/Hero Immortal.ogg`)循环播放 + 主音量滑条(USlider)。
- 保留**作弊模式开关**。
- **不做**(用户拍板): 表现/玩法开关、伤害飘字、其他音效(下一步)、按键重绑定。

## 架构

### 新增 `UGT_SettingsSubsystem`(UGameInstanceSubsystem)
跨菜单/关卡常驻,统一管音频与音频偏好。
- `Initialize`: 从 `GConfig`(`GameUserSettings.ini` 段 `[Graytail.Settings]`,键 `MusicVolume`,默认 0.5)读音量;`LoadObject` BGM USoundWave(不在此播,无 world)。
- `StartMusicIfNeeded(UWorld*)`: 幂等。无活动组件时 `UGameplayStatics::SpawnSound2D(World, BGM, Volume, 1, 0, nullptr, /*persist*/false, /*autoDestroy*/false)` 拿 `UAudioComponent` 存为 `UPROPERTY` 引用(防 GC),`SetVolumeMultiplier(Volume)`,`bAutoDestroy=false` 跨界面常驻。由主菜单/HUD 构造时调一次。
- `SetMusicVolume(float 0..1)`: 有组件则 `SetVolumeMultiplier`;写回 `GConfig` 并 `Flush`。
- `GetMusicVolume()`。
- `Deinitialize`: 停掉组件。
- **假设**: 本作单一持久 world(全 UMG 切换、无 `OpenLevel`),故 SpawnSound2D 一次即可常驻;若日后改为切关再议(注释标注)。
- **为何子系统**: BGM 必须比任何单个 widget/world 活得久,GameInstanceSubsystem 是标准归宿。
- BGM 循环: USoundWave 导入时置 `bLooping=true`(SpawnSound2D 不强制循环,靠资源 looping 标志)。

### 显示: 直接用引擎 `UGameUserSettings`
`UGameUserSettings* GS = GEngine->GetGameUserSettings();`
- 显示模式: `EWindowMode`(Fullscreen / Windowed / WindowedFullscreen)轮换。
- 分辨率: 在 `GS->GetSupportedFullscreenResolutions(...)` 或精选常用列表里 ◀▶ 切。
- 垂直同步: `SetVSyncEnabled(bool)`。
- 应用: 改后 `SetFullscreenMode/SetScreenResolution/SetVSyncEnabled` → `ApplyResolutionSettings(false)` → `SaveSettings()`(自动写 `GameUserSettings.ini`)。
- **PIE 限制**: 编辑器内分辨率/模式可能不可见生效(编辑器掌控视口),打包/Standalone 真实生效;音量与作弊在 PIE 可验。

### 设置卡片改造(`GT_SettingsWidget`)
卡片仍为垂直列,加小节标题;行数增多 → 外层套 `UScrollBox` 防溢出(~7 行 + 标题)。复用行构件:
- `MakeCyclerRow(Label, &ValueText, OnPrev, OnNext)`: `[标签] [◀] [值] [▶]`(HorizontalBox)。
- `MakeToggleRow(Label, &ValueText, OnToggle)`: 点击切换的按钮(复用现有作弊按钮范式)。
- `MakeSliderRow(Label, &USlider*, &PercentText, OnChanged)`: 标签 + USlider + 百分比文本。
布局: 「显示」(模式/分辨率 cycler + 垂同 toggle)→「音频」(主音量 slider)→「其他」(作弊 toggle)→「返回」。
- **USlider 首用**: 设基础 `FSliderStyle`(亮色滑块/槽)保证深底卡片上可见;`OnValueChanged` 绑 `SetMusicVolume` 并刷百分比文本。

## 数据流
`Open()` 读当前值(UGameUserSettings + Subsystem)刷新所有行 → 用户改某项 → handler 改引擎/子系统 + 存盘 + 刷该行标签。音量滑条 `OnValueChanged` → `Subsystem->SetMusicVolume` + 刷百分比。

## 资产导入
`assets/audio/Hero Immortal.ogg` → `/Game/Graytail/Audio/Hero_Immortal`(USoundWave)。`import_png_assets.py` 只管 PNG;音频走一次性编辑器 py 导入(`AssetImportTask` + 设 `bLooping=true`),或手动导。LFS 已配二进制。代码按 `/Game/Graytail/Audio/Hero_Immortal` 加载。

## 持久化
- 音量 → `GConfig` `GGameUserSettingsIni` `[Graytail.Settings] MusicVolume`。
- 显示 → `UGameUserSettings::SaveSettings`(`GameUserSettings.ini` 引擎段)。
- `GameUserSettings.ini` 本机配置,**不进仓库**(同既有约定)。

## 验证
- 编译 0 + 163 冒烟(回归网)。
- UI/音频无头验不了 → **PIE 目检**: 开设置 → 切模式/分辨率/垂同 → 拖音量(听 BGM 变化)→ 作弊开关仍工作 → ESC/返回 关闭。
- 风险: ①PIE 多次 play BGM 重播 → `StartMusicIfNeeded` 幂等 + Deinitialize 停;②USlider 深底可见性 → 设亮色 style。

## 影响面 / 文件
- 新增: `GT_SettingsSubsystem.h/.cpp`。
- 改: `GT_SettingsWidget.h/.cpp`(卡片重构 + 三区块)。
- 接线: 主菜单/HUD 构造时调 `StartMusicIfNeeded`。
- 资产: `Content/Graytail/Audio/Hero_Immortal.uasset`(+ LFS)。
- BasicDebug/163 不受影响(纯 UI/音频,不碰规则内核)。
