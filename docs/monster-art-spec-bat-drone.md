# 新怪美术素材规格 — 蝙蝠 / 无人机 (bat & drone)

> 给图像模型（即梦 / 通义万相 / SD+像素LoRA）用的逐项规格 + prompt。生成后按 §4 命名丢进 `assets/` 跑导入脚本，代码接线我来做。
> 硬约束沿用 `art-asset-plan.md` §1：粗像素、黑描边、4–8色 flat cel、**无抗锯齿**、**透明 PNG**、俯视略正面、序列帧用**横向 sprite sheet**。

## 0. 渲染现实（决定了帧数/尺寸，别偏离）

| 项 | 代码事实 | 对美术的约束 |
|---|---|---|
| 怪物身体 | `EnemyImage` 画布显示 **80×80**，单张静态帧，当前靠代码染色区分怪种 | 源做 **128×128**；你给真彩图后我**去掉染色**（TintColor 设白） |
| 死亡碎裂 | 5 帧独立文件 `*_shatter_0..4`，源 128²，显示 80×80，0.09s/帧 | 死亡必须 **5 帧 128²**（横向 sheet 640×128，我来切） |
| 蝙蝠散弹 | 14×14 纯色粉点 ×5（无贴图） | 升级成 **32×32** 小弹丸 |
| 无人机激光 | 800×14 纯色蓝条（无贴图） | 升级成可横向平铺的 **128×32 光束段** + 起手/命中闪光 |

配色基准（请把这两套色烧进贴图，方便丢掉占位染色）：
- **蝙蝠**：紫色系 `#B866FF`（RGB 0.72,0.40,1.0）为主，深紫描边。
- **无人机**：蓝灰金属 `#669EEB`（RGB 0.40,0.62,0.92）+ 锈迹暗部，镜头红光点缀。

## 1. 必给的参考图（喂 AI 当风格锚 / img2img）

每次生成都附上 **A**（统一像素颗粒度/描边/明暗阶），死亡动画再加 **B**（统一碎裂观感/帧数节奏）：

- **A 风格锚（必带）**：`assets/Textures/enemy_slime.png`
- **B 死亡锚（做死亡帧时带）**：`assets/Textures/generated/effects/slime_shatter_0.png` … `_4.png`
- **C 拟人调性（蝙蝠可带）**：`assets/Textures/generated/characters/huanxiong/frames/00_front_idle.png`（"灰尾"动物拟人风）

通用正向词：`pixel art, chunky pixels, bold dark outline, limited palette, flat cel shading, no anti-aliasing, transparent background, top-down 3/4 view, centered, game sprite`
通用负向词：`blurry, soft shading, gradient, realistic, photo, anti-aliasing, jpeg artifacts, drop shadow, white background`

---

## 2. 蝙蝠 (bat) — 快、脆、朝玩家方向打 3 发散弹

紫色拟人卡通蝙蝠，体型小、动作敏捷。共 4 类素材：

### 2.1 待机/飞行 sheet（4 帧，512×128）
- **内容**：原地振翅循环（翅膀展开↔收拢），轻微上下浮动。
- **中文**：`像素游戏怪物，紫色卡通蝙蝠原地振翅飞行动画，共4帧横向排列，翅膀从展开到收拢循环；粗像素卡通风，深紫黑色描边，4-6色有限调色板，俯视略正面，每帧透明背景，硬边无抗锯齿，与参考史莱姆相同的像素颗粒度。`
- **EN**：`pixel art monster sprite sheet, 4 frames horizontal, purple cartoon bat flapping wings in place, wings open to closed loop, chunky pixels, bold dark-purple outline, limited palette, top-down 3/4 view, transparent background, hard edges, matching reference slime pixel scale`
- 若 AI 4 帧难保持一致 → 退 **2 帧 256×128**（展翅 / 收翅）也能用。

### 2.2 蓄力+发射（2 帧，256×128）
- **内容**：帧0 后仰、张嘴蓄能发紫光（预警）；帧1 前冲张大嘴喷射（发射瞬间）。替代当前"身体闪烁"预警。
- **中文**：`像素游戏怪物动作图，紫色卡通蝙蝠攻击动作2帧：第一帧后仰张嘴聚集紫色能量蓄力，第二帧前冲张大嘴发射；粗像素卡通风，深紫黑色描边，透明背景，硬边无抗锯齿，俯视略正面。`
- **EN**：`pixel art bat attack frames, 2 frames: frame1 leaning back mouth open charging purple energy, frame2 lunging forward mouth wide firing, chunky pixels, bold outline, transparent background, hard edges, top-down 3/4 view`

### 2.3 死亡碎裂 sheet（5 帧，640×128）★复用碎裂管线
- **内容**：完整→裂成紫色方块碎片+翅膜碎屑四散→消散。**务必 5 帧**，节奏对齐参考 B。
- **中文**：`像素游戏死亡碎裂动画，紫色卡通蝙蝠裂成方块状像素碎片和翅膜碎屑向四周飞溅最后消散，共5帧横向排列；保留深紫黑色描边与紫色配色；每帧透明背景，硬边无抗锯齿，俯视略正面，碎裂节奏参考所给史莱姆碎裂图。`
- **EN**：`pixel art death shatter animation, 5 frames horizontal, purple cartoon bat breaking into chunky pixel shards and wing-membrane gibs flying outward then fading, dark-purple outline, transparent background, hard edges, matching the reference slime shatter timing`

### 2.4 散弹弹丸（32×32，1–2 帧）
- **内容**：小颗紫色能量弹/音波尖刺，带一点拖尾感。2 帧可做闪烁。
- **中文**：`像素游戏子弹图标，小颗紫色能量弹丸/尖刺，发光，单个居中，粗像素卡通风，黑描边，透明背景，硬边无抗锯齿。`
- **EN**：`pixel art projectile, small glowing purple energy bolt/spike, centered, chunky pixels, bold outline, transparent background, hard edges`

---

## 3. 无人机 (drone) — 慢、肉、正对玩家放持续激光

蓝灰锈蚀的悬浮巡检机械，单红色镜头眼（**机械体，是动物拟人主题的有意例外**）。共 4 类素材：

### 3.1 悬停 sheet（4 帧，512×128）
- **内容**：悬停轻微上下浮动，底部旋翼模糊，机身状态灯/镜头红光呼吸闪烁。
- **中文**：`像素游戏怪物，蓝灰色锈蚀悬浮巡检无人机，单个红色镜头眼，悬停待机动画共4帧横向排列，底部旋翼旋转模糊+机身红色指示灯呼吸闪烁+轻微上下浮动；粗像素卡通风，黑色描边，4-6色有限调色板，俯视略正面，每帧透明背景，硬边无抗锯齿。`
- **EN**：`pixel art monster sprite sheet, 4 frames horizontal, blue-grey rusty hovering inspection drone with single red camera eye, idle hover with spinning rotor blur and blinking red indicator light, chunky pixels, bold dark outline, limited palette, top-down 3/4 view, transparent background, hard edges`
- 退路同上：2 帧（灯亮/灯灭）也能用。

### 3.2 激光蓄力（3 帧，384×128）
- **内容**：镜头从蓝→红逐帧增亮聚能，机身微震，发射前一帧最亮。配合"红线瞄准"预警。
- **中文**：`像素游戏机械蓄力动作3帧，蓝灰无人机的红色镜头眼逐帧从微亮到刺眼地聚集激光能量，机身轻微震动；粗像素卡通风，黑色描边，透明背景，硬边无抗锯齿，俯视略正面。`
- **EN**：`pixel art drone charging laser, 3 frames, red camera eye charging from dim to blinding glow, body trembling, chunky pixels, bold outline, transparent background, hard edges, top-down 3/4 view`

### 3.3 死亡爆裂 sheet（5 帧，640×128）★复用碎裂管线
- **内容**：完整→金属碎片+火花迸射+小爆炸团→冒烟残骸消散。**5 帧**，节奏对齐参考 B。
- **中文**：`像素游戏死亡爆裂动画，蓝灰无人机被炸成金属碎片并迸射火花和小型爆炸火光最后冒烟消散，共5帧横向排列；粗像素卡通风，黑色描边，每帧透明背景，硬边无抗锯齿，俯视略正面，节奏参考所给史莱姆碎裂图。`
- **EN**：`pixel art death explosion, 5 frames horizontal, blue-grey drone bursting into metal shards with sparks and a small fiery blast then smoking debris fading, chunky pixels, bold outline, transparent background, hard edges, matching reference shatter timing`

### 3.4 激光素材（3 件）
- **光束段 128×32（可横向平铺）**：亮青核心+蓝外辉，左右无缝拼接。
  `像素游戏激光光束横向贴图，亮青色核心加蓝色外辉光，可左右无缝平铺，水平，透明背景，硬边像素风，无抗锯齿。` / `pixel art horizontal laser beam, bright cyan core with blue outer glow, seamlessly tileable horizontally, transparent background, hard pixel edges`
- **枪口起手闪光 48×48（1 帧）**：镜头处的青白爆闪。
- **命中冲击点 48×48（2–3 帧）**：打在目标上的灼烧火花小爆点。
  `像素游戏激光命中冲击点2-3帧，青白色火花灼烧小爆点，居中，透明背景，硬边像素风。`

---

## 4. 命名与上线（生成后丢这里，导入我来跑）

放进 `assets/Textures/generated/` 对应子目录，文件名小写：

```
怪物身体 sheet      monsters/bat_idle_sheet.png        (512×128, 4帧)
                    monsters/drone_idle_sheet.png
攻击/蓄力 sheet     monsters/bat_attack_sheet.png      (256×128)
                    monsters/drone_charge_sheet.png    (384×128)
死亡碎裂 sheet       effects/bat_shatter_sheet.png      (640×128, 5帧) → 我切成 bat_shatter_0..4
                    effects/drone_shatter_sheet.png    (640×128, 5帧) → drone_shatter_0..4
弹丸/光束           effects/bat_bolt.png               (32×32)
                    effects/drone_laser_beam.png       (128×32 平铺)
                    effects/drone_laser_muzzle.png     (48×48)
                    effects/drone_laser_impact.png     (48×48)
```

- 一张横向 sheet 我用脚本切成等宽帧，键透明（你只要保证整图透明背景、帧等间距居中）。
- 二进制走 LFS（已配）；编辑器自改的 `.uproject`/`Config/*.ini` 不提交。

## 5. 优先级（按"丢掉占位"的收益排）

| 优先 | 素材 | 收益 |
|---|---|---|
| ★1 | 两怪 **身体单帧**（idle sheet 取第1帧即可先上） | 立刻摆脱"染色史莱姆" |
| ★2 | 两怪 **死亡 5 帧**（bat 紫羽 / drone 火花，**不再共用洋红 slime 碎裂**） | 击杀反馈贴合怪种 |
| ★3 | **散弹/激光贴图**（替纯色图元） | 远程攻击辨识度 |
| 4 | idle 4 帧振翅/悬停 + 蓄力帧 | 待机/起手动画，juice |
| 5 | 枪口/命中闪光、飞行阴影 | 锦上添花 |

## 6. "还有什么" —— 我额外建议补的（你问的）

1. **每怪独立死亡特效**：现死亡固定播洋红 slime 碎裂；机器人碎成洋红史莱姆很出戏。→ 已在 §2.3/§3.3 拆成各自的，**强烈建议做**。
2. **飞行阴影**：蝙蝠/无人机脚下一坨半透明软椭圆（64×24），卖"悬空"感。可纯代码画，也可给 1 张图。
3. **枪口/镜头起手闪光**：发射瞬间嘴边/镜头一下爆闪，比单纯弹丸飞出去更有打击感（§3.4 已含无人机版，蝙蝠可加 32×32 紫闪）。
4. **激光命中玩家的灼烧点**（§3.4），比"血条掉数字"更直观。
5. **受击帧 / 命中闪白**：怪挨打时白闪——这个**纯代码就能做**，不必出图。
6. **入场演出**（可选低优）：蝙蝠从上俯冲入场 / 无人机通电亮灯启动，各 3 帧；不做也不影响。
7. **弹道拖尾/残影**：纯代码，无需素材。

> 不需要出的：受击闪白、阴影、拖尾、踩雷爆点、拾取飘字——这些程序化更省，列在这里只为你心里有数。
