# 灰尾回收 — 美术素材规划 (art-asset-plan)

> 用途：把"游戏里需要补/改的美术"一次性列清，每项给 **尺寸 / 帧数 / 画风 / 格式 / AI 生成 prompt / 需提供的参考图**，
> 直接拿去喂图像模型（即梦 / 通义万相 / SD+像素LoRA），生成后按第 4 节命名→丢进 `assets/`→跑导入脚本即可上线。
> 文本 LLM（DeepSeek/GLM/Kimi）生不了图；像素 SFX/简单 VFX 可程序化纯代码合成，复杂精灵/序列帧才需图像模型。

---

## 1. 统一画风规格（所有新素材的硬约束）

游戏世界（角色/怪物/道具/地板/图标）是 **粗像素卡通风**；务必让新素材与此一致，否则混搭出戏。
基准参考物：`assets/Textures/enemy_slime.png`（怪物）、`assets/Textures/generated/characters/huanxiong/`（角色）。

| 维度 | 规格 |
|---|---|
| 画风 | 粗像素（chunky pixel art），逻辑分辨率约 32–48px 放大到目标尺寸，硬边、**无抗锯齿** |
| 描边 | 清晰深色外轮廓（1–2 逻辑像素），卡通造型、夸张表情 |
| 调色板 | 每个对象 4–8 色，2–3 阶明暗（flat cel shading），避免渐变 |
| 背景 | **透明 PNG（RGBA / PNG-32）**，单个对象居中、留 ~8% 安全边 |
| 视角 | 角色/怪物 = 俯视略带正面（3/4 top-down），与玩家朝向体系一致；道具/地板 = 俯视 |
| 主题 | **动物拟人**（游戏名"灰尾"，现有角色为浣熊/狐狸/猫/兔子）。新 NPC、可爱系怪物优先贴这个调性 |
| 格式 | `.png`，sRGB，无有损压缩；序列帧用横向 sprite sheet |

**SD / 像素LoRA 通用正向词**：
`pixel art, chunky pixels, bold dark outline, limited palette, flat cel shading, no anti-aliasing, transparent background, top-down 3/4 view, centered, game sprite`
**通用负向词**：`blurry, soft shading, gradient, realistic, photo, anti-aliasing, jpeg artifacts, drop shadow, white background`

---

## 2. 现有素材盘点（已有，作风格锚 / 复用基准）

| 类别 | 资产 | 单尺寸 | 路径（assets 源 / Content 运行时） |
|---|---|---|---|
| 玩家角色 | `huanxiong` 浣熊（**已启用**）+ `huli`狐/`mao`猫/`tuzi`兔（已生成未启用） | 128×128/帧，4列×3行 sheet=512×384 | `Textures/generated/characters/<name>/` → `Sprites/Characters/<name>/` |
| 怪物 | `enemy_slime`（**唯一**，所有怪共用） | 128×128 静态单帧 | `Textures/enemy_slime.png` → `Sprites/enemy_slime` |
| 房间地板 | room_safe/danger/exit/monster/event/treasure | （Misc 套图） | `Sprites/Misc/room_*` |
| 世界道具 props | 宝箱开/关、撤离装置暗/亮、商人台、异常核心、地刺、零件堆、扫描仪、金币堆、医疗包、物资箱（12 个） | 小件 160×160 / 大件 256×256 | `Textures/generated/props/` → `Sprites/Props/` |
| 小地图图标 | 定位/未知格/已探/扫描/旗/陷阱/怪/箱/撤离/出口/已清/数字×3（14 个） | **32×32 + 64×64 两套** | `Textures/generated/icons/{32,64}/` |
| 物品 UI 图标 | 装备/消耗/天赋/回收物图标 | （局外组美术，**写实画风**⚠） | `Content/Graytail/UI/deploy/ui_icon_*`、`Items/Recovered/*` |
| UI 杂项 | 金币、血条填充 64×64；标题背景 1672×941 | — | `Textures/generated/ui/`、`Textures/menu_bg.png` |
| 房间背景 | `fangjian_jichu_1024` | 1024×1024 | `Sprites/Rooms/` |

---

## 3. 缺口与生成清单（按优先级排）

### 3.1 ★怪物死亡碎裂特效（你点名要的）
- **用途**：击杀怪物时播放，让战斗有反馈（当前怪物死亡是瞬间消失，无演出）。
- **规格**：6 帧横向 sprite sheet，每帧 **128×128**，整图 **768×128**，逐帧透明背景。
- **画风**：复用 slime 的洋红配色与描边，裂成方块状像素碎片（gibs/shards）逐帧四散+变淡消散。
- **需提供给 AI 的参考图**：`assets/Textures/enemy_slime.png`（作 img2img / 风格锚，保证配色造型一致）。
- **Prompt（即梦/通义，中文）**：
  > 像素游戏风格，洋红色卡通史莱姆的死亡碎裂动画，共6帧横向排列；史莱姆从完整逐渐裂成方块状像素碎片并向四周飞溅、最后消散；保留黑色描边和与参考图相同的洋红配色；每帧透明背景，硬边无抗锯齿，俯视略正面视角。
- **Prompt（SD/像素LoRA，英文）**：
  > `pixel art death animation sprite sheet, 6 frames horizontal, magenta cartoon slime shattering into chunky pixel shards and gibs, debris flying outward then fading, same palette and bold outline as reference, transparent background, hard edges, top-down 3/4 view` ＋通用负向词
- **替代方案（免 AI）**：碎裂可纯代码做——把 slime 贴图按 4×4 切块各自抛物线飞散+渐隐，零素材。若要快可先上这个，AI 碎片图作后续增强。

### 3.2 新怪物种类（打破"全是史莱姆"）
- **用途**：代码框架已支持移动模式 `Stationary/ChasePlayer` × 攻击模式 `MeleeCircle/RangedProjectile`（远程接口已留），但贴图只有 slime。补 3 种即可显著丰富。
- **规格**：每种 **128×128 静态单帧**（与 slime 同规格即可直接接入 `GT_MonsterCatalog.SpritePath`）。若要走路动画再按角色 12 帧规范扩展。
- **建议 3 种**（贴异常回收/动物主题）：
  | 代号 | 造型 | 行为映射 |
  |---|---|---|
  | `enemy_drone` 巡检无人机 | 悬浮小型机械眼，红色镜头 | 远程型（RangedProjectile，红线瞄准→飞弹） |
  | `enemy_turret` 故障炮台 | 半埋地的锈蚀小炮台 | 固定型（Stationary + Ranged） |
  | `enemy_hound` 异常体犬 | 黑紫色四足兽，獠牙发光（呼应"异常体犬牙"装备） | 近战追击（ChasePlayer + MeleeCircle） |
- **参考图**：`enemy_slime.png`（统一颗粒度/描边/明暗阶数）。
- **Prompt 模板**：
  > 像素游戏怪物立绘，<造型描述>，粗像素卡通风，黑色描边，4-6色有限调色板，俯视略正面，透明背景，硬边无抗锯齿，与参考史莱姆同样的像素颗粒度。
  > EN：`pixel art monster sprite, <desc>, chunky pixels, bold dark outline, limited palette, top-down 3/4 view, transparent background, matching the reference slime's pixel scale`
- **接入成本**：每加 1 种 = `EGT_MonsterType` 末尾加枚举 + catalog 加一行原型 + 指 SpritePath，**纯代码 10 分钟**（已留好扩展点）。

### 3.3 事件房 NPC（4 类事件）
- **现状**：事件房（旅商/赌徒/祭坛/机关）目前是纯文字 `EventPanel`，地图上只有一块 `room_event` 地板 + 一个 `shangren_tai`(商人台) 道具，**没有 NPC 角色立绘**。
- **用途**：给每类事件配 1 张 NPC/场景立绘，挂在事件面板顶部，提升辨识度与氛围。
- **规格**：**256×256**（面板立绘，比 props 大一档），透明背景；动物拟人风对齐角色。
- **4 张清单**：
  | 事件 | NPC/意象 | 造型建议 |
  |---|---|---|
  | 旅商 | 流浪商人 | 披斗篷的拟人动物（如獾/狐），背大背包、提马灯 |
  | 赌徒 | 赌徒 | 叼牙签的拟人动物，手持骰子/卡牌，奸笑 |
  | 祭坛 | 神秘祭坛 | 发紫光的异常祭坛（场景物，非角色），骨骸/触手装饰 |
  | 机关 | 机关装置 | 锈蚀的机械操作台 + 红色警示灯 |
- **参考图**：`characters/huli/frames/00_front_idle.png`（角色拟人风锚）、`props/04_shangren_tai.png`（道具风锚）。
- **Prompt 模板**：
  > 像素游戏事件立绘，<NPC 描述>，动物拟人，粗像素卡通风，黑色描边，俯视略正面半身，透明背景，氛围<暗调/神秘/诡异>。

### 3.4 物品图标画风统一（修"写实混像素"）
- **问题**：装备/回收物图标是局外组**写实画风**（`GT_ItemCatalog.cpp` 注释自述），与世界像素风冲突。
- **方案**：把现有约 20 个物品图标重做成像素风（或先做最显眼的局外 10 件装备）。
- **规格**：**128×128**，透明背景，单物件居中，像素风同上。（UI 会自适应缩放，128 够清晰。）
- **清单**（装备 10）：防护背心/磨刀石/急救包/绝缘套/罗盘/大背包/异常体犬牙/封锁区结晶/公司工牌/回收磁石。
- **参考图**：现有 `props/10_yiliaobao.png`(医疗包) / `props/11_wuzi_xiang.png`(物资箱) 是像素风物品图，作风格锚。
- **Prompt 模板**：
  > 像素游戏物品图标，<物品描述>，单个物体居中，粗像素卡通风，黑色描边，4-8色，透明背景，硬边。
- **优先级**：中（不影响玩法，影响观感统一）。可放到怪物/特效之后。

### 3.5 角色皮肤启用 — `huli`/`mao`/`tuzi`（零美术，纯代码）
- 3 套动物角色 12 帧**已生成并导入**，只是没接。可做"出勤前选角色皮肤"，**不需要任何新素材**，只需在角色选择处把 `huanxiong` 的硬编码路径改为可选。低成本高感知收益。

### 3.6 可选打磨特效（多为纯代码，列出备选）
- 命中闪白 / 受击红闪 / 踩雷爆点 / 拾取金币飘字 / 撤离成功光效——这些**程序化即可**，无需 AI 素材。
- 若要精致：可生成 64×64 的 8 帧爆炸/拾取闪光 sheet（512×64），prompt 同 3.1 思路。

---

## 4. 命名与导入流程（生成后怎么上线）

1. **命名**：沿用现有拼音/英文小写 + 序号约定。
   - 怪物：`assets/Textures/<id>.png`（如 `enemy_hound.png`），单帧。
   - 角色多帧：`assets/Textures/generated/characters/<name>/frames/NN_<dir>_<state>.png` + `<name>_sheet.png`。
   - 事件立绘：`assets/Textures/generated/ui/events/<event>.png`。
   - 物品图标：`assets/Textures/generated/ui/items/<item_id>.png`。
2. **导入**：把 PNG 放进 `assets/` 后，跑 `Content/Python/import_png_assets.py`（**必须完整编辑器跑，勿加 `-unattended`**，会崩在主框架初始化；用法见脚本头注释）。会生成 `/Game/Graytail/...` 下同名 `.uasset`。
3. **接代码**：怪物→`GT_MonsterCatalog`（枚举+原型+SpritePath）；事件立绘/物品图标→对应 widget 的 `IconForXxx`/事件面板加载路径。
4. **二进制走 LFS**（`.gitattributes` 已配）；**编辑器自动改写的 `.uproject`/`Config/*.ini` 不要提交**。
5. **画风自检**：新图丢进游戏前，并排 `enemy_slime.png` 比一眼——像素颗粒度、描边粗细、明暗阶数对得上才算合格。

---

## 优先级总览（投入产出比）

| 优先 | 项 | 美术量 | 代码量 | 收益 |
|---|---|---|---|---|
| ★1 | 怪物碎裂（3.1，可先纯代码） | 1 sheet 或 0 | 小 | 战斗反馈，立竿见影 |
| ★2 | 新怪物 ×3（3.2） | 3 张 | 小（已留扩展点） | 打破单调，丰富战斗 |
| 3 | 角色皮肤启用（3.5） | 0 | 小 | 零成本选角 |
| 4 | 事件房 NPC ×4（3.3） | 4 张 | 中 | 事件辨识+氛围 |
| 5 | 物品图标统一（3.4） | ~10–20 张 | 小 | 观感统一 |
| 6 | 打磨特效（3.6，纯代码） | 0 | 中 | juice |
