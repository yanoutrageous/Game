#pragma once

/**
 * 物品系统示例与测试代码
 * 这是一个参考实现，展示如何在游戏中使用物品系统
 * 
 * 使用方法：
 * 1. 创建一个 Actor 类，继承此类
 * 2. 在 BeginPlay 中调用初始化方法
 * 3. 通过 PIE 或控制台测试各项功能
 */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemSystem/GraytailItemSystem.h"
#include "GT_ItemSystemExample.generated.h"

class UInventory;
class UVault;

UCLASS()
class GRAYTAIL_API AGT_ItemSystemExample : public AActor
{
	GENERATED_BODY()

public:
	AGT_ItemSystemExample();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ========== 测试方法 ==========

	/**
	 * 初始化系统
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem|Test")
	void InitializeItemSystem();

	/**
	 * 测试物品创建
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem|Test")
	void TestItemCreation();

	/**
	 * 测试背包操作
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem|Test")
	void TestInventoryOperations();

	/**
	 * 测试堆叠合并
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem|Test")
	void TestStackMerging();

	/**
	 * 测试消耗品使用
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem|Test")
	void TestConsumableUsage();

	/**
	 * 测试装备系统
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem|Test")
	void TestEquipmentSystem();

	/**
	 * 测试仓库操作
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem|Test")
	void TestVaultOperations();

	/**
	 * 测试物品效果触发
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem|Test")
	void TestItemEffectTrigger();

	/**
	 * 测试金币系统
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem|Test")
	void TestCurrencySystem();

	/**
	 * 测试Run生命周期
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem|Test")
	void TestRunLifecycle();

	/**
	 * 打印所有测试结果
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem|Test")
	void PrintTestResults();

protected:
	// 物品系统管理器
	UPROPERTY(VisibleAnywhere, Category = "ItemSystem|Test")
	UItemSystemManager* ItemSystemManager = nullptr;

	// 测试用背包
	UPROPERTY(VisibleAnywhere, Category = "ItemSystem|Test")
	UInventory* TestInventory = nullptr;

	// 测试结果
	UPROPERTY(VisibleAnywhere, Category = "ItemSystem|Test")
	TArray<FString> TestResults;

	/**
	 * 加载物品数据表
	 */
	bool LoadItemDataTable();

	/**
	 * 记录测试结果
	 */
	void RecordTestResult(const FString& TestName, bool bPassed, const FString& Details = TEXT(""));

	/**
	 * 输出日志
	 */
	void LogTest(const FString& Message);
};
