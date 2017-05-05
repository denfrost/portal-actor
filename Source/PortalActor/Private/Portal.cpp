// Fill out your copyright notice in the Description page of Project Settings.

#include "PortalActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "DrawDebugHelpers.h"
#include "Components/ArrowComponent.h"
#include "EngineUtils.h"
#include "Portal.h"


// Sets default values
APortal::APortal() {
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Frame = CreateDefaultSubobject<UStaticMeshComponent>(FName("Frame"));
	RootComponent = Frame;
	//Frame->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	Portal = CreateDefaultSubobject<UStaticMeshComponent>(FName("Portal"));
	Portal->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, true));

	TargetCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(FName("TargetCapture"));
	TargetCapture->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::KeepWorld, true));

	TeleportedActors = TSet<AActor*>();
	ReceivedActors = TSet<AActor*>();

	Overlap = CreateDefaultSubobject<UBoxComponent>(FName("Overlap"));
	Overlap->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, true));
	Overlap->bGenerateOverlapEvents = true;
	Overlap->SetCollisionProfileName(FName("Portal"));
}

// Called when the game starts or when spawned
void APortal::BeginPlay() {
	Super::BeginPlay();

	Overlap->OnComponentBeginOverlap.AddDynamic(this, &APortal::OnOverlapBegin);

	if (!Target) {
		return;
	}

	TargetCapture->SetWorldLocation(Target->GetActorLocation());

	if (ensure(PortalMaterial)) {
		Portal->SetMaterial(0, MakeRenderMaterial(TargetCapture));

	}
}

UMaterialInstanceDynamic* APortal::MakeRenderMaterial(USceneCaptureComponent2D* CaptureToUse) {
	FVector2D ViewportSize;
	GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->InitAutoFormat(ViewportSize.X, ViewportSize.Y);
	RenderTarget->UpdateResourceImmediate(true);

	CaptureToUse->TextureTarget = RenderTarget;

	UMaterialInstanceDynamic* PortalMaterialInstance = UMaterialInstanceDynamic::Create(PortalMaterial, this);
	PortalMaterialInstance->SetTextureParameterValue(FName("Target"), RenderTarget);

	return PortalMaterialInstance;
}

// Called every frame
void APortal::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (!TargetCapture) {
		return;
	}

	UpdateCapture();
}

USceneCaptureComponent2D* APortal::GetCaptureComponent() const {
	return TargetCapture;
}

TArray<UPrimitiveComponent*> APortal::GetPortalComponents(const APortal* Requester) const {
	TArray<UPrimitiveComponent*> HiddenPortalComponents;
	PortalMeshesMap.GenerateValueArray(HiddenPortalComponents);

	HiddenPortalComponents.AddUnique(Portal);

	return HiddenPortalComponents;
}

bool APortal::CheckNeedToUpdate(FVector ActorLocation) const {
	auto RelativeLocation = ActorLocation - GetActorLocation();
	
	auto result = FVector::DotProduct(RelativeLocation, GetActorForwardVector());

	if (result >= 0) {
		return true;
	}

	return false;
}

// Big thanks to Redbox for this algorithm:
// https://wiki.unrealengine.com/Simple_Portals
// TODO: fix method for viewing portal through portal
void APortal::UpdateCapture() {
	if (!Target) {
		return;
	}

	auto PlayerCamera = GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
	auto CameraLocation = PlayerCamera->GetCameraLocation();

	if (!CheckNeedToUpdate(CameraLocation)) {
		return;
	}

	auto CaptureTransform = GetTeleportTransform(PlayerCamera->GetTransformComponent()->GetComponentTransform(), true);

	TargetCapture->SetWorldLocationAndRotation(CaptureTransform.GetLocation(), CaptureTransform.GetRotation());

	Target->UpdatePortalsInSight(this);

	// set clip plane
	// !!! This requires to enable global clip option in the project's settings
	TargetCapture->ClipPlaneNormal = Target->GetActorForwardVector();
	TargetCapture->ClipPlaneBase = Target->GetActorLocation();
	TargetCapture->bEnableClipPlane = true;

}

void APortal::UpdatePortalsInSight(const APortal* Requester) const {
	auto RequesterCapture = Requester->GetCaptureComponent();
	RequesterCapture->HiddenComponents.Empty();

	uint32 SkippedComponents = 0;
	for (TActorIterator<APortal> ActorItr(GetWorld()); ActorItr; ++ActorItr) {
		APortal* VisiblePortal = *ActorItr;

		if (VisiblePortal == Requester) {
			continue;
		}

		FVector PortalLocation = VisiblePortal->GetActorLocation();

		if (!CheckNeedToUpdate(PortalLocation)) {
			continue;
		}

		FHitResult HitResult;
		FVector CaptureLocation = RequesterCapture->GetComponentLocation();
		if (!GetWorld()->LineTraceSingleByChannel(HitResult, CaptureLocation, PortalLocation, ECollisionChannel::ECC_Camera)) {
			continue;
		}

		auto VisiblePortalMesh = VisiblePortal->RenderForPortal(Requester);

		auto HiddenPortalComponents = VisiblePortal->GetPortalComponents(Requester);
		for (UPrimitiveComponent* HiddenPortalComponent : HiddenPortalComponents) {
			if (HiddenPortalComponent->GetUniqueID() == VisiblePortalMesh->GetUniqueID()) {
				SkippedComponents += 1;
				if (Requester->bDebug) {
					UE_LOG(LogTemp, Warning, TEXT("skipped: %s / %s"), *VisiblePortal->GetName(), *HiddenPortalComponent->GetName());
				}
				continue;
			}

			RequesterCapture->HiddenComponents.AddUnique(HiddenPortalComponent);
		}
	}

	if (Requester->bDebug) {
		UE_LOG(LogTemp, Warning, TEXT("Hidden components: %d, skipped: %d"), RequesterCapture->HiddenComponents.Num(), SkippedComponents);
	}

}

UStaticMeshComponent* APortal::RenderForPortal(const APortal* Requester) {
	if (!Requester) {
		return nullptr;
	}

	if (Requester->bDebug) {
		UE_LOG(LogTemp, Warning, TEXT("Render %s for %s"), *GetName(), *Requester->GetName());
	}

	USceneCaptureComponent2D* PortalCapture;
	UStaticMeshComponent* PortalMesh;

	uint32 RequesterID = Requester->GetUniqueID();
	if (CapturesMap.Contains(RequesterID)) {
		PortalCapture = CapturesMap[RequesterID];
		PortalMesh = Cast<UStaticMeshComponent>(PortalMeshesMap[RequesterID]);
	} else {
		PortalCapture = NewObject<USceneCaptureComponent2D>(this);
		PortalCapture->RegisterComponent();
		PortalCapture->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::KeepWorld, true));
		CapturesMap.Add(RequesterID, PortalCapture);

		PortalMesh = NewObject<UStaticMeshComponent>(this);
		PortalMesh->RegisterComponent();
		PortalMesh->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, true));
		PortalMesh->SetStaticMesh(Portal->GetStaticMesh());
		PortalMesh->SetRelativeTransform(Portal->GetRelativeTransform());
		PortalMesh->SetMaterial(0, MakeRenderMaterial(PortalCapture));
		//PortalMesh->SetMaterial(0, Portal->GetMaterial(0));
		PortalMesh->SetHiddenInGame(false);
		PortalMesh->SetVisibility(true);
		
		PortalMeshesMap.Add(RequesterID, PortalMesh);
	}

	auto CaptureTransform = GetTeleportTransform(Requester->GetCaptureComponent()->GetComponentTransform(), true);

	PortalCapture->SetWorldLocationAndRotation(CaptureTransform.GetLocation(), CaptureTransform.GetRotation());
	PortalCapture->ClipPlaneNormal = Target->GetActorForwardVector();
	PortalCapture->ClipPlaneBase = Target->GetActorLocation();
	PortalCapture->bEnableClipPlane = true;

	return PortalMesh;
}

void APortal::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult) {
	if (TeleportedActors.Contains(OtherActor) || ReceivedActors.Contains(OtherActor)) {
		return;
	}

	// Temporary add the Actor to a list, to avoid duplicate teleports (becaue of multiple BeginOverlap events)
	SetCleanupTimer(OtherActor, &TeleportedActors);

	// TODO: disable teleporting when overlapping from behind
	Teleport(OtherActor);
}

FTransform APortal::GetTeleportTransform(FTransform ActorTransform, bool bCaptureTransform) const {
	FTransform SourceTransform = GetActorTransform();
	FTransform TargetTransform = Target->GetActorTransform();

	FVector SourceScale = SourceTransform.GetScale3D();
	// TODO: fix it for bCaptureTransform == false (teleportation)
	FVector InverseScale = FVector(SourceScale.X * bCaptureTransform ? -1 : 1, SourceScale.Y * -1, SourceScale.Z);
	FTransform InverseTransform = FTransform(SourceTransform.Rotator(), SourceTransform.GetLocation(), InverseScale);

	FVector CaptureLocation = TargetTransform.TransformPosition(InverseTransform.InverseTransformPosition(ActorTransform.GetLocation()));

	FRotationMatrix R = FRotationMatrix(ActorTransform.Rotator());
	FVector OUT_X;
	FVector OUT_Y;
	FVector OUT_Z;
	R.GetScaledAxes(OUT_X, OUT_Y, OUT_Z);

	auto SourceByX = FMath::GetReflectionVector(SourceTransform.InverseTransformVector(OUT_Y), FVector(1, 0, 0));

	auto SourceByXY = FMath::GetReflectionVector(SourceByX, FVector(0, 1, 0));
	auto DirectionY = TargetTransform.TransformVector(SourceByXY);

	auto TargetByX = FMath::GetReflectionVector(SourceTransform.InverseTransformVector(OUT_X), FVector(1, 0, 0));

	auto TargetByXY = FMath::GetReflectionVector(TargetByX, FVector(0, 1, 0));
	auto DirectionX = TargetTransform.TransformVector(TargetByXY);

	auto CaptureRotation = FRotationMatrix::MakeFromXY(DirectionX, DirectionY).Rotator();

	return FTransform(CaptureRotation, CaptureLocation);
}

void APortal::Teleport(AActor* Actor) {
	// Don't teleport the Portal itself suddenly
	if (Actor == this) {
		return;
	}

	if (!Target) {
		// TODO: Allow to declare some default exit point for Portals without Target
		return;
	}

	Target->TeleportReceived(Actor);
	auto Transform = GetTeleportTransform(Actor->GetActorTransform());
	Actor->SetActorLocation(Transform.GetLocation());

	auto PawnActor = Cast<APawn>(Actor);
	if (IsValid(PawnActor)) {
		auto Controller = PawnActor->GetController();
		Controller->SetControlRotation(Transform.Rotator());
	} else {
		Actor->SetActorRotation(Transform.GetRotation());
	}
}

void APortal::TeleportReceived(AActor* ReceivedActor) {
	SetCleanupTimer(ReceivedActor, &ReceivedActors);
}

void APortal::SetCleanupTimer(AActor* ActorToCleanup, TSet<AActor*>* ActorsList) {
	ActorsList->Add(ActorToCleanup);
	FTimerHandle CleanupTimer;
	GetWorld()->GetTimerManager().SetTimer(CleanupTimer, [=]() {
		ActorsList->Remove(ActorToCleanup);
	}, 0.1f, false);
}
