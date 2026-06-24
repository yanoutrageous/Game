#pragma once

#include "CoreMinimal.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateBrush.h"
#include "Components/Border.h"
#include "Misc/PackageName.h"

// 全 UI 共用的中文像素字体(FusionPixel)。
// 引擎默认 Roboto/RobotoMono 无中文字形, 中文走平滑矢量回退字, 在像素美术 + 小字号下显糊;
// 改用仓库自带、经 _import_fusionpixel_font.py 导入的 FusionPixel 像素中文字体, 笔画清晰且贴合像素风。
//
// 实现说明: 引擎 Python 没暴露 FFontData/FTypeface 内层结构, 无法在导入脚本里把 UFontFace 接进
// UFont 资产; 故只导入 UFontFace, 在 C++ 这里用 FStandaloneCompositeFont 运行时包一层。
// 复合字体非 UObject, 不会把 UFontFace 计入 GC, 故首次加载后 AddToRoot 让字体常驻(单例)。
// 字体未导入时优雅回退到引擎默认字体, 不崩 —— 换字与导入解耦, 缺资产也能编译运行。
namespace GT_UIStyle
{
	inline FSlateFontInfo Font(int32 Size)
	{
		static TSharedPtr<FStandaloneCompositeFont> CompositeFont;
		if (!CompositeFont.IsValid())
		{
			if (UFontFace* Face = LoadObject<UFontFace>(nullptr, TEXT("/Game/Graytail/UI/Fonts/FusionPixel_Face.FusionPixel_Face")))
			{
				Face->AddToRoot();
				CompositeFont = MakeShared<FStandaloneCompositeFont>();
				FTypefaceEntry Entry;
				Entry.Name = FName(TEXT("Default"));
				Entry.Font = FFontData(Face);
				CompositeFont->DefaultTypeface.Fonts.Add(MoveTemp(Entry));
			}
		}
		if (CompositeFont.IsValid())
		{
			return FSlateFontInfo(CompositeFont, Size);
		}
		return FCoreStyle::GetDefaultFontStyle("Regular", Size);
	}

	// 深色圆角按钮样式。
	// 引擎默认 UButton 是浅灰白底, 我们的菜单/设置/作弊面板用浅色文字, 压上去对比极低 = 看不清;
	// 统一套这个深底(圆角 + 细描边), 文字色仍由各按钮自定, 保证可读。各处 SetStyle(DarkButton()) 即可。
	inline FButtonStyle DarkButton()
	{
		auto Brush = [](const FLinearColor& Fill, const FLinearColor& Outline)
		{
			FSlateBrush B;
			B.DrawAs = ESlateBrushDrawType::RoundedBox;
			B.TintColor = FSlateColor(Fill);
			B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			B.OutlineSettings.CornerRadii = FVector4(6.f, 6.f, 6.f, 6.f);
			B.OutlineSettings.Color = FSlateColor(Outline);
			B.OutlineSettings.Width = 1.f;
			return B;
		};
		// FColor 包一层 = 按所见 sRGB 取色(FLinearColor(FColor) 会转线性, 渲染再转回, 显示即此值)。
		const FLinearColor Edge(FColor(92, 104, 130));
		FButtonStyle S;
		S.Normal = Brush(FLinearColor(FColor(38, 44, 58)), Edge);
		S.Hovered = Brush(FLinearColor(FColor(56, 64, 84)), Edge);
		S.Pressed = Brush(FLinearColor(FColor(28, 33, 44)), Edge);
		S.Disabled = Brush(FLinearColor(FColor(30, 34, 42)), FLinearColor(FColor(52, 57, 68)));
		S.NormalPadding = FMargin(14.f, 7.f);
		S.PressedPadding = FMargin(14.f, 7.f);
		return S;
	}

	// 弹窗/卡片换皮用的中性金属框(从组员框去饱和+对比重映射, 离线烤色相)。
	// 运行时按面板直接换对应色版(不靠 multiply tint -> 不踩绿底染不出金/色彩空间的坑, 所见即所得)。
	// 5 档稀有度框(白蓝紫金红)+ 中性(设置/暂停)+ 铜(事件)。
	inline const TCHAR* PanelDialogSkin() { return TEXT("/Game/Graytail/UI/common/ui_panel_metal_neutral"); }  // 设置/暂停/战利品-common 白
	inline const TCHAR* PanelSkinRare()   { return TEXT("/Game/Graytail/UI/common/ui_panel_metal_rare"); }     // 蓝 rare
	inline const TCHAR* PanelSkinEpic()   { return TEXT("/Game/Graytail/UI/common/ui_panel_metal_epic"); }     // 紫 epic
	inline const TCHAR* PanelSkinGold()   { return TEXT("/Game/Graytail/UI/common/ui_panel_metal_gold"); }     // 金 legendary
	inline const TCHAR* PanelSkinMythic() { return TEXT("/Game/Graytail/UI/common/ui_panel_metal_mythic"); }   // 红 mythic
	inline const TCHAR* PanelSkinCopper() { return TEXT("/Game/Graytail/UI/common/ui_panel_metal_copper"); }   // 事件

	// 把组员的边框贴图作为面板背景刷(9-slice): 四角原始像素不拉伸、只拉中段, 面板与贴图长宽比差异大也不变形。
	// 调用方传外层 UBorder(自身仍保留纯色 SetBrushColor 作 fallback); 贴图缺失时静默返回, 不崩。
	// MarginFrac = 边框占贴图的比例(0.2 ≈ 金属角占两成); Opacity 给整图叠一层透明度。
	inline void SkinPanel9(UBorder* Panel, const TCHAR* AssetPath, float MarginFrac = 0.2f, float Opacity = 1.f)
	{
		if (!Panel) { return; }
		const FString Path(AssetPath);
		const FString ObjectPath = Path + TEXT(".") + FPackageName::GetShortName(Path);
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
		if (!Texture) { return; }   // 缺资产: 保留调用方已设的纯色底
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = FMargin(MarginFrac);
		Panel->SetBrush(Brush);
		Panel->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, Opacity));
	}
}
