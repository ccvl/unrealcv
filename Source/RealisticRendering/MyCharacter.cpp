// Fill out your copyright notice in the Description page of Project Settings.

#include "RealisticRendering.h"
#include "MyCharacter.h"
#include "MyHUD.h"
#include "StaticMeshResources.h"
#include "Networking.h"
#include <string>
#include "ImageUtils.h"
#include "ViewMode.h"


// Sets default values
AMyCharacter::AMyCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	NetworkManager = NewObject<UNetworkManager>();
	NetworkManager->World = this->GetWorld();
}

// Called when the game starts or when spawned
void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();

	DefineConsoleCommands();
	NetworkManager->ListenSocket();
}

void AMyCharacter::NotifyClient(FString Message)
{
	// Send a message to client to say a new frame is rendered.
	NetworkManager->SendMessage(Message);
}

void AMyCharacter::TakeScreenShot(FString Filename)
{
	static uint32 NumCaptured = 0;
	NumCaptured++;
	Filename = FString::Printf(TEXT("%04d.png"), NumCaptured);
	TArray<FColor> Bitmap;

	bool bScreenshotSuccessful = false;
	UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
	FViewport* InViewport = ViewportClient->Viewport;
	bScreenshotSuccessful = GetViewportScreenShot(InViewport, Bitmap);
	// ViewportClient->GetHighResScreenshotConfig().MergeMaskIntoAlpha(Bitmap);
	// Ensure that all pixels' alpha is set to 255
	for (auto& Color : Bitmap)
	{
		Color.A = 255;
	}

	if (bScreenshotSuccessful)
	{
		NotifyClient(FString::Printf(TEXT("%s"), *Filename));
		FString ScreenShotName = Filename;
		FIntVector Size(InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y, 0);
		// TODO: Need to blend alpha, a bit weird from screen.


		TArray<uint8> CompressedBitmap;
		FImageUtils::CompressImageArray(Size.X, Size.Y, Bitmap, CompressedBitmap);
		FFileHelper::SaveArrayToFile(CompressedBitmap, *ScreenShotName);
	}
	else
	{
		NotifyClient(TEXT("Screen capture failed"));
	}
}

// Called every frame
void AMyCharacter::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

}

// Called to bind functionality to input
void AMyCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	Super::SetupPlayerInputComponent(InputComponent);

	check(InputComponent);

	InputComponent->BindAxis("MoveForward", this, &AMyCharacter::MoveForward);
	InputComponent->BindAxis("MoveRight", this, &AMyCharacter::MoveRight);

	// Handle Mouse Input
	InputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	InputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);

	InputComponent->BindAction("Fire", IE_Pressed, this, &AMyCharacter::OnFire);
}

void AMyCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AMyCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AMyCharacter::OnFire()
{
	DispatchCommands();
	PaintAllObjects();
	TakeScreenShot(TEXT(""));

	UE_LOG(LogTemp, Warning, TEXT("Fire"));
	FHitResult HitResult;
	// The original version for the shooting game use CameraComponent
	FVector StartLocation = GetActorLocation();
	FRotator Direction = GetActorRotation();

	FVector EndLocation = StartLocation + Direction.Vector() * 10000;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
	{
		AActor* HitActor = HitResult.GetActor();

		// UE_LOG(LogTemp, Warning, TEXT("%s"), *HitActor->GetActorLabel());
		// Draw a bounding box of the hitted object and also output the name of it.
	}
}

void AMyCharacter::PaintAllObjects()
{
	// Iterate over all actors
	ULevel* Level = GetLevel();
	for (auto Actor : Level->Actors)
	{
		if (Actor && Actor->IsA(AStaticMeshActor::StaticClass())) // Only StaticMeshActor is interesting
		{
			// if (Actor->GetActorLabel() == FString("SM_Door43"))
			{
				PaintObject(Actor);
			}
		}
	}

	// Paint actor using floodfill.
}

void AMyCharacter::PaintObject(AActor* Actor)
{
	TArray<UMeshComponent*> PaintableComponents;
	// TInlineComponentArray<UMeshComponent*> MeshComponents;
	// Actor->GetComponents<UMeshComponent>(MeshComponents);
	Actor->GetComponents<UMeshComponent>(PaintableComponents);


	for (auto MeshComponent : PaintableComponents)
	{
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
		{
			if (UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh)
			{
				uint32 PaintingMeshLODIndex = 0;
				uint32 NumLODLevel = StaticMeshComponent->StaticMesh->RenderData->LODResources.Num();
				check(NumLODLevel == 1);
				FStaticMeshLODResources& LODModel = StaticMeshComponent->StaticMesh->RenderData->LODResources[PaintingMeshLODIndex];
				FStaticMeshComponentLODInfo* InstanceMeshLODInfo = NULL;

				// PaintingMeshLODIndex + 1 is the minimum requirement, enlarge if not satisfied
				StaticMeshComponent->SetLODDataCount(PaintingMeshLODIndex + 1, StaticMeshComponent->LODData.Num());
				InstanceMeshLODInfo = &StaticMeshComponent->LODData[PaintingMeshLODIndex];

				if (!InstanceMeshLODInfo->OverrideVertexColors) {
					InstanceMeshLODInfo->OverrideVertexColors = new FColorVertexBuffer;

					FColor FillColor = FColor(255, 255, 255, 255);
					InstanceMeshLODInfo->OverrideVertexColors->InitFromSingleColor(FColor::White, LODModel.GetNumVertices());
				}

				uint32 NumVertices = LODModel.GetNumVertices();
				check(InstanceMeshLODInfo->OverrideVertexColors);
				check(NumVertices <= InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices());
				StaticMeshComponent->CachePaintedDataIfNecessary();

				FColor NewColor = FColor(FMath::RandRange(0, 255), FMath::RandRange(0, 255), FMath::RandRange(0, 255), 255);
				for (uint32 ColorIndex = 0; ColorIndex < NumVertices; ++ColorIndex)
				{
					// FColor NewColor = FColor(FMath::RandRange(0, 255), FMath::RandRange(0, 255), FMath::RandRange(0, 255), 255);
					// LODModel.ColorVertexBuffer.VertexColor(ColorIndex) = NewColor;  // This is vertex level
					// Need to initialize the vertex buffer first
					uint32 NumOverrideVertexColors = InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices(),
						NumPaintedVertices = InstanceMeshLODInfo->PaintedVertices.Num();
					check(NumOverrideVertexColors == NumPaintedVertices);
					InstanceMeshLODInfo->OverrideVertexColors->VertexColor(ColorIndex) = NewColor;
					InstanceMeshLODInfo->PaintedVertices[ColorIndex].Color = NewColor;
				}
				BeginInitResource(InstanceMeshLODInfo->OverrideVertexColors);
				StaticMeshComponent->MarkRenderStateDirty();
				// BeginUpdateResourceRHI(InstanceMeshLODInfo->OverrideVertexColors);


				/*
				// TODO: Need to check other LOD levels
				// Use flood fill to paint mesh vertices
				UE_LOG(LogTemp, Warning, TEXT("%s:%s has %d vertices"), *Actor->GetActorLabel(), *StaticMeshComponent->GetName(), NumVertices);

				if (LODModel.ColorVertexBuffer.GetNumVertices() == 0)
				{
					// Mesh doesn't have a color vertex buffer yet!  We'll create one now.
					LODModel.ColorVertexBuffer.InitFromSingleColor(FColor(255, 255, 255, 255), LODModel.GetNumVertices());
				}

				*/
			}
		}
	}
}

void Hello()
{
	UE_LOG(LogTemp, Warning, TEXT("Hello World."));
}


void AMyCharacter::ActionWrapper(const TArray<FString>& Args)
{
	UE_LOG(LogTemp, Warning, TEXT("The arg of action is %s"), *Args[0]);
}

void AMyCharacter::DispatchCommands()
{
	auto SetViewMode = FConsoleCommandWithArgsDelegate::CreateStatic(FViewMode::SetMode);
	// CommandDispatcher.BindCommand("vset /mode/(?<ViewMode>.*)", SetViewMode); // Better to check the correctness at compile time
	CommandDispatcher.BindCommand("vset /mode/(.*)", SetViewMode); // Better to check the correctness at compile time
	auto GetViewMode = FConsoleCommandWithArgsDelegate::CreateStatic(FViewMode::GetMode);
	CommandDispatcher.BindCommand("vget /mode", GetViewMode);
	CommandDispatcher.Exec("vset /mode/depth");
	CommandDispatcher.Exec("vget /mode");

	/*
	CommandDispatcher.BindCommand("vget /camera/[id]/location", Command);
	CommandDispatcher.BindCommand("vget /camera/[id]/rotation", Command);
	CommandDispatcher.BindCommand("vget /camera/[id]/name", Command);
	CommandDispatcher.BindCommand("vget /camera/[id]/image", Command); // Take a screenshot and return filename

	CommandDispatcher.BindCommand("vset /camera/[id]/location [x] [y] [z]", Command);
	// CommandDispatcher.BindCommand("vset /camera/[id]/rotation [x] [y] [z]", Command);
	*/
	CommandDispatcher.Alias("VisionDepth", "vset /mode/depth"); // Alias for human interaction
	CommandDispatcher.Alias("VisionCamInfo", "vget /camera/0/name");

}

void AMyCharacter::DefineConsoleCommands()
{
	// Example Command, Hello
	IConsoleObject* HelloCmd = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Hello"),
		TEXT("Print a hello message to Log"),
		FConsoleCommandDelegate::CreateStatic(Hello),
		ECVF_Default
		);

	// Select An Object, this is done through OnFire event
	// ViewMode
	FViewMode::RegisterCommands();

	// Show and Hide the Cursor
	IConsoleObject* ToggleCursorCmd = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("VisionToggleCursor"),
		TEXT("Toggle whether the cursor is visible"),
		FConsoleCommandDelegate::CreateStatic(AMyHUD::ToggleCursor),
		ECVF_Default
		);

	// Show labels on the screen
	IConsoleObject* ToggleLabelCmd = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("VisionToggleLabel"),
		TEXT("Toggle whether the label of object is visible"),
		FConsoleCommandDelegate::CreateStatic(AMyHUD::ToggleLabel),
		ECVF_Default
		);

	// Make Screen Shot


	// Add Action Command
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("VisionForward"),
		TEXT("Move Camera Forward"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &AMyCharacter::ActionWrapper),
		// it also provides CreateRAW
		ECVF_Default
		);
}
