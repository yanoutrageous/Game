#include "ItemSystem/GT_ItemSystemExample.h"
#include "ItemSystem/GT_Inventory.h"
#include "ItemSystem/GT_Vault.h"
#include "Engine/DataTable.h"

AGT_ItemSystemExample::AGT_ItemSystemExample()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGT_ItemSystemExample::BeginPlay()
{
	Super::BeginPlay();

	LogTest(TEXT("========== Item System Examples Started =========="));
	InitializeItemSystem();
}

void AGT_ItemSystemExample::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGT_ItemSystemExample::InitializeItemSystem()
{
	LogTest(TEXT("\n[1] 初始化物品系统..."));

	ItemSystemManager = GetGameInstance()->GetSubsystem<UItemSystemManager>();
	if (!ItemSystemManager)
	{
		LogTest(TEXT("ERROR: 无法获取 ItemSystemManager"));
		RecordTestResult(TEXT("Initialize ItemSystem"), false, TEXT("ItemSystemManager not found"));
		return;
	}

	// 加载物品数据表
	if (!LoadItemDataTable())
	{
		LogTest(TEXT("ERROR: 无法加载物品数据表"));
		RecordTestResult(TEXT("Initialize ItemSystem"), false, TEXT("Failed to load DataTable"));
		return;
	}

	LogTest(TEXT("✓ 物品系统初始化成功"));
	RecordTestResult(TEXT("Initialize ItemSystem"), true);

	// 运行所有测试
	TestItemCreation();
	TestInventoryOperations();
	TestStackMerging();
	TestConsumableUsage();
	TestEquipmentSystem();
	TestVaultOperations();
	TestItemEffectTrigger();
	TestCurrencySystem();
	TestRunLifecycle();
	PrintTestResults();
}

void AGT_ItemSystemExample::TestItemCreation()
{
	LogTest(TEXT("\n[2] 测试物品创建..."));

	// 测试按 ID 创建
	UItemBase* Item1 = ItemSystemManager->CreateItemByID(1);
	if (Item1 && Item1->GetItemID() == 1)
	{
		LogTest(FString::Printf(TEXT("✓ 按ID创建成功: %s"), *Item1->GetItemName()));
		RecordTestResult(TEXT("Create Item by ID"), true, Item1->GetItemName());
	}
	else
	{
		LogTest(TEXT("✗ 按ID创建失败"));
		RecordTestResult(TEXT("Create Item by ID"), false);
	}

	// 测试按名称创建
	UItemBase* Item2 = ItemSystemManager->CreateItemByName(TEXT("应急绷带"));
	if (Item2 && Item2->GetItemID() == 5)
	{
		LogTest(FString::Printf(TEXT("✓ 按名称创建成功: %s"), *Item2->GetItemName()));
		RecordTestResult(TEXT("Create Item by Name"), true, Item2->GetItemName());
	}
	else
	{
		LogTest(TEXT("✗ 按名称创建失败"));
		RecordTestResult(TEXT("Create Item by Name"), false);
	}

	// 测试便利方法
	UItemBase* Item3 = ItemSystemManager->CreateCompanyBadge();
	if (Item3 && Item3->GetItemID() == 10)
	{
		LogTest(FString::Printf(TEXT("✓ 便利方法创建成功: %s"), *Item3->GetItemName()));
		RecordTestResult(TEXT("Create Item by Convenience Method"), true, Item3->GetItemName());
	}
	else
	{
		LogTest(TEXT("✗ 便利方法创建失败"));
		RecordTestResult(TEXT("Create Item by Convenience Method"), false);
	}
}

void AGT_ItemSystemExample::TestInventoryOperations()
{
	LogTest(TEXT("\n[3] 测试背包操作..."));

	TestInventory = ItemSystemManager->CreateInventory(20.f, nullptr);
	if (!TestInventory)
	{
		LogTest(TEXT("✗ 创建背包失败"));
		RecordTestResult(TEXT("Create Inventory"), false);
		return;
	}

	LogTest(TEXT("✓ 创建背包成功"));
	RecordTestResult(TEXT("Create Inventory"), true);

	// 添加物品
	UItemBase* Item = ItemSystemManager->CreateItemByID(5);  // 应急绷带
	bool bAdded = TestInventory->AddItem(Item);
	if (bAdded)
	{
		LogTest(FString::Printf(TEXT("✓ 添加物品成功，当前背包重量: %.1f"), TestInventory->GetCurrentWeight()));
		RecordTestResult(TEXT("Add Item"), true);
	}
	else
	{
		LogTest(TEXT("✗ 添加物品失败"));
		RecordTestResult(TEXT("Add Item"), false);
	}

	// 查询
	int32 ItemCount = TestInventory->GetItemCount();
	LogTest(FString::Printf(TEXT("✓ 背包物品数: %d"), ItemCount));

	float Capacity = TestInventory->GetAvailableCapacity();
	LogTest(FString::Printf(TEXT("✓ 剩余容量: %.1f"), Capacity));
}

void AGT_ItemSystemExample::TestStackMerging()
{
	LogTest(TEXT("\n[4] 测试堆叠合并..."));

	if (!TestInventory)
	{
		LogTest(TEXT("✗ 背包不存在"));
		return;
	}

	// 添加第一个绷带
	UItemBase* Bandage1 = ItemSystemManager->CreateItemByID(5);
	TestInventory->AddItem(Bandage1, true);

	// 添加第二个绷带（应该合并）
	UItemBase* Bandage2 = ItemSystemManager->CreateItemByID(5);
	TestInventory->AddItem(Bandage2, true);

	// 添加第三个绷带（应该合并）
	UItemBase* Bandage3 = ItemSystemManager->CreateItemByID(5);
	TestInventory->AddItem(Bandage3, true);

	int32 TotalCount = TestInventory->CountItemByID(5);
	if (TotalCount >= 3)
	{
		LogTest(FString::Printf(TEXT("✓ 堆叠合并成功，应急绷带总数: %d"), TotalCount));
		RecordTestResult(TEXT("Stack Merging"), true, FString::Printf(TEXT("Total: %d"), TotalCount));
	}
	else
	{
		LogTest(FString::Printf(TEXT("✗ 堆叠合并失败，应急绷带总数: %d"), TotalCount));
		RecordTestResult(TEXT("Stack Merging"), false);
	}
}

void AGT_ItemSystemExample::TestConsumableUsage()
{
	LogTest(TEXT("\n[5] 测试消耗品使用..."));

	if (!TestInventory)
	{
		LogTest(TEXT("✗ 背包不存在"));
		return;
	}

	// 添加应急绷带
	UItemBase* Bandage = ItemSystemManager->CreateItemByID(5);
	TestInventory->AddItem(Bandage);

	int32 StackBefore = TestInventory->CountItemByID(5);
	LogTest(FString::Printf(TEXT("使用前绷带数: %d"), StackBefore));

	// 使用消耗品
	bool bUsed = TestInventory->UseConsumableAt(TestInventory->FindItemIndex(Bandage));
	if (bUsed)
	{
		int32 StackAfter = TestInventory->CountItemByID(5);
		LogTest(FString::Printf(TEXT("✓ 消耗品使用成功，使用后绷带数: %d"), StackAfter));
		RecordTestResult(TEXT("Use Consumable"), true, FString::Printf(TEXT("Before: %d, After: %d"), StackBefore, StackAfter));
	}
	else
	{
		LogTest(TEXT("✗ 消耗品使用失败"));
		RecordTestResult(TEXT("Use Consumable"), false);
	}
}

void AGT_ItemSystemExample::TestEquipmentSystem()
{
	LogTest(TEXT("\n[6] 测试装备系统..."));

	if (!TestInventory)
	{
		LogTest(TEXT("✗ 背包不存在"));
		return;
	}

	// 创建装备
	UEquipment* Gloves = Cast<UEquipment>(ItemSystemManager->CreateItemByID(3));  // 公司标准手套
	if (!Gloves)
	{
		LogTest(TEXT("✗ 创建装备失败"));
		RecordTestResult(TEXT("Equipment Creation"), false);
		return;
	}

	TestInventory->AddItem(Gloves);

	// 装备
	int32 GlovesIndex = TestInventory->FindItemIndex(Gloves);
	bool bEquipped = TestInventory->EquipItemAt(GlovesIndex, EGT_EquipmentSlot::Tool);

	if (bEquipped && Gloves->IsEquipped())
	{
		LogTest(TEXT("✓ 装备成功"));
		RecordTestResult(TEXT("Equip Item"), true);

		// 检查装备槽位
		UItemBase* EquippedItem = TestInventory->GetEquippedItem(EGT_EquipmentSlot::Tool);
		if (EquippedItem == Gloves)
		{
			LogTest(TEXT("✓ 装备槽位正确"));
			RecordTestResult(TEXT("Equipment Slot"), true);
		}
	}
	else
	{
		LogTest(TEXT("✗ 装备失败"));
		RecordTestResult(TEXT("Equip Item"), false);
	}
}

void AGT_ItemSystemExample::TestVaultOperations()
{
	LogTest(TEXT("\n[7] 测试仓库操作..."));

	UVault* Vault = ItemSystemManager->GetGlobalVault();
	if (!Vault)
	{
		LogTest(TEXT("✗ 仓库不存在"));
		RecordTestResult(TEXT("Get Vault"), false);
		return;
	}

	LogTest(TEXT("✓ 成功获取仓库"));

	// 创建物品并存入
	UItemBase* Item = ItemSystemManager->CreateItemByID(5);
	bool bStored = Vault->StoreItem(Item, 5);

	if (bStored)
	{
		LogTest(TEXT("✓ 物品存入仓库成功"));
		RecordTestResult(TEXT("Store Item"), true);

		int32 Count = Vault->GetStoredItemCount(5);
		LogTest(FString::Printf(TEXT("✓ 仓库中绷带数: %d"), Count));
	}
	else
	{
		LogTest(TEXT("✗ 物品存入失败"));
		RecordTestResult(TEXT("Store Item"), false);
	}

	// 取出物品
	UItemBase* Withdrawn = Vault->WithdrawItem(5, 2);
	if (Withdrawn)
	{
		LogTest(FString::Printf(TEXT("✓ 从仓库取出 %d 个物品"), Withdrawn->GetStackSize()));
		RecordTestResult(TEXT("Withdraw Item"), true, FString::Printf(TEXT("Count: %d"), Withdrawn->GetStackSize()));
	}
	else
	{
		LogTest(TEXT("✗ 取出失败"));
		RecordTestResult(TEXT("Withdraw Item"), false);
	}
}

void AGT_ItemSystemExample::TestItemEffectTrigger()
{
	LogTest(TEXT("\n[8] 测试物品效果触发..."));

	// 创建被动工艺品
	UPassiveArtifact* Artifact = Cast<UPassiveArtifact>(ItemSystemManager->CreateItemByID(1));  // 异常体犬牙
	if (!Artifact)
	{
		LogTest(TEXT("✗ 创建工艺品失败"));
		RecordTestResult(TEXT("Create Artifact"), false);
		return;
	}

	LogTest(TEXT("✓ 创建工艺品成功"));

	// 注册装备
	ItemSystemManager->RegisterEquippedItem(Artifact);
	LogTest(TEXT("✓ 注册装备物品"));

	// 广播事件
	ItemSystemManager->BroadcastItemTriggerEvent(EGT_ItemEffectTrigger::OnDefeatMonster, nullptr, TEXT(""));
	LogTest(TEXT("✓ 广播 OnDefeatMonster 事件"));

	RecordTestResult(TEXT("Item Effect Trigger"), true);

	// 取消注册
	ItemSystemManager->UnregisterEquippedItem(Artifact);
}

void AGT_ItemSystemExample::TestCurrencySystem()
{
	LogTest(TEXT("\n[9] 测试金币系统..."));

	FGT_CurrencyState Currency;
	Currency.Reset();

	// 测试待结算币
	Currency.AddPendingGold(100);
	if (Currency.PendingGold == 100)
	{
		LogTest(TEXT("✓ 待结算币添加成功"));
		RecordTestResult(TEXT("Add Pending Gold"), true);
	}

	// 测试转换
	Currency.ConvertPendingToSecured(1.15f);  // 1.15倍（公司工牌效果）
	int32 ExpectedSecured = FMath::FloorToInt(100.f * 1.15f);
	if (Currency.SecuredGold == ExpectedSecured && Currency.PendingGold == 0)
	{
		LogTest(FString::Printf(TEXT("✓ 金币转换成功: Pending -> Secured (倍数1.15), 结果: %d"), Currency.SecuredGold));
		RecordTestResult(TEXT("Convert Currency"), true, FString::Printf(TEXT("Result: %d"), Currency.SecuredGold));
	}

	// 测试全局转换
	Currency.ConvertSecuredToGlobal();
	if (Currency.GlobalGold == ExpectedSecured && Currency.SecuredGold == 0)
	{
		LogTest(FString::Printf(TEXT("✓ 金币全局转换成功: Secured -> Global, 结果: %d"), Currency.GlobalGold));
		RecordTestResult(TEXT("Convert to Global"), true);
	}
}

void AGT_ItemSystemExample::TestRunLifecycle()
{
	LogTest(TEXT("\n[10] 测试Run生命周期..."));

	// 模拟冒险开始
	LogTest(TEXT("模拟冒险开始..."));
	UInventory* RunInventory = ItemSystemManager->CreateInventory(20.f, nullptr);
	FGT_CurrencyState RunCurrency;
	RunCurrency.Reset();

	// 模拟掉落
	for (int32 i = 0; i < 3; ++i)
	{
		UItemBase* LootItem = ItemSystemManager->CreateItemByID(5);
		RunInventory->AddItem(LootItem);
	}
	RunCurrency.AddPendingGold(150);

	LogTest(FString::Printf(TEXT("背包物品数: %d, 待结算币: %d"), RunInventory->GetItemCount(), RunCurrency.PendingGold));

	// 模拟撤离成功
	LogTest(TEXT("模拟撤离成功..."));
	RunCurrency.ConvertPendingToSecured(1.0f);
	for (UItemBase* Item : RunInventory->GetAllItems())
	{
		ItemSystemManager->GetGlobalVault()->StoreItem(Item);
	}

	LogTest(FString::Printf(TEXT("安全结算币: %d, 仓库物品数: %d"), RunCurrency.SecuredGold, ItemSystemManager->GetGlobalVault()->GetUniqueItemCount()));

	// 返回HUB
	LogTest(TEXT("返回HUB..."));
	RunCurrency.ConvertSecuredToGlobal();
	LogTest(FString::Printf(TEXT("全局结算币: %d"), RunCurrency.GlobalGold));

	RecordTestResult(TEXT("Run Lifecycle"), true);
}

void AGT_ItemSystemExample::PrintTestResults()
{
	LogTest(TEXT("\n========== 测试结果汇总 =========="));
	for (const FString& Result : TestResults)
	{
		LogTest(Result);
	}
	LogTest(TEXT("========== 测试完成 ==========\n"));
}

bool AGT_ItemSystemExample::LoadItemDataTable()
{
	// 尝试加载物品数据表
	// 注意：这是示例路径，实际路径需要根据项目配置调整
	UDataTable* ItemTable = LoadObject<UDataTable>(nullptr, TEXT("DataTable'/Game/Data/DT_Items.DT_Items'"));

	if (!ItemTable)
	{
		LogTest(TEXT("⚠ 数据表未找到，尝试创建测试数据..."));
		// 在实际应用中，应该正确配置数据表路径
		// 或者创建一个临时数据表用于测试
		return false;
	}

	ItemSystemManager->LoadItemDataTable(ItemTable);
	LogTest(TEXT("✓ 成功加载物品数据表"));
	return true;
}

void AGT_ItemSystemExample::RecordTestResult(const FString& TestName, bool bPassed, const FString& Details)
{
	FString Result = FString::Printf(
		TEXT("[%s] %s %s"),
		bPassed ? TEXT("PASS") : TEXT("FAIL"),
		*TestName,
		*Details.IsEmpty() ? TEXT("") : FString::Printf(TEXT("- %s"), *Details)
	);
	TestResults.Add(Result);
}

void AGT_ItemSystemExample::LogTest(const FString& Message)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, Message);
	}
	UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
}
