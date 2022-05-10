
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImposterBakeProxy.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogImposterBaker, Log, All);

UENUM()
enum class ECaptureChanel : uint8
{
	AlbedoAlpha,
	NormalRoughness,
	DepthMetallicSpecularAO,
	Num
};

struct FCaptureView
{
	FVector	CaptureCenter;
	float CaptureRadius;
	FVector CaptureDirection;

	FVector GetCaptureLoc() const { return CaptureCenter + CaptureDirection * CaptureRadius; }
	FRotator GetCaptureRot() const { return FRotationMatrix::MakeFromX(-CaptureDirection).Rotator(); }
};

UCLASS(BlueprintType, Blueprintable)
class IMPOSTERBAKER_API AImposterBakeProxy : public AActor
{
	GENERATED_BODY()

public:
	AImposterBakeProxy();
	
	UFUNCTION(BlueprintCallable, CallInEditor, Category = ImposterBaker)
	void Bake();
	
	virtual void BeginPlay() override;

protected:
	bool NeedToReallocateRT() const;
	void ReallocateRT();
	FCaptureView CalculateCaptureView(uint32 IndexX, uint32 IndexY) const;

public:
	UPROPERTY(EditAnywhere, Category = ImposterBakeOptions)
	uint32 CaptureSubdivisionCount = 16;

	UPROPERTY(EditAnywhere, Category = ImposterBakeOptions)
	uint32 TextureResolution = 4096;

	UPROPERTY(EditAnywhere, Category = ImposterBakeOptions)
	float DilationRange = .02f;
	
	UPROPERTY(EditAnywhere, Category = ImposterBakeOptions)
	TMap<ECaptureChanel, class UMaterialInterface*> CaptureMaterials;

	UPROPERTY(EditAnywhere, Category = ImposterRenderOptions)
	class UMaterialInterface* RenderMaterial;

	UPROPERTY(EditAnywhere, Category = ImposterRenderOptions)
	UStaticMesh* ImposterMesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImposterBaker)
	TSoftObjectPtr<AActor> MeshToBake;

protected:
	UPROPERTY()
	TMap<ECaptureChanel, class UTextureRenderTarget2D*> CapturedAtlas;

	UPROPERTY()
	class UTextureRenderTarget2D* CapturedRenderTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Capture)
	class USceneCaptureComponent2D* SceneCaptureComponent2D;
	
	TMap<ECaptureChanel, class UMaterialInstanceDynamic*> CaptureMaterialDynamicInstances;
};
