#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "CanvasToolSubsystem.generated.h"

class ACanvasBoardActor;
class UInputAction;

/**
 * Creates the drawing board and drives it from the keyboard and the right mouse button.
 * The plugin registers its own Enhanced Input context at runtime, so nothing in the project's
 * existing blueprints or assets has to change.
 *
 *   B  - take the board out / put it away
 *   Middle mouse (hold) - draw a continuous line on the board
 *   X  - toggle eraser (draws the board's base colour)
 *   C  - clear the board (everything back to the base colour)
 *   Z / V - smaller / larger brush
 */
UCLASS()
class CANVASTOOL_API UCanvasToolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintReadOnly, Category = "Canvas")
	TObjectPtr<ACanvasBoardActor> Board;

	UFUNCTION() void OnDrawStarted(const FInputActionValue& Value);
	UFUNCTION() void OnDrawStopped(const FInputActionValue& Value);
	UFUNCTION() void OnToggleBoard(const FInputActionValue& Value);
	UFUNCTION() void OnToggleEraser(const FInputActionValue& Value);
	UFUNCTION() void OnClearBoard(const FInputActionValue& Value);
	UFUNCTION() void OnBrushSmaller(const FInputActionValue& Value);
	UFUNCTION() void OnBrushLarger(const FInputActionValue& Value);
	UFUNCTION() void OnPickColour(const FInputActionValue& Value);
	UFUNCTION() void OnSelectBoard(const FInputActionValue& Value);
	UFUNCTION() void OnSelectRelease(const FInputActionValue& Value);

protected:
	void TrySetupInput();
	void SetupBoardActor();
	void AddMappingContext();
	void Message(const FString& Text, const FColor& Color) const;

	/** removes (or restores) the project's right-mouse mappings so its spray cannot fire while
	 *  the board is out; the original triggers and modifiers are restored exactly */
	void SetProjectRightMouseEnabled(bool bEnabled);

	/** re-adds the project's input contexts so Enhanced Input rebuilds its cached key table */
	void RebuildProjectContexts();

	/** top-right key hints overlay, built in code (no widget assets needed) */
	void ShowHints(bool bShow);

	/** one remembered right-mouse mapping so it can be restored verbatim */
	struct FCachedRightMouse
	{
		TWeakObjectPtr<UInputMappingContext> Context;
		FEnhancedActionKeyMapping Mapping;
	};

	TSharedPtr<class SWidget> HintsWidget;
	TArray<FCachedRightMouse> CachedRightMouse;
	UPROPERTY() TArray<TObjectPtr<UInputMappingContext>> PatchedContexts;
	bool bRightMousePatched = false;

	UPROPERTY() TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY() TObjectPtr<UInputAction> IA_Draw;
	UPROPERTY() TObjectPtr<UInputAction> IA_Board;
	UPROPERTY() TObjectPtr<UInputAction> IA_Eraser;
	UPROPERTY() TObjectPtr<UInputAction> IA_Clear;
	UPROPERTY() TObjectPtr<UInputAction> IA_Smaller;
	UPROPERTY() TObjectPtr<UInputAction> IA_Larger;
	UPROPERTY() TObjectPtr<UInputAction> IA_Pick;

	FTimerHandle SetupTimer;
	bool bInputBound = false;
	bool bReportedDefaultIMC = false;
};