#include "CanvasBoardActor.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ACanvasBoardActor::ACanvasBoardActor()
{
	PrimaryActorTick.bCanEverTick = true;

	BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
	SetRootComponent(BoardMesh);
	BoardMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoardMesh->SetCollisionObjectType(ECC_WorldDynamic);
	BoardMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	// only the drawing trace may hit the board; the character's capsule must pass straight through
	BoardMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BoardMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	BoardMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	BoardMesh->SetGenerateOverlapEvents(false);
	BoardMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		BoardMesh->SetStaticMesh(PlaneMesh.Object);
	}

	// the little dot that shows where the brush is
	CursorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CursorMesh"));
	CursorMesh->SetupAttachment(BoardMesh);
	CursorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CursorMesh->SetGenerateOverlapEvents(false);
	CursorMesh->SetCastShadow(false);
	CursorMesh->SetVisibility(false);
	if (PlaneMesh.Succeeded())
	{
		CursorMesh->SetStaticMesh(PlaneMesh.Object);
	}

	// bright frame shown while the board is selected
	HighlightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HighlightMesh"));
	HighlightMesh->SetupAttachment(BoardMesh);
	HighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HighlightMesh->SetGenerateOverlapEvents(false);
	HighlightMesh->SetCastShadow(false);
	HighlightMesh->SetVisibility(false);
	if (PlaneMesh.Succeeded())
	{
		HighlightMesh->SetStaticMesh(PlaneMesh.Object);
	}
}

void ACanvasBoardActor::BeginPlay()
{
	Super::BeginPlay();
	SetupBoard();
}

void ACanvasBoardActor::SetupBoard()
{
	if (!CanvasRT)
	{
		CanvasRT = UKismetRenderingLibrary::CreateRenderTarget2D(
			this, CanvasSize, CanvasSize, ETextureRenderTargetFormat::RTF_RGBA8, BaseColor, false);
		// only the brand new canvas starts empty - putting the board away and back again keeps
		// every stroke, and only the explicit Clear action (C) wipes it
		if (CanvasRT)
		{
			UKismetRenderingLibrary::ClearRenderTarget2D(this, CanvasRT, BaseColor);
		}
	}

	if (!BoardMID && BoardMesh)
	{
		if (UMaterialInterface* BoardMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/CanvasTool/M_BoardSurface.M_BoardSurface")))
		{
			BoardMID = UMaterialInstanceDynamic::Create(BoardMat, this);
			if (BoardMID && CanvasRT)
			{
				// try the expected parameter name first, then a couple of fallbacks, and verify
				// that the render target really got bound (an unbound slot renders black)
				const TCHAR* Names[] = { TEXT("CanvasTexture"), TEXT("Canvas"), TEXT("Texture"),
										 TEXT("CanvasTex"), TEXT("RenderTarget") };
				for (const TCHAR* Name : Names)
				{
					BoardMID->SetTextureParameterValue(Name, CanvasRT);
					UTexture* Bound = nullptr;
					BoardMID->GetTextureParameterValue(FMaterialParameterInfo(Name), Bound);
					if (Bound)
					{
						bTextureBound = true;
						break;
					}
				}
			}
			BoardMesh->SetMaterial(0, BoardMID);
		}
	}

	if (!BrushMID)
	{
		if (UMaterialInterface* BrushMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/CanvasTool/M_BrushDot.M_BrushDot")))
		{
			BrushMID = UMaterialInstanceDynamic::Create(BrushMat, this);
		}
	}

	if (BoardMesh)
	{
		// the plane is 100x100 cm, so the scale turns it into the wanted board size; the actor
		// itself is oriented every frame to face the player's view
		BoardMesh->SetRelativeLocation(FVector::ZeroVector);
		BoardMesh->SetRelativeRotation(FRotator::ZeroRotator);
		BoardMesh->SetRelativeScale3D(FVector(BoardWidth / 100.f, BoardHeight / 100.f, 1.f));
	}

	// a filled dot (reusing the brush material) marks the brush position on the board
	if (CursorMesh && BrushMID)
	{
		if (UMaterialInstanceDynamic* CursorMID =
			UMaterialInstanceDynamic::Create(BrushMID->GetMaterial(), this))
		{
			CursorMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.35f, 0.1f, 1.f));
			CursorMID->SetVectorParameterValue(TEXT("Center"), FLinearColor(0.5f, 0.5f, 0.f, 1.f));
			CursorMID->SetScalarParameterValue(TEXT("Radius"), 0.5f);
			CursorMesh->SetMaterial(0, CursorMID);
		}

		if (HighlightMesh)
		{
			// opaque solid colour so the oversized plane reads as a clear frame around the board
			if (UMaterialInterface* HighlightMat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Game/CanvasTool/M_BoardHighlight.M_BoardHighlight")))
			{
				HighlightMesh->SetMaterial(0, HighlightMat);
			}
		}
	}
}

FLinearColor ACanvasBoardActor::GetInkColor() const
{
	if (bHasInkOverride)
	{
		return InkOverride;
	}
	// The project's existing colour-picking logic stores the picked colour in the character's
	// PickColor variable; the brush simply uses that.
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (FStructProperty* Prop = CastField<FStructProperty>(
			Pawn->GetClass()->FindPropertyByName(TEXT("PickColor"))))
		{
			if (Prop->Struct == TBaseStructure<FLinearColor>::Get())
			{
				const FLinearColor Picked = *Prop->ContainerPtrToValuePtr<FLinearColor>(Pawn);
				// white (or an un-picked colour) is invisible on the pale board, so fall back to a
				// dark ink until the player actually picks a colour from a cylinder
				const bool bTooPale = Picked.R > 0.93f && Picked.G > 0.93f && Picked.B > 0.93f;
				if (!bTooPale)
				{
					return Picked;
				}
				return FLinearColor(0.05f, 0.05f, 0.09f, 1.f);
			}
		}
	}
	return FLinearColor(0.05f, 0.05f, 0.09f, 1.f);
}

void ACanvasBoardActor::PaintAtUV(FVector2D UV, const FLinearColor& Color, float Radius)
{
	if (!CanvasRT || !BrushMID)
	{
		return;
	}
	BrushMID->SetVectorParameterValue(TEXT("Color"), Color);
	BrushMID->SetVectorParameterValue(TEXT("Center"), FLinearColor(UV.X, UV.Y, 0.f, 0.f));
	BrushMID->SetScalarParameterValue(TEXT("Radius"), FMath::Max(0.002f, Radius));
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, CanvasRT, BrushMID);
	++PaintedDots;
}

void ACanvasBoardActor::StrokeToUV(FVector2D UV, const FLinearColor& Color, float Radius)
{
	if (!bHasLastUV)
	{
		PaintAtUV(UV, Color, Radius);
		LastUV = UV;
		bHasLastUV = true;
		return;
	}

	// fill the gap since the previous frame so the stroke is a continuous line
	const float Dist = FVector2D::Distance(LastUV, UV);
	const float Spacing = FMath::Max(0.25f * Radius, 0.002f);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dist / Spacing), 1, 96);
	for (int32 i = 1; i <= Steps; ++i)
	{
		PaintAtUV(FMath::Lerp(LastUV, UV, (float)i / (float)Steps), Color, Radius);
	}
	LastUV = UV;
}

void ACanvasBoardActor::ClearBoard()
{
	if (CanvasRT)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, CanvasRT, BaseColor);
	}
}

void ACanvasBoardActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsOut || !BoardMesh)
	{
		bHasLastUV = false;
		return;
	}

	// The board hangs in the locked view, so it stays put on screen while the mouse moves freely.
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	FVector ViewLoc = FVector::ZeroVector;
	FRotator ViewRot = FRotator::ZeroRotator;
	if (PC)
	{
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		// keep the view locked while the board is out, whatever the mouse does
		if (bViewLocked)
		{
			PC->SetControlRotation(LockedViewRot);
			ViewRot = LockedViewRot;
		}
	}
	else
	{
		return;
	}

	const FVector Fwd = ViewRot.Vector();
	const FVector Right = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::Y);
	const FVector Up = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::Z);
	SetActorLocationAndRotation(
		ViewLoc + Fwd * FollowDistance + Up * FollowVerticalOffset + Right * FollowHorizontalOffset,
		FRotationMatrix::MakeFromZX(-Fwd, Right).Rotator());

	// ---- where is the mouse pointing on the board? (this is what lets you draw up/down) ----
	FVector2D MouseUV = FVector2D::ZeroVector;
	bool bMouseOnBoard = false;
	FHitResult Hit;
	{
		FVector RayOrigin = FVector::ZeroVector;
		FVector RayDir = FVector::ZeroVector;
		if (PC->DeprojectMousePositionToWorld(RayOrigin, RayDir))
		{
			const FVector RayEnd = RayOrigin + RayDir * 4000.f;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(CanvasBoardAim), false, PC->GetPawn());
			if (GetWorld()->LineTraceSingleByChannel(Hit, RayOrigin, RayEnd, ECC_Visibility, Params) &&
				Hit.GetActor() == this)
			{
				const FVector Local = BoardMesh->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
				// engine plane: U runs along +X, V along +Y (no axis swap, no flips)
				MouseUV.X = FMath::Clamp(Local.X / 100.f + 0.5f, 0.f, 1.f);
				MouseUV.Y = FMath::Clamp(Local.Y / 100.f + 0.5f, 0.f, 1.f);
				bMouseOnBoard = true;
			}
		}
	}
	CursorUV = MouseUV;
	bCursorOnBoard = bMouseOnBoard;

	// dragging the selected board: follow the real mouse position, converted to camera-space cm
	if (bSelected && bDragging)
	{
		float Mx = 0.f;
		float My = 0.f;
		if (PC->GetMousePosition(Mx, My))
		{
			if (bHaveLastMouse)
			{
				float VpX = 1920.f;
				float VpY = 1080.f;
				if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
				{
					const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
					VpX = FMath::Max(1.f, (float)Size.X);
					VpY = FMath::Max(1.f, (float)Size.Y);
				}
				float FovDeg = 90.f;
				if (PC->PlayerCameraManager)
				{
					FovDeg = PC->PlayerCameraManager->GetFOVAngle();
				}
				const float WorldPerPixel = (2.f * FollowDistance *
					FMath::Tan(FMath::DegreesToRadians(FovDeg * 0.5f))) / VpX;
				FollowHorizontalOffset = FMath::Clamp(
					FollowHorizontalOffset + (Mx - LastMouseX) * WorldPerPixel, -150.f, 150.f);
				FollowVerticalOffset = FMath::Clamp(
					FollowVerticalOffset - (My - LastMouseY) * WorldPerPixel * (VpX / VpY),
					-120.f, 80.f);
			}
			LastMouseX = Mx;
			LastMouseY = My;
			bHaveLastMouse = true;
		}
	}
	else
	{
		bHaveLastMouse = false;
	}

	// bright frame around the board while selected
	if (HighlightMesh)
	{
		const bool bShowFrame = bSelected && bViewLocked;
		HighlightMesh->SetVisibility(bShowFrame);
		if (bShowFrame)
		{
			const FVector Centre = BoardMesh->GetComponentLocation();
			const FVector Normal = BoardMesh->GetComponentRotation().RotateVector(FVector::UpVector);
			HighlightMesh->SetWorldLocation(Centre - Normal * 1.0f);
			HighlightMesh->SetWorldRotation(BoardMesh->GetComponentRotation());
			HighlightMesh->SetWorldScale3D(FVector((BoardWidth + 7.f) / 100.f,
												   (BoardHeight + 7.f) / 100.f, 1.f));
		}
	}
	// move the little glowing dot to the mouse position on the board
	if (CursorMesh)
	{
		const bool bShow = bViewLocked && bMouseOnBoard;
		CursorMesh->SetVisibility(bShow);
		if (bShow)
		{
			// uv -> local plane position (inverse of the mapping above), pushed 0.6 cm forward
			// sit exactly on the surface point the mouse ray hit, lifted a hair to avoid z-fighting
			CursorMesh->SetWorldLocation(Hit.ImpactPoint + Hit.ImpactNormal * 0.8f);
			const float CursorScale = FMath::Max(0.02f, BrushRadius * 2.f * 100.f) / 100.f;
			CursorMesh->SetRelativeScale3D(FVector(BoardWidth / 100.f * CursorScale,
												   BoardHeight / 100.f * CursorScale, 1.f));
		}
	}

	if (!bDrawing)
	{
		bHasLastUV = false;
		return;
	}

	if (!bMouseOnBoard)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(99001, 0.2f, FColor::Red,
				TEXT("DRAW: mouse is not over the board"));
		}
		bHasLastUV = false;
		return;
	}

	const FVector2D UV = MouseUV;
	const FLinearColor Ink = bErase ? BaseColor : GetInkColor();
	StrokeToUV(UV, Ink, BrushRadius);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(99001, 0.2f, FColor::Green,
			FString::Printf(TEXT("DRAW: uv=(%.2f, %.2f)  dots=%d  ink=(%.2f %.2f %.2f)"),
				UV.X, UV.Y, PaintedDots, Ink.R, Ink.G, Ink.B));
	}
}