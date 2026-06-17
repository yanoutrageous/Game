#pragma once

#include "CoreMinimal.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"

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
}
