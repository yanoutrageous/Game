# Graytail (灰尾回收) — UE C++ 迁移项目

扫雷情报驱动的 2D 俯视搜打撤 roguelike。`scripts/` 是已验证可玩的 Lua 原型(只读参照),正在逐块迁移到 `UE/Graytail/` 的 C++ 内核框架。设计文档在 `docs/`。

## 构建与验证 (UE 5.7 / Win64)

- 编译前必须完全关闭 UE 编辑器(Live Coding 占编译锁,报 "Unable to build while Live Coding is active")
- 编译: `Engine\Build\BatchFiles\Build.bat GraytailEditor Win64 Development "-Project=<绝对路径>\UE\Graytail\Graytail.uproject" -WaitMutex`,看退出码(0=成功)
- 冒烟测试(必须全过): `UnrealEditor-Cmd.exe <uproject> -run=GT_RuntimeSmokeRunner -unattended -nopause -nosplash` → 输出含 `Overall=Pass`
- 无头玩法验证: `UnrealEditor-Cmd.exe <uproject> -game -nullrhi -unattended -nopause -nosplash -ExecCmds="gt.StartStd Standard 12345, gt.Bag, quit"` — stdout 无输出,结果 grep `UE/Graytail/Saved/Logs/Graytail.log` 的 `LogGraytailManualPlay`
- 手动验证: PIE 内 `` ` `` 控制台跑 gt.* 命令(`gt.Help` 看全表;gt.* 仅在 PIE/game world 生效);`gt.HUD` 打开最小可玩界面
- UI 渲染效果无头验证不了,需人工 PIE 目检

## 架构铁律

- BasicDebug 模式 = 冻结测试夹具(163 冒烟测试依赖其固定布局: 雷(2,2)/出口(9,9)/事件(4,1)/战斗(1,4)),**永不改其行为**;新规则加平行路径,运行时按 `RunContext::GetMapMode()` 分流(Standard = 真实规则)
- 改状态的玩家动作必须走 Command 管线(CommandProcessor);UI/调试层只发命令、读状态、听事件
- 随机必须确定性: 用 (seed, x, y, salt) 的 int64 哈希,公式逐位对齐 Lua 原型(Balance.lua / Combat.lua / RunInventory.lua)
- UENUM 新值只加在末尾(不动已有序号);加完全局搜该枚举的 switch 补分支

## 已踩的坑

- `FGT_TruthCell.bResolved` 在进房时即被置位(GT_RoomResolver::ResolveRoomAt),不能当"房间已处理完"用
- UHT 禁止 FGT_Xxx 与 UGT_Xxx 共享引擎名(FGT_ItemDef 会撞已有的 UGT_ItemDef)
- 纯 C++ UMG 必须在 `RebuildWidget()` 里搭 WidgetTree;NativeConstruct 时 Slate 已构建,再设 RootWidget 不显示
- 编辑器会自动改写 `Config/*.ini` 与 `.uproject`(EngineAssociation、输入轴展开等)——这类自动改动不要提交

## Git

- 仅在明确要求时提交;提交信息用 feat:/fix:/chore: 前缀 + 英文正文
- 美术素材在 `assets/`(运行时按文件名前缀模糊加载,文件名带导出时间戳);二进制资产走 LFS(.gitattributes 已配)
