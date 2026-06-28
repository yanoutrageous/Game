# 怪物美术素材规格 — 绿史莱姆 + 小史莱姆 (slime & slimeling)

> 给图像模型(即梦 / 通义万相 / SD+像素 LoRA)用的逐项规格 + prompt。生成后按 §4 命名丢进 `assets/`,导入我来跑、代码接线我来做。
> 硬约束沿用 `art-asset-plan.md` §1 / `monster-art-spec-bat-drone.md`:粗像素、黑(深绿黑)描边、4–8 色 flat cel、**无抗锯齿**、**透明 PNG**、俯视略正面、序列帧用**横向 sprite sheet**。
> 背景:史莱姆是三怪里**唯一还在用洋红占位图**的。它要改**绿色**——而绿色**没法靠代码染色**(占位是洋红,绿通道≈0,乘绿 tint 变 muddy),所以**必须出真·绿色贴图**。多数是**同名覆盖**,导入即生效、零改码。

## 0. 渲染现实(决定帧数/尺寸,别偏离)

| 项 | 代码事实 | 对美术的约束 |
|---|---|---|
| 史莱姆本体 | `EnemyImage` 画布显示 **80×80**,**单张静态帧**(史莱姆不做待机帧动画) | 源做 **128×128 单帧**;直接覆盖 `enemy_slime.png` 即生效 |
| 小史莱姆(子体) | 同一套渲染,代码再**缩放 ~0.7**(显示 ~56×56) | 源也做 **128×128 单帧**;给独立图更好,不给就复用母体缩小 |
| 死亡碎裂 | 5 帧独立文件 `slime_shatter_0..4`,源 128²,显示 80×80,0.09s/帧 | 死亡必须 **5 帧 128²**(横向 sheet 640×128,我来切) |

**绿色配色基准**(把绿烧进贴图,代码 TintColor 保持 White):
- 主体亮绿 `#5BD64A`(RGB 0.36,0.84,0.29),暗部绿 `#3AA82E`,高光浅绿 `#9CF08A`,描边深绿近黑 `#163E12`。
- 眼睛/嘴用深色(近黑或深绿),保持"史莱姆"的呆萌或凶相皆可(见 §1 参考)。

## 1. 必给的参考图(喂 AI 当风格锚 / img2img)

- **A 风格+造型锚(必带,直接 img2img)**:`assets/Textures/enemy_slime.png` —— **就是现在那只洋红愤怒小肉团**。保留它的**形状、表情、像素颗粒度、描边粗细、俯视角度**,**只把洋红整体替换成上面的绿色系**。这是最省事、最稳的做法。
- **B 死亡锚(做死亡帧时带)**:`assets/Textures/generated/effects/slime_shatter_0.png … _4.png` —— 现有洋红碎裂 5 帧。保留**碎裂造型与帧节奏**,把碎片**改成绿色系 + 深绿黑描边**。

通用正向词:`pixel art, chunky pixels, bold dark outline, limited palette, flat cel shading, no anti-aliasing, transparent background, top-down 3/4 view, centered, game sprite`
通用负向词:`blurry, soft shading, gradient, realistic, photo, anti-aliasing, jpeg artifacts, drop shadow, white background`

---

## 2. 绿史莱姆本体(母体) — 128×128 单帧 ★最高优先

绿色果冻质感小肉团,和参考 A 同款造型/表情,只换绿色。母体死亡时会裂成 2 只小史莱姆。
- **中文**:`像素游戏怪物,绿色果冻史莱姆小肉团,半透明果冻质感,顶部一点高光,与参考图同样的形状/表情/像素颗粒度/描边,只把洋红配色整体换成鲜亮的史莱姆绿(主体亮绿+暗部深绿+浅绿高光),深绿近黑粗描边,4-6色有限调色板,俯视略正面,居中,透明背景,硬边无抗锯齿。`
- **EN**:`pixel art monster, green jelly slime blob, translucent jelly look with a top highlight, same shape/expression/pixel scale/outline as the reference, recolor the magenta into a vivid slime green (bright green body, dark green shadow, pale green highlight), bold dark-green near-black outline, limited 4-6 color palette, top-down 3/4 view, centered, transparent background, hard edges, no anti-aliasing`
- 表情:沿用参考的凶相即可(它是会追人咬人的近战怪);想要呆萌版也行,你定。

## 3. 小史莱姆 / 子体(slimeling) — 128×128 单帧(可选)

母体死后分裂出的小号史莱姆,更小、更快、更弱。**可选**:不给就用母体贴图缩小 0.7 顶上;给独立图更有辨识度。
- **建议差异**:比母体**更圆更小一坨**,表情更"贼"/更急(快速乱窜的感觉),颜色稍浅一点的绿以便和母体区分。
- **中文**:`像素游戏怪物,小一号的绿色果冻史莱姆幼体,比普通史莱姆更圆更小一坨,半透明果冻质感,机灵/急躁的小表情,稍浅的史莱姆绿,深绿近黑粗描边,4-6色有限调色板,俯视略正面,居中,透明背景,硬边无抗锯齿,与参考史莱姆相同的像素颗粒度。`
- **EN**:`pixel art monster, smaller green jelly slimeling, rounder and tinier than a normal slime, translucent jelly look, a quick/cheeky little face, slightly lighter slime green, bold dark-green near-black outline, limited palette, top-down 3/4 view, centered, transparent background, hard edges, matching reference slime pixel scale`

## 4. 绿死亡碎裂 sheet(5 帧,640×128)★复用碎裂管线

绿史莱姆被打爆:完整 → 裂成绿色果冻方块碎片 + 黏液飞溅 → 四散消散。**务必 5 帧**,节奏对齐参考 B。
- **中文**:`像素游戏死亡碎裂动画,绿色果冻史莱姆裂成方块状像素碎片和绿色黏液飞沫向四周飞溅最后消散,共5帧横向排列;保留深绿黑描边与史莱姆绿配色;每帧透明背景,硬边无抗锯齿,俯视略正面,碎裂节奏参考所给洋红史莱姆碎裂图。`
- **EN**:`pixel art death shatter animation, 5 frames horizontal, green jelly slime breaking into chunky pixel shards and green slime-goo splatter flying outward then fading, dark-green near-black outline, slime-green palette, transparent background, hard edges, matching the reference magenta slime shatter timing`

---

## 5. 攻击动画(蓄力 + 扑击,2 帧 256×128)— 给史莱姆补本体攻击动作

蝙蝠/无人机都有本体攻击帧,史莱姆现在近战只有地面预警圈 + 身体抖动、本体没动作。补 2 帧,套现有近战相位机(**warning 相 = 蓄力帧,active 相 = 扑击帧**),母体与小史莱姆共用(子体缩 0.7)。
- 帧0 **蓄力/下蹲**(warning):史莱姆向下压扁、绷紧攒劲、略后仰(预警"要扑")。
- 帧1 **扑击/猛胀**(active):向前猛地拉伸鼓胀扑出去(命中瞬间),身体最舒展。
- 与本体**同款绿色** + 同描边/像素颗粒度;贴图源**朝右**(代码按朝向水平翻转面向玩家)。
- **中文**:`像素游戏怪物攻击动作2帧横向排列,绿色果冻史莱姆:第一帧向下压扁蓄力绷紧略后仰,第二帧向前猛地拉伸鼓胀扑击;与本体同款史莱姆绿+深绿黑描边,粗像素卡通风,透明背景,硬边无抗锯齿,俯视略正面,源朝右。`
- **EN**:`pixel art monster attack, 2 frames horizontal, green jelly slime: frame1 squashing down winding up tense leaning back, frame2 stretching and bulging forward lunging to strike; same slime green + dark-green outline as the body, chunky pixels, transparent background, hard edges, top-down 3/4 view, facing right`
- 退路:只给 1 帧"扑击"也能用(蓄力沿用现有身体抖动)。**代码接线我在 A 之后做**。

## 6. 命名与上线(生成后丢这里,导入我来跑)

| 资产 | 路径 / 文件名 | 规格 | 上线方式 |
|---|---|---|---|
| 绿史莱姆本体 | `assets/Textures/enemy_slime.png`(**同名覆盖**) | 128×128 单帧 | 重导即生效,**零改码** |
| 小史莱姆(可选) | `assets/Textures/generated/monsters/enemy_slimeling.png`(新) | 128×128 单帧 | 我接 SpritePath(不给则母体缩 0.7) |
| 绿死亡碎裂 | `assets/Textures/generated/effects/slime_shatter_sheet.png`(640×128, 5帧)→ 我切成 `slime_shatter_0..4`(**同名覆盖**) | 横向 5 帧等宽 | 重导即生效,**零改码** |
| 攻击动画(可选) | `assets/Textures/generated/monsters/slime_attack_sheet.png`(256×128, 2帧) | 横向 2 帧等宽(蓄力/扑击) | 我切帧 + 在 A 后接线 |

- 一张横向 sheet 我用脚本切等宽帧、键透明(你只要保证整图透明背景、帧等间距居中)。
- 二进制走 LFS(已配);编辑器自改的 `.uproject`/`Config/*.ini` 不提交。

## 7. 优先级(按收益排)
| 优先 | 素材 | 收益 |
|---|---|---|
| ★1 | **绿史莱姆本体**(覆盖 enemy_slime) | 直接实现"绿色"诉求,零改码 |
| ★2 | **绿死亡碎裂 5 帧** | 死亡动画也绿(你明确要的),零改码 |
| 3 | **小史莱姆独立图** | 子体辨识度;不给可用母体缩小顶替 |
| 4 | **攻击 2 帧**(蓄力+扑击) | 近战手感/juice,与蝙蝠/无人机看齐 |

> 不必出的(程序化更省):子体缩小、受击闪白、阴影、分裂飞溅拖尾——这些代码做。绿色是唯一**必须出图**的,因为洋红 tint 不成。
