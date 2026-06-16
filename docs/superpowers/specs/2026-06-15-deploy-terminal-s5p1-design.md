# 局外元进度 · S5 元终端 UI · Phase 1 — 设计文档

- 日期: 2026-06-15
- 分支: `feature/meta-progress`
- 子项目: 局外 S5 Phase 1(承接 S1 状态 / S2 结算 / S3 开局应用)
- 素材: `Content/Graytail/UI/{deploy,common}`(deploy 套件为局外大厅量身做)

## 1. 目标与范围

把局外系统做成玩家点得到的"部署终端"界面。**Phase 1 = 终端外壳 + 导航 + 申领(商店) + 作业装备(装备/loadout) + 金币栏 + 作业摘要**,跑通"买→装→带入→出发"核心闭环。

**Phase 2(后续)**: 仓库页(卖回收物)、天赋页(解锁)、抢救记录页(只读统计)。

**非目标**: 不改 S1~S3 后端(只调其公有 API);不做 6 新机制物品(S6);磨刀石/急救包/绝缘套专属图缺 → 用占位图标(flag 待补)。

## 2. 入口与归属(已与豪豪确认)

- 入口 = 主菜单**「装备天赋」**热区(现为"未开放提示")。
- 范式照 `GT_MainMenuWidget`(表现层薄壳): 新增 `GT_DeployTerminalWidget`,主菜单新增 `TDelegate<void()> OnDeployRequested`,在 ConfirmSelection 选中「装备天赋」时触发;**HUD(`GT_GameHudWidget`)持有部署终端**(像持有 MainMenu 一样),收到 OnDeployRequested 时 Open 终端、关主菜单;终端「返回」回主菜单,「出发探索」走既有 OnStartRequested 开局流程。
- 终端读写 **`UGT_MetaProgressSubsystem`**(S1 API);UI 只发调用、读状态、调用后重读刷新(无新状态)。

## 3. 布局(UMG, 纯 C++, RebuildWidget 搭 WidgetTree)

```
CanvasPanel(root, 填满)
├─ Border 背景刷 = ui_panel_terminal_main(9-slice 罩满)
├─ 顶栏 HorizontalBox: 标题"部署终端" + 弹簧 + [ui_icon_account_gold] + 结算币数字(常驻)
├─ 中部 HorizontalBox:
│   ├─ 左 导航 VerticalBox(宽~200): 申领/作业装备/仓库(P2)/天赋(P2)/抢救记录(P2)
│   │     每项 = Button(ui_button_nav_*) + 选中描 ui_frame_highlight
│   ├─ 中 内容 = Border(ui_panel_deploy_main_blank) → ScrollBox(ui_scrollbar_vertical) → 物品卡片列表
│   └─ 右 摘要 = Border(ui_panel_deploy_summary_blank): "作业摘要"标题 + 已装列表 + 本局净加成
└─ 底栏 HorizontalBox: [ui_button_back_main 返回] + 弹簧 + [ui_button_confirm_deploy_large 出发探索]
```

**物品卡片(统一复用控件 `BuildItemCard`)**: `[图标] 名称 / 效果文案 / 价或状态 / [动作按钮]`。动作按钮文案随上下文(申领=申领/已购、作业装备=装备/卸下/带入±)。买不起 → 价格红字 + 按钮禁用(改进点)。

## 4. 各页 → S1 API

### 4.1 申领(商店)
- 列表 = `GT_MetaCatalog::GetEquipDefs()`(6 装备)+ `GetConsumableDefs()`(止血贴)。
- 每件读 `OwnsItem`/`GetGold`:未拥有且买得起 → [申领] 调 `BuyItem`;已拥有 → [已购] 灰显;消耗品 → [申领] 调 `BuyConsumable(id,1)`(可多次)。
- 调用后重读刷新列表 + 顶栏金币 + 摘要。

### 4.2 作业装备(装备 + loadout)
- 上半:已购装备(`OwnsItem` 为真的)→ [装备]/[卸下] 调 `ToggleEquip`(≤2,满 2 时未装项按钮禁用 + 提示);已装项描 highlight。
- 下半:消耗品(`GetConsumableCount>0`)→ [−]/[+] 调 `SetLoadoutConsumable(id, n±1)`,显示"带入 n / 库存 m"(夹 maxCarry)。

### 4.3 作业摘要(右栏, 常驻刷新, 改进点)
- 已装:`GetEquippedItems()` 名称列。
- 本局净加成:`GetEquipBonus()` + `GetTalentEffects()` → "生命+20 / 战力+2 / 雷伤-10 / 搜索+50% / 首次免雷 / 撤离提示"(有则列)。
- 带入消耗品:`GetLoadout()` 名称×数量。
- 让玩家所见即所得(原版无此实时摘要)。

## 5. 实现要点

- 新文件 `UI/GT_DeployTerminalWidget.h/.cpp`。沿用主菜单/HUD 的 `LoadUiTexture`(LoadObject 包路径缓存防 GC)+ 9-slice Border 背景刷范式。
- 导航切换:`enum class ESection { Requisition, Loadout }`(P1 两页)+ `ShowSection` 重建中央列表;P2 再加 Warehouse/Talent/Recovery 枚举值(末尾追加,不动序号)。
- 焦点/输入:Open 时 SetKeyboardFocus;鼠标点击为主(按钮 OnClicked 委托);键盘上下/Enter 可选(P1 先鼠标,够用)。
- 改 `GT_MainMenuWidget`:加 `OnDeployRequested` 委托 + ConfirmSelection 里「装备天赋」项触发(实现时确认该项 index);去掉它的"未开放"提示分支。
- 改 `GT_GameHudWidget`:持有 `GT_DeployTerminalWidget` 成员,BuildWidgetTree 创建(层级在 MainMenu 之上),绑 MainMenu->OnDeployRequested = 打开终端;终端返回回调 = 关终端开主菜单;终端出发回调 = 复用 HandleMenuStartRequested。
- 不改 S1~S3 后端 / 163 夹具 / UGT_ItemDef。

## 6. 验证(UI 需 PIE / 截图, 无法无头)

1. **[BUILD]** 0 错;163 全过(UI 不碰局内)。
2. **截图验证**(临时改 NativeConstruct 直开终端 → Build → UnrealEditor -game -windowed -ExecCmds=gt.HUD → PrintWindow 截图 → 目检布局/素材/中文不截断),或交豪豪 PIE。
3. **PIE 交互**(豪豪): gt.HUD → 主菜单点装备天赋 → 终端打开;申领页买 armor(金币减、变已购)、装备 armor/whetstone(描 highlight、第 3 件禁用)、止血贴带入 ±;摘要实时显示 生命+20/战力+2;点出发 → 开局后 gt.Bag 应为 Hp120/Power12 + 带入消耗品(串起 S3)。返回回主菜单。

## 7. 路线图位置
S1 状态(done) → S2 结算(done) → S3 开局应用(done) → **S5 P1 部署终端外壳+商店+装备(本文档)** → S5 P2(仓库/天赋/抢救页) → S4 tradePrice 决策 → S6 新机制物品。
