// Fill out your copyright notice in the Description page of Project Settings.

#include "PortalActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/ArrowComponent.h"
#include "Engine/TextureRenderTarget2D.h"
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

	FaceArrow = CreateDefaultSubobject<UArrowComponent>(FName("FaceArrow"));
	FaceArrow->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, true));
	FaceArrow->SetRelativeLocation(FVector(0, 0, 0));
	auto PortalFaceRotation = GetActorForwardVector().RotateAngleAxis(180, GetActorUpVector()).Rotation();
	FaceArrow->SetWorldRotation(PortalFaceRotation);

	ExitArrow = CreateDefaultSubobject<UArrowComponent>(FName("ExitArrow"));
	ExitArrow->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::KeepWorld, true));
}

// Called when the game starts or when spawned
void APortal::BeginPlay() {
	Super::BeginPlay();

	if (bDebug) {
		FaceArrow->SetHiddenInGame(false);
		ExitArrow->SetHiddenInGame(false);
	}
	ExitArrow->SetArrowColor_New(FColor::Blue);

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

void APortal::UpdateCapture() {
	if (!Target) {
		return;
	}

	FVector TargetLocation = Target->GetActorLocation();
	FRotator TargetRotation = Target->GetActorRotation();

	FVector PortalLocation = GetActorLocation();
	FRotator PortalRotation = GetActorRotation();

	FRotator DeltaRotation = FRotator(TargetRotation.Pitch, TargetRotation.Yaw - 180, TargetRotation.Roll) - PortalRotation;
	DeltaRotation.Normalize();

	auto PlayerCameraTransform = GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetTransformComponent();

	FVector NewLocation = TargetLocation - DeltaRotation.RotateVector(PortalLocation) + DeltaRotation.RotateVector(PlayerCameraTransform->GetComponentLocation());
	FRotator NewRotation = FRotator(FQuat(DeltaRotation)*FQuat(PlayerCameraTransform->GetComponentRotation()));

	TargetCapture->SetWorldLocationAndRotation(NewLocation, NewRotation);

	if (bDebug) {
		auto TeleportTransform = GetTeleportTransform(GetWorld()->GetFirstPlayerController()->GetPawn());
		ExitArrow->SetWorldTransform(TeleportTransform);
	}

	// TODO: set clip plane
}

void APortal::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult) {
	if (TeleportedActors.Contains(OtherActor) || ReceivedActors.Contains(OtherActor)) {
		return;
	}

	// Temporary add the Actor to a list, to avoid duplicate teleports (becaue of multiple BeginOverlap events)
	SetCleanupTimer(OtherActor, &TeleportedActors);

	Teleport(OtherActor);
}

FTransform APortal::GetTeleportTransform(AActor* Actor) {
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
	return FTransform((TargetRotation + DiffRotation).Vector().RotateAngleAxis(180, Target->GetActorUpVector()).Rotation(), Target->GetActorLocation() + DiffTargetRotation.RotateVector(DiffVector));;
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