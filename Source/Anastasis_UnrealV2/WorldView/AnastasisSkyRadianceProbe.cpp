#include "WorldView/AnastasisSkyRadianceProbe.h"

#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Math/Float16Color.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FString Encode(const TSharedRef<FJsonObject>& Object)
{
    FString Output;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Output));
    return Output;
}

FString ProbeError(const TCHAR* Reason)
{
    const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("error"), Reason);
    return Encode(Object);
}
}

FString UAnastasisSkyRadianceProbe::CaptureEmissiveSkyRadiance(USkyLightComponent* SkyLight)
{
    if (!SkyLight || !SkyLight->IsRegistered())
    {
        return ProbeError(TEXT("missing_or_unregistered_sky_light"));
    }

    FSHVectorRGB3 Irradiance;
    TArray<FFloat16Color> Radiance;
    SkyLight->CaptureEmissiveRadianceEnvironmentCubeMap(Irradiance, Radiance);
    const int32 PixelCount = Radiance.Num();
    if (PixelCount == 0 || PixelCount % 6 != 0)
    {
        return ProbeError(TEXT("empty_or_invalid_emissive_capture"));
    }
    const int32 PixelsPerFace = PixelCount / 6;
    const int32 Edge = FMath::RoundToInt(FMath::Sqrt(static_cast<double>(PixelsPerFace)));
    if (Edge * Edge != PixelsPerFace)
    {
        return ProbeError(TEXT("non_square_cube_faces"));
    }

    TArray<double> Luminances;
    Luminances.Reserve(PixelCount);
    double Sum = 0.0;
    int32 Nonzero = 0;
    int32 Invalid = 0;
    double FaceSum[6] = {};
    int32 FaceNonzero[6] = {};
    for (int32 Index = 0; Index < PixelCount; ++Index)
    {
        const FLinearColor Color = Radiance[Index].GetFloats();
        const double Y = 0.2126 * Color.R + 0.7152 * Color.G + 0.0722 * Color.B;
        if (!FMath::IsFinite(Y) || Y < 0.0)
        {
            ++Invalid;
            continue;
        }
        Luminances.Add(Y);
        Sum += Y;
        const int32 Face = Index / PixelsPerFace;
        FaceSum[Face] += Y;
        if (Y > 0.0)
        {
            ++Nonzero;
            ++FaceNonzero[Face];
        }
    }
    if (Luminances.IsEmpty())
    {
        return ProbeError(TEXT("no_valid_radiance_samples"));
    }
    Luminances.Sort();
    const auto Quantile = [&Luminances](double Fraction)
    {
        const int32 Index = FMath::Clamp(
            FMath::RoundToInt(Fraction * (Luminances.Num() - 1)),
            0, Luminances.Num() - 1);
        return Luminances[Index];
    };

    const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("method"), TEXT("USkyLightComponent::CaptureEmissiveRadianceEnvironmentCubeMap"));
    Object->SetStringField(TEXT("scope"), TEXT("separate_emissive_only_capture_not_live_realtime_cubemap"));
    Object->SetNumberField(TEXT("cube_edge"), Edge);
    Object->SetNumberField(TEXT("pixel_count"), PixelCount);
    Object->SetNumberField(TEXT("valid_count"), Luminances.Num());
    Object->SetNumberField(TEXT("invalid_count"), Invalid);
    Object->SetNumberField(TEXT("nonzero_count"), Nonzero);
    Object->SetNumberField(TEXT("mean_y"), Sum / Luminances.Num());
    Object->SetNumberField(TEXT("p50_y"), Quantile(0.5));
    Object->SetNumberField(TEXT("p95_y"), Quantile(0.95));
    Object->SetNumberField(TEXT("p99_y"), Quantile(0.99));
    Object->SetNumberField(TEXT("max_y"), Luminances.Last());
    Object->SetNumberField(TEXT("sky_intensity"), SkyLight->Intensity);
    Object->SetNumberField(TEXT("sky_source_type"), static_cast<int32>(SkyLight->SourceType.GetValue()));
    Object->SetBoolField(TEXT("real_time_capture"), SkyLight->bRealTimeCapture);
    Object->SetNumberField(TEXT("configured_resolution"), SkyLight->CubemapResolution);
    TArray<TSharedPtr<FJsonValue>> Faces;
    for (int32 Face = 0; Face < 6; ++Face)
    {
        const TSharedRef<FJsonObject> FaceObject = MakeShared<FJsonObject>();
        FaceObject->SetNumberField(TEXT("face"), Face);
        FaceObject->SetNumberField(TEXT("mean_y"), FaceSum[Face] / PixelsPerFace);
        FaceObject->SetNumberField(TEXT("nonzero_count"), FaceNonzero[Face]);
        Faces.Add(MakeShared<FJsonValueObject>(FaceObject));
    }
    Object->SetArrayField(TEXT("faces"), Faces);
    return Encode(Object);
}
