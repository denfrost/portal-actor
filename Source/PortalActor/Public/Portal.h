// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "Portal.generated.h"

class UArrowComponent;

UCLASS()
class PORTALACTOR_API APortal: public AActor {
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APortal();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	USceneCaptureComponent2D* GetCaptureComponent() const;
	TArray<UPrimitiveComponent*> GetPortalComponents(const APortal* Requester) const;

	void UpdatePortalsInSight(const APortal* Requester) const;
	UStaticMeshComponent* RenderForPortal(const APortal* Requester);

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
	UMaterialInstanceDynamic* MakeRenderMaterial(USceneCaptureComponent2D* CaptureToUse);

	bool CheckNeedToUpdate(FVector ActorLocation) const;
	void UpdateCapture();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);

	FTransform GetTeleportTransform(FTransform ActorTransform, bool bCaptureTransform = false) const;

	void Teleport(AActor* Actor);
	void TeleportReceived(AActor* ReceivedActor);

	void SetCleanupTimer(AActor* ActorToCleanup, TSet<AActor*>* ActorsList);

	TMap<uint32, USceneCaptureComponent2D*> CapturesMap;
	TMap<uint32, UPrimitiveComponent*> PortalMeshesMap;

	TSet<AActor*> TeleportedActors;
	TSet<AActor*> ReceivedActors;

	// Debug stuff
	UPROPERTY(EditAnywhere, Category = "Portal")
	bool bDebug = false;

	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	UArrowComponent* ForwardArrow;
};
