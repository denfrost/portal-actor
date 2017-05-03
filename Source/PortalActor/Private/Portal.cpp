// Fill out your copyright notice in the Description page of Project Settings.

#include "PortalActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "DrawDebugHelpers.h"
#include "Components/ArrowComponent.h"
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
	TargetCapture->bEnableClipPlane = true;

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
		FVector2D ViewportSize;
		GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);

		UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		RenderTarget->InitAutoFormat(ViewportSize.X, ViewportSize.Y);
		RenderTarget->UpdateResourceImmediate(true);

		PortalMaterialInstance = UMaterialInstanceDynamic::Create(PortalMaterial, this);
		PortalMaterialInstance->SetTextureParameterValue(FName("Target"), RenderTarget);
		Portal->SetMaterial(0, PortalMaterialInstance);

		TargetCapture->TextureTarget = RenderTarget;
	}
}

// Called every frame
void APortal::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (!TargetCapture) {
		return;
	}

	UpdateCapture();
}

FVector APortal::GetPortalCenter() const {
	return Portal->GetComponentLocation();
}

bool APortal::CheckNeedToUpdate() {
	auto CameraLocation = GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetTransformComponent()->GetComponentLocation();
	auto RelativeLocation = CameraLocation - GetActorLocation();
	
	auto result = FVector::DotProduct(RelativeLocation, GetActorForwardVector());

	if (result >= 0) {
		return true;
	}

	return false;
}

// Big thanks to Redbox for this algorithm:
// https://wiki.unrealengine.com/Simple_Portals
void APortal::UpdateCapture() {
	if (!Target) {
		return;
	}

	if (!CheckNeedToUpdate()) {
		return;
	}

	auto PlayerCamera = GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
	FTransform CameraTransform = PlayerCamera->GetTransformComponent()->GetComponentTransform();
	FTransform SourceTransform = GetActorTransform();
	FTransform TargetTransform = Target->GetActorTransform();

	FVector SourceScale = SourceTransform.GetScale3D();
	FVector InverseScale = FVector(SourceScale.X * -1, SourceScale.Y * -1, SourceScale.Z);
	FTransform InverseTransform = FTransform(SourceTransform.Rotator(), SourceTransform.GetLocation(), InverseScale);

	FVector CaptureLocation = TargetTransform.TransformPosition(InverseTransform.InverseTransformPosition(CameraTransform.GetLocation()));

	FRotationMatrix R = FRotationMatrix(CameraTransform.Rotator());
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

	TargetCapture->SetWorldLocationAndRotation(CaptureLocation, CaptureRotation);

	// set clip plane
	// !!! This requires to enable global clip option in the project's settings
	TargetCapture->ClipPlaneNormal = Target->GetActorForwardVector();
	TargetCapture->ClipPlaneBase = Target->GetActorLocation();
}

void APortal::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult) {
	if (TeleportedActors.Contains(OtherActor) || ReceivedActors.Contains(OtherActor)) {
		return;
	}

	// Temporary add the Actor to a list, to avoid duplicate teleports (becaue of multiple BeginOverlap events)
	SetCleanupTimer(OtherActor, &TeleportedActors);

	Teleport(OtherActor);
}

FTransform APortal::GetTeleportTransform(AActor* Actor) const {
	// Adjust relative rotation
	auto PortalRotation = GetActorForwardVector().RotateAngleAxis(180, GetActorUpVector()).Rotation();
	auto TargetRotation = Target->GetActorForwardVector().RotateAngleAxis(180, Target->GetActorUpVector()).Rotation();
	auto PawnActor = Cast<APawn>(Actor);
	FRotator DiffRotation;
	if (IsValid(PawnActor)) {
		auto Controller = PawnActor->GetController();
		DiffRotation = Controller->GetControlRotation() - PortalRotation;
	} else {
		DiffRotation = Actor->GetActorRotation() - PortalRotation;
		// TODO: fix actor's movement vector
	}

	// Adjust relative location betwen a Portal and the Actor
	auto DiffTargetRotation = TargetRotation - PortalRotation;
	auto DiffVector = Actor->GetActorLocation() - GetActorLocation();

	// TODO: fix teleportation when rotating not around Z axis
	return FTransform((TargetRotation + DiffRotation).Vector().RotateAngleAxis(180, Target->GetActorUpVector()).Rotation(), Target->GetActorLocation() + DiffTargetRotation.RotateVector(DiffVector));
}

void APortal::Teleport(AActor* Actor) {
	// Don't teleport the Portal itself suddenly
	if (Actor == this) {
		return;
	}

	if (!Target) {
		// TODO: Declare some default exit point for Portals without Target
		return;
	}

	Target->TeleportReceived(Actor);
	auto Transform = GetTeleportTransform(Actor);
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
