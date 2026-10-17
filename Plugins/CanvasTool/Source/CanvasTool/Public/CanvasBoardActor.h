#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CanvasBoardActor.generated.h"

class UStaticMeshComponent;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;

/**
 * A handheld drawing board.  The picture lives in a render target; the brush draws soft dots into
 * it with a translucent material, the eraser draws the board's base colour, and clearing fills the
 * whole target with the base colour.  The ink colour is taken from the character's PickColor
 * variable, which is the project's existing colour-picking logic.
 */
UCLASS()
class CANVASTOOL_API ACanvasBoardActor : public AActor
{
	GENERATED_BODY()

public:
	ACanvasBoardActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Canvas")
	TObjectPtr<UStaticMeshComponent> BoardMesh;

	/** small glowing dot that shows where the brush currently is */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Canvas")
	TObjectPtr<UStaticMeshComponent> CursorMesh;

	/** the view rotation captured when the board came out; the view is locked to it */
	FRotator LockedViewRot = FRotator::ZeroRotator;
	bool bViewLocked = false;

	/** brush uv under the mouse cursor this frame (valid when bCursorOnBoard) */
	FVector2D CursorUV = FVector2D(0.5f, 0.5f);
	bool bCursorOnBoard = false;

	/** selection state: selected boards show a highlight frame and can be dragged around */
	UPROPERTY(BlueprintReadOnly, Category = "Canvas")
	bool bSelected = false;
	bool bDragging = false;
	float LastMouseX = 0.f;
	float LastMouseY = 0.f;
	bool bHaveLastMouse = false;

	/** extra camera-space offset added by dragging (cm) */
	UPROPERTY(BlueprintReadOnly, Category = "Canvas")
	float FollowHorizontalOffset = 0.f;

	/** bright frame shown around the board while it is selected */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Canvas")
	TObjectPtr<UStaticMeshComponent> HighlightMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Canvas")
	TObjectPtr<UTextureRenderTarget2D> CanvasRT;

	/** the colour the board starts as and the colour the eraser restores */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Canvas")
	FLinearColor BaseColor = FLinearColor(0.93f, 0.92f, 0.87f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Canvas")
	int32 CanvasSize = 1024;

	/** physics size of the board in cm (the engine plane is 100x100 units) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Canvas")
	float BoardWidth = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Canvas")
	float BoardHeight = 50.f;

	/** brush radius in uv units (0.02 = 2% of the board) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Canvas")
	float BrushRadius = 0.020f;

	/** how many brush dots have been stamped (debug readout) */
	int32 PaintedDots = 0;

	/** colour taken from a cylinder; overrides the character's PickColor while set */
	UPROPERTY(BlueprintReadWrite, Category = "Canvas")
	FLinearColor InkOverride = FLinearColor(0.05f, 0.05f, 0.09f, 1.f);
	bool bHasInkOverride = false;

	/** how far in front of the player's view the board floats (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Canvas")
	float FollowDistance = 70.f;

	/** how far below the view centre the board floats (cm, negative = lower) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Canvas")
	float FollowVerticalOffset = -16.f;

	/** runtime state driven by the subsystem */
	bool bDrawing = false;
	bool bErase = false;
	bool bIsOut = false;

	// stroke continuity state (written by the subsystem too)
	bool bHasLastUV = false;
	FVector2D LastUV = FVector2D::ZeroVector;

	/** true when the canvas render target was successfully bound to the display material */
	bool bTextureBound = false;

	/** the dynamic material that displays the canvas (public so the subsystem can report it) */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BoardMID;

	UFUNCTION(BlueprintCallable, Category = "Canvas")
	void SetupBoard();

	UFUNCTION(BlueprintCallable, Category = "Canvas")
	void PaintAtUV(FVector2D UV, const FLinearColor& Color, float Radius);

	UFUNCTION(BlueprintCallable, Category = "Canvas")
	void StrokeToUV(FVector2D UV, const FLinearColor& Color, float Radius);

	UFUNCTION(BlueprintCallable, Category = "Canvas")
	void ClearBoard();

	UFUNCTION(BlueprintCallable, Category = "Canvas")
	FLinearColor GetInkColor() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BrushMID;

};