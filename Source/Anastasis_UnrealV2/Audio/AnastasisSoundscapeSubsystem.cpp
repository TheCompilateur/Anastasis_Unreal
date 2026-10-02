#include "Audio/AnastasisSoundscapeSubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "ProceduralMeshComponent.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Village/AnastasisVillagerVisual.h"
#include "WorldView/AnastasisWorldEmbodiment.h"

using namespace AnastasisSoundscape;
namespace
{
TAutoConsoleVariable<int32> Enabled(TEXT("anastasis.Soundscape.Enabled"), 1,
    TEXT("Causal village audio: visible walking, observed construction swings, rendered rivers. 0 stops all voices."));
TAutoConsoleVariable<float> Master(TEXT("anastasis.Soundscape.Volume"), 0.6f,
    TEXT("Soundscape gain, clamped 0..1."));
void Retire(FAnastasisSoundVoice& Voice)
{
    if (Voice.Component) { Voice.Component->Stop(); Voice.Component->DestroyComponent(); }
    Voice = FAnastasisSoundVoice{};
}
void Queue(USoundWaveProcedural* Wave, const TArray<int16>& PCM)
{
    Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM.GetData()), PCM.Num() * sizeof(int16));
}
}

bool UAnastasisSoundscapeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer;
}
void UAnastasisSoundscapeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UAnastasisSimulationSubsystem>();
}
bool UAnastasisSoundscapeSubsystem::IsTickable() const
{
    return !IsTemplate() && GetWorld() && GetWorld()->HasBegunPlay();
}
TStatId UAnastasisSoundscapeSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UAnastasisSoundscapeSubsystem, STATGROUP_Tickables);
}
void UAnastasisSoundscapeSubsystem::StopAll()
{
    for (auto& Voice : Voices) Retire(Voice);
    Voices.Empty();
    Retire(Water);
    Tracks.Empty();
    WaterGain = 0;
    WaterScan = 0;
    bWaterNearby = false;
}
void UAnastasisSoundscapeSubsystem::Deinitialize()
{
    StopAll();
    Super::Deinitialize();
}
FAnastasisSoundVoice UAnastasisSoundscapeSubsystem::MakeVoice(const FVector& Position, float Radius, float Gain)
{
    FAnastasisSoundVoice Voice;
    Voice.Wave = NewObject<USoundWaveProcedural>(this);
    Voice.Wave->SetSampleRate(SampleRate);
    Voice.Wave->NumChannels = 1;
    Voice.Wave->Duration = INDEFINITELY_LOOPING_DURATION;
    Voice.Wave->bLooping = false;
    Voice.Component = NewObject<UAudioComponent>(this);
    Voice.Component->bAutoActivate = false;
    Voice.Component->bAutoDestroy = false;
    Voice.Component->bIsUISound = false;
    Voice.Component->bAllowSpatialization = true;
    Voice.Component->RegisterComponentWithWorld(GetWorld());
    Voice.Component->SetWorldLocation(Position);
    FSoundAttenuationSettings Attenuation;
    Attenuation.bAttenuate = true;
    Attenuation.bSpatialize = true;
    Attenuation.AttenuationShapeExtents = FVector(150, 0, 0);
    Attenuation.FalloffDistance = Radius;
    Voice.Component->AdjustAttenuation(Attenuation); // Applied before Play.
    Voice.Component->SetSound(Voice.Wave);
    Voice.Component->SetVolumeMultiplier(Gain);
    return Voice;
}
void UAnastasisSoundscapeSubsystem::Emit(ESound Sound, const FVector& Position)
{
    // Fixed cap, no waiting queue: warp can discard gestures, never replay a backlog.
    if (Voices.Num() >= 12) { ++Dropped; return; }
    FAnastasisSoundVoice Voice = MakeVoice(Position, Sound == ESound::Work ? 3500 : 1400,
        FMath::Clamp(Master.GetValueOnGameThread(), 0.f, 1.f));
    FSynth Synth(117 + ++Serial);
    Queue(Voice.Wave, Synth.Render(Sound, SampleRate / 4));
    Voice.Expires = Clock + 0.35;
    Voice.Component->Play();
    Voices.Add(Voice);
    if (Sound == ESound::Step) ++Steps; else ++Work;
}
void UAnastasisSoundscapeSubsystem::UpdateWater(const FVector& Listener, float DeltaTime)
{
    WaterScan -= DeltaTime;
    if (WaterScan <= 0)
    {
        WaterScan = 0.5f;
        bWaterNearby = false;
        double Best = FMath::Square(4500.0);
        // Read the river section actually rendered in THIS world, not the global drainage cache.
        for (TActorIterator<AAnastasisWorldEmbodiment> It(GetWorld()); It; ++It)
        {
            if (It->IsHidden()) continue;
            TArray<UProceduralMeshComponent*> Meshes;
            It->GetComponents(Meshes);
            for (auto* Mesh : Meshes)
            {
                if (Mesh->GetFName() != TEXT("ExperimentalTerrain") || !Mesh->IsVisible()) continue;
                const FProcMeshSection* Section = Mesh->GetProcMeshSection(2);
                if (!Section || !Section->bSectionVisible) continue;
                const int32 Triangles = Section->ProcIndexBuffer.Num() / 3;
                // Bound work on dense worlds. Missing tiny triangles means silence, not invented water.
                const int32 Stride = FMath::Max(1, FMath::DivideAndRoundUp(Triangles, 12000));
                for (int32 T = 0; T < Triangles; T += Stride)
                {
                    const auto& V = Section->ProcVertexBuffer;
                    const auto& I = Section->ProcIndexBuffer;
                    const FTransform Transform = Mesh->GetComponentTransform();
                    const FVector A = Transform.TransformPosition(V[I[T * 3]].Position);
                    const FVector B = Transform.TransformPosition(V[I[T * 3 + 1]].Position);
                    const FVector C = Transform.TransformPosition(V[I[T * 3 + 2]].Position);
                    const FVector Point = FMath::ClosestPointOnTriangleToPoint(Listener, A, B, C);
                    const double Distance = FVector::DistSquared(Listener, Point);
                    if (Distance < Best) { Best = Distance; WaterTarget = Point; bWaterNearby = true; }
                }
            }
        }
    }
    if (bWaterNearby && !Water.Component)
    {
        Water = MakeVoice(WaterTarget, 4500, 0);
        Queue(Water.Wave, WaterSynth.Render(ESound::Water, SampleRate / 2));
        Water.Component->Play();
    }
    WaterGain = FMath::FInterpTo(WaterGain, bWaterNearby ? 1.f : 0.f, DeltaTime, 2.f);
    if (Water.Component)
    {
        Water.Component->SetWorldLocation(FMath::VInterpTo(Water.Component->GetComponentLocation(), WaterTarget, DeltaTime, 2.f));
        Water.Component->SetVolumeMultiplier(WaterGain * FMath::Clamp(Master.GetValueOnGameThread(), 0.f, 1.f));
        if (Water.Wave->GetAvailableAudioByteCount() < SampleRate / 2)
            Queue(Water.Wave, WaterSynth.Render(ESound::Water, SampleRate / 2));
        if (!bWaterNearby && WaterGain < 0.01f) Retire(Water);
    }
}
void UAnastasisSoundscapeSubsystem::Tick(float DeltaTime)
{
    Clock += DeltaTime;
    auto* Host = GetWorld()->GetSubsystem<UAnastasisSimulationSubsystem>();
    auto* Player = GetWorld()->GetFirstPlayerController();
    if (!Host || !Player || !Enabled.GetValueOnGameThread() || Master.GetValueOnGameThread() <= 0)
    {
        StopAll();
        PreviousSimTime = -1;
        return;
    }
    const auto& Sim = Host->GetSimulation();
    const double Now = Sim.GetTime();
    if (Now < PreviousSimTime) StopAll();
    PreviousSimTime = Now;
    for (int32 I = Voices.Num() - 1; I >= 0; --I)
        if (Clock >= Voices[I].Expires) { Retire(Voices[I]); Voices.RemoveAtSwap(I); }
    FVector Listener;
    FRotator Rotation;
    Player->GetPlayerViewPoint(Listener, Rotation);
    TSet<FString> Seen;
    for (const auto& Npc : Sim.GetVillage().GetActors())
    {
        auto* Body = Host->GetVillagePresentation().FindVillager(Npc.Id);
        if (!Body) continue;
        Seen.Add(Npc.Id);
        FObservation Observation;
        Observation.Position = Body->GetActorLocation();
        Observation.bVisible = !Body->IsHidden() && !Npc.Inside.bActive;
        const auto& Session = Npc.WorkSession;
        if (Session.bActive && Session.CraftId == TEXT("build"))
        {
            Observation.Session = Session.BuildingId;
            Observation.Swings = Session.SwingsDone;
            Observation.LastSwing = Session.LastSwingAt;
        }
        const FEvents Events = Tracks.FindOrAdd(Npc.Id).Observe(Observation, DeltaTime);
        const double Distance = FVector::DistSquared(Listener, Observation.Position);
        if (Events.bStep && Distance < FMath::Square(1550.0)) Emit(ESound::Step, Observation.Position);
        if (Events.bWork && Distance < FMath::Square(3650.0)) Emit(ESound::Work, Observation.Position);
    }
    for (auto It = Tracks.CreateIterator(); It; ++It) if (!Seen.Contains(It.Key())) It.RemoveCurrent();
    UpdateWater(Listener, DeltaTime);
}
FString UAnastasisSoundscapeSubsystem::Snapshot() const
{
    return FString::Printf(TEXT("{\"steps\":%d,\"work\":%d,\"dropped\":%d,\"voices\":%d,\"tracks\":%d,\"water\":%s,\"water_bytes\":%d,\"enabled\":%s}"),
        Steps, Work, Dropped, Voices.Num(), Tracks.Num(), Water.Component ? TEXT("true") : TEXT("false"),
        Water.Wave ? Water.Wave->GetAvailableAudioByteCount() : 0, Enabled.GetValueOnGameThread() ? TEXT("true") : TEXT("false"));
}
FString UAnastasisSoundscapeDebugLibrary::GetSoundscapeStatus(const UObject* Context)
{
    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
    const auto* Soundscape = World ? World->GetSubsystem<UAnastasisSoundscapeSubsystem>() : nullptr;
    return Soundscape ? Soundscape->Snapshot() : TEXT("{}");
}
