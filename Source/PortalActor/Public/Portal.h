// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "Portal.generated.h"

UCLASS()
class PORTALACTOR_API APortal: public AActor {
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APortal();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	UStaticMeshComponent* Frame = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	UStaticMeshComponent* Portal = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	UBoxComponent* Overlap = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	UArrowComponent* FaceArrow = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	UArrowComponent* ExitArrow = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	UMaterialInterface* PortalMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Portal")
	APortal* Target = nullptr;

	USceneCaptureComponent2D* TargetCapture = nullptr;
	UMaterialInstanceDynamic* PortalMaterialInstance = nullptr;

	void UpdateCapture();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);

	FTransform GetTeleportTransform(AActor* Actor) const;

	void Teleport(AActor* Actor);
	void TeleportReceived(AActor* ReceivedActor);

	void SetCleanupTimer(AActor* ActorToCleanup, TSet<AActor*>* ActorsList);

	TSet<AActor*> TeleportedActors;
	TSet<AActor*> ReceivedActors;

	bool bDebug = false;
};
