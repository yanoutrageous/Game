#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GT_SettingsSubsystem.generated.h"

class USoundBase;
class UAudioComponent;

// 玩家偏好/音频子系统(GameInstance 级常驻, 跨菜单/界面)。
// 负责 BGM 循环播放 + 主音量, 音量持久化到 GameUserSettings.ini 的 [Graytail.Settings] 段。
// 显示设置(分辨率/全屏/垂同)走引擎 UGameUserSettings, 不在此处。
UCLASS()
class GRAYTAIL_API UGT_SettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 有 world 时幂等启动 BGM 循环播放(主菜单/HUD 构造时调一次)。
	void StartMusicIfNeeded(UWorld* World);

	// 主音量 0..1: 实时作用于 BGM + 写盘。
	void SetMusicVolume(float Volume);
	float GetMusicVolume() const { return MusicVolume; }

	// 一次性音效: 按 Key 播放 /Game/Graytail/Audio/SFX/<Key>(首用缓存), 走 SfxVolume。
	void PlaySfx(const UObject* WorldContext, FName Key);
	void SetSfxVolume(float Volume);
	float GetSfxVolume() const { return SfxVolume; }

private:
	void LoadConfig();
	void SaveConfig() const;

	UPROPERTY(Transient) USoundBase* MusicSound = nullptr;
	UPROPERTY(Transient) UAudioComponent* MusicComponent = nullptr;
	UPROPERTY(Transient) TMap<FName, USoundBase*> SfxCache;   // Key -> 已加载音效(防 GC)
	float MusicVolume = 0.5f;   // 对齐 Lua 原版 bgm gain 0.5
	float SfxVolume = 0.7f;
};
