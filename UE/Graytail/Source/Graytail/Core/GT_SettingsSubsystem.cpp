#include "Core/GT_SettingsSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* GTSettingsSection = TEXT("Graytail.Settings");
	const TCHAR* GTMusicAssetPath = TEXT("/Game/Graytail/Audio/Hero_Immortal");
}

void UGT_SettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadConfig();
	// 资源可能尚未导入(返回 null 则静默无 BGM, 不报错)。
	MusicSound = LoadObject<USoundBase>(nullptr, GTMusicAssetPath);
}

void UGT_SettingsSubsystem::Deinitialize()
{
	if (MusicComponent)
	{
		MusicComponent->Stop();
		MusicComponent = nullptr;
	}
	Super::Deinitialize();
}

void UGT_SettingsSubsystem::StartMusicIfNeeded(UWorld* World)
{
	if (!World || !MusicSound)
	{
		return;
	}
	if (MusicComponent && MusicComponent->IsPlaying())
	{
		return;   // 幂等: 已在播则不重开(防 PIE/重进重播)
	}
	// 持久 BGM: 不随关卡切换销毁(本作单一持久 world, 全 UMG 切换无 OpenLevel)。
	// 循环依赖 USoundWave 导入时的 bLooping 标志。
	MusicComponent = UGameplayStatics::SpawnSound2D(
		World, MusicSound, MusicVolume, 1.f, 0.f, nullptr,
		/*bPersistAcrossLevelTransition*/ false, /*bAutoDestroy*/ false);
	if (MusicComponent)
	{
		MusicComponent->SetVolumeMultiplier(MusicVolume);
	}
}

void UGT_SettingsSubsystem::SetMusicVolume(float Volume)
{
	MusicVolume = FMath::Clamp(Volume, 0.f, 1.f);
	if (MusicComponent)
	{
		MusicComponent->SetVolumeMultiplier(MusicVolume);
	}
	SaveConfig();
}

void UGT_SettingsSubsystem::LoadConfig()
{
	float Value = 0.5f;
	if (GConfig && GConfig->GetFloat(GTSettingsSection, TEXT("MusicVolume"), Value, GGameUserSettingsIni))
	{
		MusicVolume = FMath::Clamp(Value, 0.f, 1.f);
	}
}

void UGT_SettingsSubsystem::SaveConfig() const
{
	if (GConfig)
	{
		GConfig->SetFloat(GTSettingsSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
