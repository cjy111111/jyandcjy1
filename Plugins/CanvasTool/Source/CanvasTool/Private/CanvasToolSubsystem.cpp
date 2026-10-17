#include "CanvasToolSubsystem.h"

#include "CanvasBoardActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Modules/ModuleManager.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

bool UCanvasToolSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

void UCanvasToolSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	SetupBoardActor();

	// build the input mapping context at runtime (no project assets are touched)
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_CanvasTool"));
	IA_Draw = NewObject<UInputAction>(this, TEXT("IA_CanvasDraw"));
	IA_Board = NewObject<UInputAction>(this, TEXT("IA_CanvasBoard"));
	IA_Eraser = NewObject<UInputAction>(this, TEXT("IA_CanvasEraser"));
	IA_Clear = NewObject<UInputAction>(this, TEXT("IA_CanvasClear"));
	IA_Smaller = NewObject<UInputAction>(this, TEXT("IA_CanvasSmaller"));
	IA_Pick = NewObject<UInputAction>(this, TEXT("IA_CanvasPick"));
	IA_Larger = NewObject<UInputAction>(this, TEXT("IA_CanvasLarger"));

	for (UInputAction* Action : { IA_Draw.Get(), IA_Board.Get(), IA_Eraser.Get(),
								  IA_Clear.Get(), IA_Smaller.Get(), IA_Larger.Get(), IA_Pick.Get() })
	{
		if (Action)
		{
			Action->ValueType = EInputActionValueType::Boolean;
		}
	}

	if (MappingContext)
	{
		MappingContext->MapKey(IA_Draw, EKeys::RightMouseButton);
		MappingContext->MapKey(IA_Board, EKeys::B);
		MappingContext->MapKey(IA_Eraser, EKeys::X);
		MappingContext->MapKey(IA_Clear, EKeys::C);
		MappingContext->MapKey(IA_Smaller, EKeys::Z);
		MappingContext->MapKey(IA_Pick, EKeys::LeftMouseButton);
		MappingContext->MapKey(IA_Larger, EKeys::V);
		MappingContext->MapKey(IA_Larger, EKeys::MouseScrollUp);
		MappingContext->MapKey(IA_Smaller, EKeys::MouseScrollDown);
	}

	AddMappingContext();

	// the pawn's input component only exists once it is possessed, so retry for a while
	InWorld.GetTimerManager().SetTimer(SetupTimer, FTimerDelegate::CreateUObject(
		this, &UCanvasToolSubsystem::TrySetupInput), 0.5f, true, 0.5f);
}

void UCanvasToolSubsystem::SetupBoardActor()
{
	UWorld* World = GetWorld();
	if (!World || Board)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Board = World->SpawnActor<ACanvasBoardActor>(
		ACanvasBoardActor::StaticClass(), FTransform::Identity, Params);
	if (Board)
	{
		Board->SetActorHiddenInGame(true);
		Board->bIsOut = false;
	}
}

void UCanvasToolSubsystem::AddMappingContext()
{
	UWorld* World = GetWorld();
	if (!World || !MappingContext)
	{
		return;
	}
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->AddMappingContext(MappingContext, 0);

				// Also make sure the project's own input context is active. If the blueprint that
				// used to add it was edited, the character stops answering WASD and jumps; adding
				// it here is harmless when it is already present.
				if (UInputMappingContext* DefaultIMC = LoadObject<UInputMappingContext>(
					nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default")))
				{
					Subsystem->AddMappingContext(DefaultIMC, 0);
					if (!bReportedDefaultIMC)
					{
						bReportedDefaultIMC = true;
						Message(TEXT("Project input context IMC_Default: ensured"), FColor::Silver);
					}
				}
			}
		}
	}
}

void UCanvasToolSubsystem::TrySetupInput()
{
	UWorld* World = GetWorld();
	if (!World || bInputBound)
	{
		// keep the mapping context alive even if the pawn is not there yet
		AddMappingContext();
		return;
	}

	APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Pawn)
	{
		return;
	}
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	if (!EIC)
	{
		return;
	}

	EIC->BindAction(IA_Draw, ETriggerEvent::Started, this, &UCanvasToolSubsystem::OnDrawStarted);
	EIC->BindAction(IA_Draw, ETriggerEvent::Completed, this, &UCanvasToolSubsystem::OnDrawStopped);
	EIC->BindAction(IA_Draw, ETriggerEvent::Canceled, this, &UCanvasToolSubsystem::OnDrawStopped);
	EIC->BindAction(IA_Board, ETriggerEvent::Started, this, &UCanvasToolSubsystem::OnToggleBoard);
	EIC->BindAction(IA_Eraser, ETriggerEvent::Started, this, &UCanvasToolSubsystem::OnToggleEraser);
	EIC->BindAction(IA_Clear, ETriggerEvent::Started, this, &UCanvasToolSubsystem::OnClearBoard);
	EIC->BindAction(IA_Smaller, ETriggerEvent::Started, this, &UCanvasToolSubsystem::OnBrushSmaller);
	EIC->BindAction(IA_Larger, ETriggerEvent::Started, this, &UCanvasToolSubsystem::OnBrushLarger);
	EIC->BindAction(IA_Pick, ETriggerEvent::Started, this, &UCanvasToolSubsystem::OnSelectBoard);
	EIC->BindAction(IA_Pick, ETriggerEvent::Completed, this, &UCanvasToolSubsystem::OnSelectRelease);

	bInputBound = true;
	World->GetTimerManager().ClearTimer(SetupTimer);
	Message(TEXT("CanvasTool ready:  B board   RMB draw   wheel/X/C brush"), FColor::Cyan);
}

void UCanvasToolSubsystem::Deinitialize()
{
	// never leave the project's input or the overlay in a patched state
	SetProjectRightMouseEnabled(true);
	ShowHints(false);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SetupTimer);
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
					LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				{
					Subsystem->RemoveMappingContext(MappingContext);
				}
			}
		}
	}
	Super::Deinitialize();
}

void UCanvasToolSubsystem::Message(const FString& Text, const FColor& Color) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, Color, Text);
	}
}

void UCanvasToolSubsystem::SetProjectRightMouseEnabled(bool bEnabled)
{
	// Gather every input mapping context that lives in the project, so whichever one the existing
	// right-mouse spray is bound through stops firing while the board is out.
	FAssetRegistryModule& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Found;
	AssetRegistry.Get().GetAssetsByClass(
		UInputMappingContext::StaticClass()->GetClassPathName(), Found, true);

	if (!bEnabled)
	{
		if (bRightMousePatched)
		{
			return;
		}
		CachedRightMouse.Reset();
		PatchedContexts.Reset();

		int32 PatchedCount = 0;
		for (const FAssetData& Data : Found)
		{
			if (!Data.PackagePath.ToString().StartsWith(TEXT("/Game")))
			{
				continue;
			}
			UInputMappingContext* Context = Cast<UInputMappingContext>(Data.GetAsset());
			if (!Context)
			{
				continue;
			}

			// collect first: UnmapKey modifies the array we would otherwise be iterating
			TArray<FEnhancedActionKeyMapping> RightMouseMappings;
			for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
			{
				if (Mapping.Key == EKeys::RightMouseButton)
				{
					RightMouseMappings.Add(Mapping);
				}
			}
			for (const FEnhancedActionKeyMapping& Mapping : RightMouseMappings)
			{
				FCachedRightMouse Cached;
				Cached.Context = Context;
				Cached.Mapping = Mapping;
				CachedRightMouse.Add(Cached);
				Context->UnmapKey(Mapping.Action, EKeys::RightMouseButton);
				++PatchedCount;
			}
			if (RightMouseMappings.Num() > 0)
			{
				PatchedContexts.Add(Context);
			}
		}
		bRightMousePatched = true;

		// Enhanced Input caches the key/action table when a context is added, so the contexts must
		// be re-added for the removal to take effect
		RebuildProjectContexts();

		Message(FString::Printf(TEXT("RMB conflict: removed %d mapping(s) from %d context(s)"),
			PatchedCount, PatchedContexts.Num()),
			PatchedCount > 0 ? FColor::Green : FColor::Red);
	}
	else
	{
		if (!bRightMousePatched)
		{
			return;
		}
		for (const FCachedRightMouse& Cached : CachedRightMouse)
		{
			if (UInputMappingContext* Context = Cached.Context.Get())
			{
				FEnhancedActionKeyMapping& Restored =
					Context->MapKey(Cached.Mapping.Action, Cached.Mapping.Key);
				Restored.Triggers = Cached.Mapping.Triggers;
				Restored.Modifiers = Cached.Mapping.Modifiers;
			}
		}
		const int32 RestoredCount = CachedRightMouse.Num();
		CachedRightMouse.Reset();
		PatchedContexts.Reset();
		bRightMousePatched = false;

		RebuildProjectContexts();
		Message(FString::Printf(TEXT("RMB conflict: restored %d mapping(s)"), RestoredCount),
			FColor::Green);
	}
}

void UCanvasToolSubsystem::RebuildProjectContexts()
{
	// remove and re-add the project's contexts so Enhanced Input rebuilds its cached key table
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
	{
		return;
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FAssetRegistryModule& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Found;
	AssetRegistry.Get().GetAssetsByClass(
		UInputMappingContext::StaticClass()->GetClassPathName(), Found, true);
	for (const FAssetData& Data : Found)
	{
		if (!Data.PackagePath.ToString().StartsWith(TEXT("/Game")))
		{
			continue;
		}
		if (UInputMappingContext* Context = Cast<UInputMappingContext>(Data.GetAsset()))
		{
			Subsystem->RemoveMappingContext(Context);
			Subsystem->AddMappingContext(Context, 0);
		}
	}
}

void UCanvasToolSubsystem::ShowHints(bool bShow)
{
	if (!bShow)
	{
		if (HintsWidget.IsValid() && GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(HintsWidget.ToSharedRef());
		}
		HintsWidget.Reset();
		return;
	}

	if (HintsWidget.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	// Slate can only draw runtime-cache fonts; the project's CJK font is an offline font which
	// renders as boxes, so the hints are kept in plain ASCII, which always renders
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 16);

	const TCHAR* Lines[] = {
		TEXT("BOARD OUT"),
		TEXT("hold RMB = draw"),
		TEXT("X = eraser"),
		TEXT("C = clear"),
		TEXT("wheel = brush size"),
		TEXT("B = put away"),
	};

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (const TCHAR* Line : Lines)
	{
		Box->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.f, 2.f)
			[
				SNew(STextBlock)
				.Font(Font)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.75f, 1.f, 0.75f, 1.f)))
				.Text(FText::FromString(Line))
			];
	}

	TSharedRef<SWidget> Root =
		SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 44.f, 22.f, 0.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.45f))
			.Padding(FMargin(14.f, 10.f))
			[
				Box
			]
		];

	GEngine->GameViewport->AddViewportWidgetContent(Root, 100);
	HintsWidget = Root;
}

void UCanvasToolSubsystem::OnDrawStarted(const FInputActionValue& Value)
{
	Message(TEXT("DRAW: MMB pressed"), FColor::Cyan);
	if (Board && Board->bIsOut)
	{
		Board->bDrawing = true;
		Board->bHasLastUV = false;
	}
}

void UCanvasToolSubsystem::OnDrawStopped(const FInputActionValue& Value)
{
	if (Board)
	{
		Board->bDrawing = false;
		Board->bHasLastUV = false;
	}
}

void UCanvasToolSubsystem::OnToggleBoard(const FInputActionValue& Value)
{
	if (!Board)
	{
		SetupBoardActor();
	}
	if (!Board)
	{
		return;
	}

	Board->bIsOut = !Board->bIsOut;
	if (Board->bIsOut)
	{
		Board->SetupBoard();

		// lock the view and free the mouse so the brush can follow the cursor across the whole
		// board (this is what makes drawing up / down / sideways possible)
		if (APlayerController* ViewPC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			Board->LockedViewRot = ViewPC->GetControlRotation();
			Board->bViewLocked = true;
			ViewPC->SetShowMouseCursor(true);
			ViewPC->SetIgnoreLookInput(true);
			// tell the character blueprint that the board is open (its left-click pick is gated on this)
			if (APawn* Pawn = ViewPC->GetPawn())
			{
				if (FBoolProperty* Prop = CastField<FBoolProperty>(
					Pawn->GetClass()->FindPropertyByName(TEXT("bCanvasBoardOpen"))))
				{
					Prop->SetPropertyValue_InContainer(Pawn, true);
				}
			}
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			ViewPC->SetInputMode(InputMode);
			Message(FString::Printf(TEXT("View locked (yaw %.0f), mouse freed - aim with the cursor"),
				Board->LockedViewRot.Yaw), FColor::Cyan);
		}

		// the board follows the player's view point itself (see ACanvasBoardActor::Tick), so there
		// is nothing to attach here - just diagnostics that prove it is in front of the view
		FVector ViewLoc = FVector::ZeroVector;
		FRotator ViewRot = FRotator::ZeroRotator;
		bool bHaveView = false;
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);
			bHaveView = true;
		}

		if (bHaveView)
		{
			const FVector Fwd = ViewRot.Vector();
			const float Ahead = FVector::DotProduct(Board->GetActorLocation() - ViewLoc, Fwd);
			Message(FString::Printf(TEXT("Board %.0f cm ahead of the view"), Ahead), FColor::Cyan);
			if (Board->BoardMesh)
			{
				const FVector S = Board->BoardMesh->GetRelativeScale3D();
				Message(FString::Printf(
					TEXT("Board scale %.2f %.2f %.2f  =>  %.0f x %.0f cm"),
					S.X, S.Y, S.Z, Board->BoardWidth, Board->BoardHeight), FColor::Cyan);
			}
			Message(Board->bTextureBound
					? TEXT("Canvas texture: BOUND")
					: TEXT("Canvas texture: NOT BOUND (material shows black)"),
				Board->bTextureBound ? FColor::Green : FColor::Red);
		}
		else
		{
			Message(TEXT("Board: no player controller / view point"), FColor::Red);
		}

		if (Board->BoardMesh)
		{
			// the drawing trace needs to hit the board; the character never should
			Board->BoardMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}

		// while the board is out: the project's right-mouse spray off, our drawing on, hints shown
		SetProjectRightMouseEnabled(false);
		ShowHints(true);
		Board->SetActorHiddenInGame(false);
		Message(TEXT("Board OUT  (hold RMB to draw, wheel = brush, X eraser, C clear, B put away)"), FColor::Green);
	}
	else
	{
		Board->bDrawing = false;
		Board->SetActorHiddenInGame(true);
		if (Board->BoardMesh)
		{
			// no collision at all while the board is put away, so it can never block the player
			Board->BoardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		// restored: right-mouse spray back, our drawing off, hints hidden, mouse captured again
		SetProjectRightMouseEnabled(true);
		ShowHints(false);
		if (Board)
		{
			Board->bViewLocked = false;
			if (Board->CursorMesh)
			{
				Board->CursorMesh->SetVisibility(false);
			}
		}
		if (APlayerController* ViewPC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			ViewPC->SetShowMouseCursor(false);
			ViewPC->SetIgnoreLookInput(false);
			if (APawn* Pawn = ViewPC->GetPawn())
			{
				if (FBoolProperty* Prop = CastField<FBoolProperty>(
					Pawn->GetClass()->FindPropertyByName(TEXT("bCanvasBoardOpen"))))
				{
					Prop->SetPropertyValue_InContainer(Pawn, false);
				}
			}
			ViewPC->SetInputMode(FInputModeGameOnly());
		}
		Message(TEXT("Board AWAY  (view unlocked, mouse captured)"), FColor::Yellow);
	}
}

void UCanvasToolSubsystem::OnToggleEraser(const FInputActionValue& Value)
{
	if (Board && Board->bIsOut)
	{
		Board->bErase = !Board->bErase;
		Message(Board->bErase ? TEXT("Eraser ON") : TEXT("Eraser OFF"),
			Board->bErase ? FColor::Orange : FColor::Green);
	}
}

void UCanvasToolSubsystem::OnClearBoard(const FInputActionValue& Value)
{
	if (Board)
	{
		Board->ClearBoard();
		Message(TEXT("Board cleared"), FColor::Green);
	}
}

void UCanvasToolSubsystem::OnBrushSmaller(const FInputActionValue& Value)
{
	if (Board)
	{
		Board->BrushRadius = FMath::Max(0.004f, Board->BrushRadius * 0.7f);
		Message(FString::Printf(TEXT("Brush radius %.3f"), Board->BrushRadius), FColor::Silver);
	}
}

void UCanvasToolSubsystem::OnBrushLarger(const FInputActionValue& Value)
{
	if (Board)
	{
		Board->BrushRadius = FMath::Min(0.12f, Board->BrushRadius * 1.4f);
		Message(FString::Printf(TEXT("Brush radius %.3f"), Board->BrushRadius), FColor::Silver);
	}
}
void UCanvasToolSubsystem::OnPickColour(const FInputActionValue& Value)
{
	if (!Board || !Board->bIsOut)
	{
		return;
	}
	UWorld* World = GetWorld();
	APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!Pawn)
	{
		return;
	}

	// 1) call the project's own pick function when one can be identified (parameterless only)
	UFunction* Found = nullptr;
	FString FoundName;
	for (TFieldIterator<UFunction> It(Pawn->GetClass()); It; ++It)
	{
		if (It->NumParms != 0)
		{
			continue;
		}
		const FString Name = It->GetName();
		if (Name.Contains(TEXT("Pick")) || Name.Contains(TEXT("Color"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Colour"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Spray"), ESearchCase::IgnoreCase))
		{
			Found = *It;
			FoundName = Name;
			break;
		}
	}
	if (Found)
	{
		Pawn->ProcessEvent(Found, nullptr);
		Message(FString::Printf(TEXT("PICK: called %s"), *FoundName), FColor::Cyan);
	}
	else
	{
		FString Candidates;
		int32 Shown = 0;
		for (TFieldIterator<UFunction> It(Pawn->GetClass()); It && Shown < 10; ++It)
		{
			if (It->NumParms != 0)
			{
				continue;
			}
			Candidates += It->GetName() + TEXT(" ");
			++Shown;
		}
		Message(FString::Printf(TEXT("PICK: no pick function found. Candidates: %s"), *Candidates), FColor::Red);
	}

	// 2) take the colour their logic stored in PickColor and use it as the ink
	Board->bHasInkOverride = false;
	const FLinearColor Picked = Board->GetInkColor();
	Board->InkOverride = Picked;
	Board->bHasInkOverride = true;
	Message(FString::Printf(TEXT("PICK: ink = (%.2f %.2f %.2f)"), Picked.R, Picked.G, Picked.B), FColor::Green);
}

void UCanvasToolSubsystem::OnSelectBoard(const FInputActionValue& Value)
{
	if (!Board || !Board->bIsOut || !Board->bCursorOnBoard)
	{
		return;
	}
	// click on the board: select (and start dragging) or deselect when already selected
	Board->bSelected = !Board->bSelected;
	Board->bDragging = Board->bSelected;
	Message(Board->bSelected
			? TEXT("Board SELECTED - drag with the mouse, click again to release")
			: TEXT("Board released"),
		Board->bSelected ? FColor::Cyan : FColor::Silver);
}

void UCanvasToolSubsystem::OnSelectRelease(const FInputActionValue& Value)
{
	if (Board)
	{
		Board->bDragging = false;
	}
}