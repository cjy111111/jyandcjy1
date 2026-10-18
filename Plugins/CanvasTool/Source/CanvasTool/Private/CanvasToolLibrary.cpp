#include "CanvasToolLibrary.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#include "Materials/MaterialInterface.h"
#endif

int32 UCanvasToolLibrary::RetargetCylinderMeshes(const FString& BlueprintFolder,
												 const FString& LeafMeshPath,
												 FVector RelativeLocation,
												 FVector RelativeScale)
{
#if WITH_EDITOR
	UStaticMesh* Leaf = LoadObject<UStaticMesh>(nullptr, *LeafMeshPath);
	if (!Leaf)
	{
		UE_LOG(LogTemp, Error, TEXT("[CanvasTool] leaf mesh not found: %s"), *LeafMeshPath);
		return -1;
	}

	FAssetRegistryModule& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssetsByPath(FName(*BlueprintFolder), Assets, true);

	int32 Changed = 0;
	for (const FAssetData& Data : Assets)
	{
		UBlueprint* BP = Cast<UBlueprint>(Data.GetAsset());
		if (!BP || !BP->SimpleConstructionScript)
		{
			continue;
		}

		bool bHitThisBlueprint = false;
		for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
		{
			if (!Node || Node->GetVariableName().ToString() != TEXT("CylinderMesh"))
			{
				continue;
			}
			UStaticMeshComponent* Templ = Cast<UStaticMeshComponent>(Node->ComponentTemplate);
			if (!Templ)
			{
				continue;
			}
			Templ->Modify();
			Templ->SetStaticMesh(Leaf);
			Templ->SetRelativeLocation(RelativeLocation);
			Templ->SetRelativeScale3D(RelativeScale);
			Templ->SetRelativeRotation(FRotator::ZeroRotator);
			bHitThisBlueprint = true;
			UE_LOG(LogTemp, Display, TEXT("[CanvasTool] %s -> leaf mesh"), *BP->GetName());
		}

		if (bHitThisBlueprint)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
			FKismetEditorUtilities::CompileBlueprint(BP);
			// saving is done from Python after this call
			++Changed;
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[CanvasTool] RetargetCylinderMeshes: %d blueprint(s) changed"), Changed);
	return Changed;
#else
	return -1;
#endif
}