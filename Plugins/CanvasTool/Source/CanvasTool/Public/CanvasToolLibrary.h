#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CanvasToolLibrary.generated.h"

/**
 * Editor helpers for the canvas tool plugin. Lives in the runtime module but every function is
 * editor-only, so the game build never sees them.
 */
UCLASS()
class CANVASTOOL_API UCanvasToolLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Replaces the StaticMesh of the "CylinderMesh" component inside every Blueprint found under
	 * BlueprintFolder, then compiles and saves those blueprints. Everything else (other components,
	 * variables, event graphs and the materials already assigned) is left untouched.
	 *
	 * @return number of blueprints changed, or -1 on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "CanvasTool")
	static int32 RetargetCylinderMeshes(const FString& BlueprintFolder,
										const FString& LeafMeshPath,
										FVector RelativeLocation,
										FVector RelativeScale);
};