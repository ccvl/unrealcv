// Fill out your copyright notice in the Description page of Project Settings.

// #include "RealisticRendering.h"
#include "UnrealCVPrivate.h"
#include "CameraViewMode.h"
#include "BufferVisualizationData.h"
#include "CommandDispatcher.h"
#include "Async.h"
#include "SceneViewport.h"
#include "ViewMode.h"

#define REG_VIEW(COMMAND, DESC, DELEGATE) \
		IConsoleManager::Get().RegisterConsoleCommand(TEXT(COMMAND), TEXT(DESC), \
			FConsoleCommandWithWorldDelegate::CreateStatic(DELEGATE), ECVF_Default);

FCameraViewMode::FCameraViewMode() : World(NULL), CurrentViewMode("lit") {}

FCameraViewMode::~FCameraViewMode() {}

FCameraViewMode& FCameraViewMode::Get()
{
	static FCameraViewMode Singleton;
	return Singleton;
}



void FCameraViewMode::SetCurrentBufferVisualizationMode(FString ViewMode)
{
	check(World);
	UGameViewportClient* Viewport = World->GetGameViewport();
	ApplyViewMode(EViewModeIndex::VMI_VisualizeBuffer, true, Viewport->EngineShowFlags); // This did more than just SetVisualizeBuffer(true);

	// From ShowFlags.cpp
	Viewport->EngineShowFlags.SetPostProcessing(true);
	// Viewport->EngineShowFlags.SetMaterials(false);
	Viewport->EngineShowFlags.SetMaterials(true);
	Viewport->EngineShowFlags.SetVisualizeBuffer(true);


	//Viewport->EngineShowFlags.AmbientOcclusion = 0;
	//Viewport->EngineShowFlags.ScreenSpaceAO = 0;
	//Viewport->EngineShowFlags.Decals = 0;
	//Viewport->EngineShowFlags.DynamicShadows = 0;
	//Viewport->EngineShowFlags.GlobalIllumination = 0;
	//Viewport->EngineShowFlags.ScreenSpaceReflections = 0;

	// ToneMapper needs to be disabled
	Viewport->EngineShowFlags.SetTonemapper(false);
	// TemporalAA needs to be disabled, or it will contaminate the following frame
	Viewport->EngineShowFlags.SetTemporalAA(false);

	// A complete list can be found from Engine/Config/BaseEngine.ini, Engine.BufferVisualizationMaterials
	// TODO: BaseColor is weird, check BUG.
	// No matter in which thread, ICVar needs to be set in the GameThread

	// AsyncTask is from https://answers.unrealengine.com/questions/317218/execute-code-on-gamethread.html
	// lambda is from http://stackoverflow.com/questions/26903602/an-enclosing-function-local-variable-cannot-be-referenced-in-a-lambda-body-unles
	// Make this async is risky, TODO:
	static IConsoleVariable* ICVar = IConsoleManager::Get().FindConsoleVariable(FBufferVisualizationData::GetVisualizationTargetConsoleCommandName());
	if (ICVar)
	{
		ICVar->Set(*ViewMode, ECVF_SetByCode); // TODO: Should wait here for a moment.
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("The BufferVisualization is not correctly configured."));
	}
	// AsyncTask(ENamedThreads::GameThread, [ViewMode]() {}); // & means capture by reference
	// The ICVar can only be set in GameThread, the CommandDispatcher already enforce this requirement.
}

/* Define console commands */
void FCameraViewMode::DepthWorldUnits()
{
	SetCurrentBufferVisualizationMode(TEXT("SceneDepthWorldUnits"));
}

/* Define console commands */
void FCameraViewMode::Depth()
{
	// SetCurrentBufferVisualizationMode(TEXT("SceneDepth"));
	UGameViewportClient* Viewport = World->GetGameViewport();
	FViewMode ViewMode(Viewport->EngineShowFlags);
	ViewMode.VisDepth();
}

void FCameraViewMode::Normal()
{
	SetCurrentBufferVisualizationMode(TEXT("WorldNormal"));
}

void FCameraViewMode::BaseColor()
{
	SetCurrentBufferVisualizationMode(TEXT("BaseColor"));
}

void FCameraViewMode::Lit()
{
	check(World);
	auto Viewport = World->GetGameViewport();
	ApplyViewMode(VMI_Lit, true, Viewport->EngineShowFlags);
	Viewport->EngineShowFlags.SetMaterials(true);
	Viewport->EngineShowFlags.SetLighting(true);
	Viewport->EngineShowFlags.SetPostProcessing(true);
	// ToneMapper needs to be enabled, or the screen will be very dark
	Viewport->EngineShowFlags.SetTonemapper(true);
	// TemporalAA needs to be disabled, otherwise the previous frame might contaminate current frame.
	// Check: https://answers.unrealengine.com/questions/436060/low-quality-screenshot-after-setting-the-actor-pos.html for detail
	Viewport->EngineShowFlags.SetTemporalAA(false);
}

void FCameraViewMode::Unlit()
{
	check(World);
	auto Viewport = World->GetGameViewport();
	ApplyViewMode(VMI_Unlit, true, Viewport->EngineShowFlags);
	Viewport->EngineShowFlags.SetMaterials(false);
	Viewport->EngineShowFlags.SetVertexColors(false);
	Viewport->EngineShowFlags.SetLightFunctions(false);
	Viewport->EngineShowFlags.SetLighting(false);
	Viewport->EngineShowFlags.SetAtmosphericFog(false);
}

void FCameraViewMode::DebugMode()
{
	// float DisplayGamma = FRenderTarget::GetDisplayGamma();
	// GetDisplayGamma();
	UGameViewportClient* GameViewportClient = World->GetGameViewport();
	FSceneViewport* SceneViewport = GameViewportClient->GetGameViewport();
	float DisplayGamma = SceneViewport->GetDisplayGamma();
}

void FCameraViewMode::Object()
{
	check(World);

	auto Viewport = World->GetGameViewport();
	/* This is from ShowFlags.cpp and not working, give it up.
	Viewport->EngineShowFlags.SetLighting(false);
	Viewport->EngineShowFlags.LightFunctions = 0;
	Viewport->EngineShowFlags.SetPostProcessing(false);
	Viewport->EngineShowFlags.AtmosphericFog = 0;
	Viewport->EngineShowFlags.DynamicShadows = 0;
	*/
	ApplyViewMode(VMI_Lit, true, Viewport->EngineShowFlags);
	// Need to toggle this view mode
	// Viewport->EngineShowFlags.SetMaterials(true);

	Viewport->EngineShowFlags.SetMaterials(false);
	Viewport->EngineShowFlags.SetLighting(false);
	Viewport->EngineShowFlags.SetBSPTriangles(true);
	Viewport->EngineShowFlags.SetVertexColors(true);
	Viewport->EngineShowFlags.SetPostProcessing(false);
	Viewport->EngineShowFlags.SetHMDDistortion(false);
	Viewport->EngineShowFlags.SetTonemapper(false); // This won't take effect here

	GVertexColorViewMode = EVertexColorViewMode::Color;
}


FExecStatus FCameraViewMode::SetMode(const TArray<FString>& Args) // Check input arguments
{
	UE_LOG(LogTemp, Warning, TEXT("Run SetMode %s"), *Args[0]);

	// Check args
	if (Args.Num() == 1)
	{
		FString ViewMode = Args[0].ToLower();
		bool bSuccess = true;
		if (ViewMode == "depth")
		{
			Depth();
		}
		else if (ViewMode == "depth1")
		{
			DepthWorldUnits();
		}
		else if (ViewMode == "normal")
		{
			Normal();
		}
		else if (ViewMode == "object_mask")
		{
			Object();
		}
		else if (ViewMode == "lit")
		{
			Lit();
		}
		else if (ViewMode == "unlit")
		{
			Unlit();
		}
		else if (ViewMode == "base_color")
		{
			BaseColor();
		}
		else if (ViewMode == "debug")
		{
			DebugMode();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Unrecognized ViewMode %s"), *ViewMode);
			bSuccess = false;
			return FExecStatus::Error(FString::Printf(TEXT("Can not set ViewMode to %s"), *ViewMode));
		}
		if (bSuccess)
		{
			CurrentViewMode = ViewMode;
			return FExecStatus::OK();
		}
	}
	// Send successful message back
	// The network manager should be blocked and wait for response. So the client needs to be universally accessible?
	return FExecStatus::Error(TEXT("Arguments to SetMode are incorrect"));
}


FExecStatus FCameraViewMode::GetMode(const TArray<FString>& Args) // Check input arguments
{
	UE_LOG(LogTemp, Warning, TEXT("Run GetMode, The mode is %s"), *CurrentViewMode);
	return FExecStatus::OK(CurrentViewMode);
}