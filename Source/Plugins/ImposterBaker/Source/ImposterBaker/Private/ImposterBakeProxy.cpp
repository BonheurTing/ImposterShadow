// Fill out your copyright notice in the Description page of Project Settings.


#include "ImposterBakeProxy.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "PackageTools.h"
#include "Components/SceneCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"

struct FEngineShowFlagsSetting;
DEFINE_LOG_CATEGORY(LogImposterBaker)

static const FString ImposterOutputPath = "/ImposterOutput/";

namespace
{
	FVector OctahedronToDirVector(const FVector2D& Grid)
	{
		const FVector2D GridAbs(FMath::Abs(Grid.X), FMath::Abs(Grid.Y));
		const float T1 = 1 - FVector2D::DotProduct(GridAbs, FVector2D::UnitVector);
		if (T1 >= 0)
		{
			FVector Ret(Grid.X, Grid.Y, T1);
			Ret.Normalize(0.0001f);
			return Ret;
		}
		else
		{
			FVector Ret((Grid.X >= 0 ? 1 : -1) * (1 - GridAbs.Y), (Grid.Y >= 0 ? 1 : -1) * (1 - GridAbs.X), T1);
			Ret.Normalize(0.0001f);
			return Ret;
		}
	}

	FString ChannelEnumToString(ECaptureChanel Channel)
	{
		switch (Channel)
		{
		case ECaptureChanel::AlbedoAlpha:
			return TEXT("AlbedoAlpha");
		case ECaptureChanel::NormalRoughness:
			return TEXT("NormalRoughness");
		case ECaptureChanel::DepthMetallicSpecularAO:
			return TEXT("DepthMetallicSpecularAO");
		default: check(0);
		}
		return TEXT("");
	}
	
	UMaterialInstanceConstant* CreateMaterialInstance(UMaterialInterface* Material, FString InName)
	{
		check(Material);

		TArray<UObject*> ObjectsToSync;

		// Create an appropriate and unique name .
		FString Name;
		FString PackageName;
		IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		// Use asset name only if directories are specified, otherwise full path.
		if (!InName.Contains(TEXT("/")))
		{
			FString const AssetName = Material->GetOutermost()->GetName();
			const FString SanitizedBasePackageName = UPackageTools::SanitizePackageName(AssetName);
			const FString PackagePath = FPackageName::GetLongPackagePath(SanitizedBasePackageName) + TEXT("/");
			AssetTools.CreateUniqueAssetName(PackagePath, InName, PackageName, Name);
		}
		else
		{
			InName.RemoveFromStart(TEXT("/"));
			InName.RemoveFromStart(TEXT("Content/"));
			InName.StartsWith(TEXT("Game/")) == true
				? InName.InsertAt(0, TEXT("/"))
				: InName.InsertAt(0, TEXT("/Game/"));
			AssetTools.CreateUniqueAssetName(InName, TEXT(""), PackageName, Name);
		}

		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = Material;

		UObject* NewAsset = AssetTools.CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName),
												   UMaterialInstanceConstant::StaticClass(), Factory);

		ObjectsToSync.Add(NewAsset);
		GEditor->SyncBrowserToObjects(ObjectsToSync);

		UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(NewAsset);

		return MIC;
	}
}

AImposterBakeProxy::AImposterBakeProxy()
{
	PrimaryActorTick.bCanEverTick = true;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SceneCaptureComponent2D = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	SceneCaptureComponent2D->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	SceneCaptureComponent2D->ProjectionType = ECameraProjectionMode::Orthographic;
	TArray<FEngineShowFlagsSetting> ShowFlagSettings;
	FEngineShowFlagsSetting AOShowFlags;
	AOShowFlags.ShowFlagName = TEXT("AmbientOcclusion");
	ShowFlagSettings.Add(AOShowFlags);
	SceneCaptureComponent2D->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	SceneCaptureComponent2D->bCaptureEveryFrame = 0;
	SceneCaptureComponent2D->bCaptureOnMovement = 0;
	SceneCaptureComponent2D->bAlwaysPersistRenderingState = true;
}

void AImposterBakeProxy::Bake()
{
	// Reallocate render targets.
	if (NeedToReallocateRT())
	{
		ReallocateRT();
	}

	// For each channel to capture.
	CaptureMaterialDynamicInstances.Reset();
	FCaptureView CaptureView{};
	for (auto ChannelIndex = 0u; ChannelIndex < static_cast<uint8>(ECaptureChanel::Num); ++ChannelIndex)
	{
		auto const Channel = static_cast<ECaptureChanel>(ChannelIndex);
		
		// Load capture material.
		UMaterialInterface** CaptureMaterial = CaptureMaterials.Find(Channel);
		auto const MaterialInstance = UMaterialInstanceDynamic::Create(*CaptureMaterial, this);
		CaptureMaterialDynamicInstances.Add(Channel, MaterialInstance);
		if (!CaptureMaterial || !(*CaptureMaterial))
		{
			UE_LOG(LogImposterBaker, Warning, TEXT("Capture material missing."));
			continue;
		}
		SceneCaptureComponent2D->PostProcessSettings.WeightedBlendables.Array.Reset();
		SceneCaptureComponent2D->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable{1.f, MaterialInstance});
		
		// For each direction.
		for (auto CaptureIndexX = 0u; CaptureIndexX < CaptureSubdivisionCount; ++CaptureIndexX)
		{
			for (auto CaptureIndexY = 0u; CaptureIndexY < CaptureSubdivisionCount; ++CaptureIndexY)
			{
				// Set up capture component.
				CaptureView = CalculateCaptureView(CaptureIndexX, CaptureIndexY);
				auto const Loc = CaptureView.GetCaptureLoc();
				auto const Rot = CaptureView.GetCaptureRot();
				MaterialInstance->SetScalarParameterValue(FName(TEXT("CaptureRadius")), CaptureView.CaptureRadius);
				MaterialInstance->SetScalarParameterValue(FName(TEXT("DilationRange")), DilationRange);
				MaterialInstance->SetScalarParameterValue(FName(TEXT("SceneTextureRes")), (uint32)(TextureResolution / CaptureSubdivisionCount));
				check(CapturedRenderTarget);
				SceneCaptureComponent2D->SetWorldLocationAndRotation(Loc, Rot);
				SceneCaptureComponent2D->ShowOnlyActors.Empty();
				SceneCaptureComponent2D->ShowOnlyActors.Add(MeshToBake.Get());
				SceneCaptureComponent2D->OrthoWidth = CaptureView.CaptureRadius * 2.f;
				SceneCaptureComponent2D->CaptureSource = SCS_FinalColorLDR;
				SceneCaptureComponent2D->TextureTarget = CapturedRenderTarget;

				// Capture.
				SceneCaptureComponent2D->CaptureScene();

				// Combine into atlas.
				auto const Atlas = CapturedAtlas[Channel];
				check(Atlas);
				UCanvas* Canvas;
				FVector2D CanvasSize;
				FDrawToRenderTargetContext CombineContext;
				UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(GetWorld(), Atlas, Canvas, CanvasSize, CombineContext);
				auto const SliceSize = CanvasSize / CaptureSubdivisionCount;
				auto const SliceOffset = FVector2D(static_cast<float>(CaptureIndexX) / CaptureSubdivisionCount,
					static_cast<float>(CaptureIndexY) / CaptureSubdivisionCount) * CanvasSize;
				Canvas->K2_DrawTexture(CapturedRenderTarget, SliceOffset, SliceSize, FVector2D::ZeroVector, FVector2D::UnitVector,
									   FLinearColor::White, BLEND_Opaque);
				UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(GetWorld(), CombineContext);
			}
		}
	}
	
	// Export imposter atlas and material.
	auto const ImposterMaterialInstance = CreateMaterialInstance(RenderMaterial, FPaths::Combine(ImposterOutputPath, FString::Printf(TEXT("M_%s"), *MeshToBake->GetName())));
	for (auto const Atlas : CapturedAtlas)
	{
		auto const ChannelName = ChannelEnumToString(Atlas.Key);
		const FString TexturePath = FPaths::Combine(ImposterOutputPath, FString::Printf(TEXT("T_%s_%s"), *MeshToBake->GetName(), *ChannelName));
		auto const SavedAtlas = UKismetRenderingLibrary::RenderTargetCreateStaticTexture2DEditorOnly(Atlas.Value, TexturePath, TC_Default, TMGS_FromTextureGroup);
		ImposterMaterialInstance->SetTextureParameterValueEditorOnly(FName(*ChannelName), SavedAtlas);
	}
	ImposterMaterialInstance->SetScalarParameterValueEditorOnly(FName(TEXT("ViewCount")), CaptureSubdivisionCount);
	ImposterMaterialInstance->SetScalarParameterValueEditorOnly(FName(TEXT("ImposterSize")), CaptureView.CaptureRadius * 2.f);

	// Generate imposter.
	auto const ImposterActor = GetWorld()->SpawnActor<AStaticMeshActor>(CaptureView.CaptureCenter + FVector(0.f, CaptureView.CaptureRadius, 0.f), FRotator::ZeroRotator);
	ImposterActor->GetStaticMeshComponent()->SetStaticMesh(ImposterMesh);
	ImposterActor->GetStaticMeshComponent()->SetMaterial(0, ImposterMaterialInstance);
	ImposterActor->GetStaticMeshComponent()->SetBoundsScale(CaptureView.CaptureRadius * 2.f / 100.0f);

	UE_LOG(LogImposterBaker, Display, TEXT("Bake complete."));
}

void AImposterBakeProxy::BeginPlay()
{
	Super::BeginPlay();
	ReallocateRT();
}

bool AImposterBakeProxy::NeedToReallocateRT() const
{
	auto NeedToReallocate = false;
	for (auto ChannelIndex = 0u; ChannelIndex < static_cast<uint8>(ECaptureChanel::Num); ++ChannelIndex)
	{
		auto const Channel = static_cast<ECaptureChanel>(ChannelIndex);
		if (!CapturedAtlas.Find(Channel)
			|| CapturedAtlas[Channel]->SizeX != TextureResolution
			|| CapturedAtlas[Channel]->SizeY != TextureResolution
			)
		{
			NeedToReallocate = true;
			break;
		}
	}

	return NeedToReallocate;
}

void AImposterBakeProxy::ReallocateRT()
{
	CapturedAtlas.Reset();
	for (auto ChannelIndex = 0u; ChannelIndex < static_cast<uint8>(ECaptureChanel::Num); ++ChannelIndex)
	{
		auto const Channel = static_cast<ECaptureChanel>(ChannelIndex);
		auto RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		RenderTarget->ClearColor = FLinearColor::Transparent;
		RenderTarget->bAutoGenerateMips = false;
		RenderTarget->InitAutoFormat(TextureResolution, TextureResolution);
		RenderTarget->UpdateResourceImmediate(true);
		CapturedAtlas.Add(Channel, RenderTarget);
	}
	
	CapturedRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	CapturedRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	CapturedRenderTarget->bAutoGenerateMips = false;
	CapturedRenderTarget->InitAutoFormat(TextureResolution / CaptureSubdivisionCount, TextureResolution / CaptureSubdivisionCount);
	CapturedRenderTarget->ClearColor = FLinearColor::Transparent;
	CapturedRenderTarget->UpdateResourceImmediate(true);
}

FCaptureView AImposterBakeProxy::CalculateCaptureView(uint32 IndexX, uint32 IndexY) const
{
	FCaptureView Result;

	FVector2D const Loc(static_cast<float>(IndexX) / (CaptureSubdivisionCount - 1), static_cast<float>(IndexY) / (CaptureSubdivisionCount - 1));
	Result.CaptureDirection = OctahedronToDirVector(Loc * 2 - 1);

	FVector CaptureRange;
	MeshToBake->GetActorBounds(false, Result.CaptureCenter, CaptureRange);
	Result.CaptureRadius = CaptureRange.Size();

	return Result;
}

