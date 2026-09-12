#include "AnastasisInfrastructure.h"

#include "AnastasisNumeric.h"
#include "Dom/JsonObject.h"

const double AnastasisSimFixedDt = 1.0 / 60.0;
const int32 AnastasisSimMaxStepMult = 10;
const TCHAR* const AnastasisInfrastructureContractVersion = TEXT("anastasis-infrastructure-v1");

using namespace Anastasis;

namespace
{
	/** `Number(value) || fallback` : NaN, infini et zero retombent sur le repli. */
	double NumberOr(double Value, double Fallback)
	{
		return (FMath::IsFinite(Value) && Value != 0.0) ? Value : Fallback;
	}
}

FAnastasisInfrastructure::FAnastasisInfrastructure(
	const TSharedRef<IAnastasisSimulation>& InSimulation, double InFixedDt)
	: Simulation(InSimulation)
	, FixedDt(InFixedDt)
{
}

TSharedPtr<FAnastasisInfrastructure> FAnastasisInfrastructure::Create(
	const TSharedRef<IAnastasisSimulation>& InSimulation, double InFixedDt, FString& OutError)
{
	if (!FMath::IsFinite(InFixedDt) || InFixedDt <= 0.0)
	{
		OutError = TEXT("ANASTASIS infrastructure requires a positive fixedDt");
		return nullptr;
	}

	TSharedRef<FAnastasisInfrastructure> Infrastructure =
		MakeShareable(new FAnastasisInfrastructure(InSimulation, InFixedDt));

	// L'instantane initial est verifie ICI, pas au premier tick : une
	// simulation qui demarre hors contrat doit echouer a la construction,
	// pendant qu'un appelant peut encore renoncer, et non plus tard au milieu
	// d'une trame ou il n'a plus que le choix d'afficher un monde faux.
	FAnastasisWorldSnapshot Initial;
	if (!Infrastructure->Snapshot(Initial, OutError))
	{
		return nullptr;
	}

	// Ancrer le focus sur le peuplement des que le monde existe, pour que le
	// TOUT PREMIER tick — y compris une eventuelle chauffe de l'hote — soit
	// simule a la meme cadence que tous les suivants. L'ancrer ici plutot que
	// dans un composant garantit qu'aucun consommateur (runtime, capture,
	// verification, banc de mesure) ne peut l'oublier. Voir le commentaire de
	// `SetViewFocus` pour ce que coute cet oubli.
	if (Initial.Settlement.bIsSet)
	{
		Infrastructure->SetViewFocus(Initial.Settlement.X, Initial.Settlement.Y);
	}

	OutError.Reset();
	return Infrastructure;
}

bool FAnastasisInfrastructure::SetViewFocus(double X, double Y)
{
	if (!FMath::IsFinite(X) || !FMath::IsFinite(Y))
	{
		return false;
	}

	bViewFocused = Simulation->SetViewFocus(X, Y);
	return bViewFocused;
}

FAnastasisStepPlan FAnastasisInfrastructure::MakeStepPlan(double Scale, double InFixedDt)
{
	const double S = FMath::Max(1.0, NumberOr(Scale, 1.0));
	const double Dt = FMath::Max(1e-6, NumberOr(InFixedDt, AnastasisSimFixedDt));

	FAnastasisStepPlan Plan;
	if (S <= 1.0)
	{
		Plan.StepDt = Dt;
		Plan.TargetSteps = 1;
		Plan.TimePerFrame = Dt;
		Plan.TimeGain = 1.0;
		return Plan;
	}

	const double StepMult = FMath::Min(S, static_cast<double>(AnastasisSimMaxStepMult));
	const double StepDt = Dt * StepMult;
	const double Wanted = (Dt * S) / StepDt;

	// Deux pas au maximum par trame. Au-dela de `SIM_MAX_STEP_MULT` la vitesse
	// demandee n'est plus rendue : le plafond est deliberement une PERTE de
	// gain de temps, pas une rafale de pas qui ferait exploser le cout de la
	// trame.
	Plan.StepDt = StepDt;
	Plan.TargetSteps = static_cast<int32>(FMath::Min(2.0, FMath::Max(1.0, JsRound(Wanted))));
	Plan.TimePerFrame = StepDt * Plan.TargetSteps;
	Plan.TimeGain = Plan.TimePerFrame / Dt;
	return Plan;
}

FAnastasisStepPlan FAnastasisInfrastructure::StepPlan(double Scale) const
{
	return MakeStepPlan(Scale, FixedDt);
}

bool FAnastasisInfrastructure::Step(double Dt, FString& OutError)
{
	if (!FMath::IsFinite(Dt) || Dt <= 0.0)
	{
		OutError = TEXT("ANASTASIS infrastructure step requires positive dt");
		return false;
	}

	Simulation->Tick(Dt);
	StepCount += 1;
	OutError.Reset();
	return true;
}

bool FAnastasisInfrastructure::Snapshot(FAnastasisWorldSnapshot& OutSnapshot, FString& OutError) const
{
	FAnastasisWorldSnapshot Projected;
	if (!Simulation->ProjectWorld(Projected))
	{
		OutError = TEXT("ANASTASIS world projection produced no snapshot");
		return false;
	}

	if (!AnastasisWorld::Assert(Projected, OutError))
	{
		return false;
	}

	OutSnapshot = MoveTemp(Projected);
	return true;
}

FAnastasisValidation FAnastasisInfrastructure::ValidateCommand(const FAnastasisCommand& Command)
{
	FAnastasisValidation Result;

	if (Command.Id.IsEmpty())
	{
		Result.Errors.Add(TEXT("id is required"));
	}
	if (Command.Kind.IsEmpty())
	{
		Result.Errors.Add(TEXT("kind is required"));
	}
	// « payload must be an object » n'apparait pas : un `TSharedPtr<FJsonObject>`
	// nul vaut « pas de charge utile » (le `payload != null` du JS) et tout
	// pointeur valide EST un objet. Le cas rejete cote JS — un tableau ou un
	// scalaire passe comme charge utile — ne peut survivre au decodage JSON,
	// qui est le seul endroit ou il existe encore : voir `FromJson`.

	Result.bValid = Result.Errors.Num() == 0;
	return Result;
}

FAnastasisCommandResult FAnastasisInfrastructure::Dispatch(const FAnastasisCommand& Command)
{
	FAnastasisCommandResult Result;
	Result.CommandId = Command.Id;
	Result.Kind = Command.Kind;
	Result.bAccepted = false;

	const FAnastasisValidation Validation = ValidateCommand(Command);
	if (!Validation.bValid)
	{
		Result.Reason = TEXT("invalid-command");
		Result.Errors = Validation.Errors;
		return Result;
	}

	// Le gestionnaire recoit le resultat DEJA prerempli et peut ecraser ce
	// qu'il veut, comme le `{...base, ...outcome}` du JS.
	if (!Simulation->HandleCommand(Command, Result))
	{
		Result.bAccepted = false;
		Result.Reason = TEXT("command-handler-unwired");
		return Result;
	}

	if (Result.bAccepted)
	{
		FString SnapshotError;
		Result.bHasSnapshot = Snapshot(Result.Snapshot, SnapshotError);
		if (!Result.bHasSnapshot)
		{
			// La commande a bien ete appliquee ; c'est la PROJECTION qui a
			// echoue. Ne pas transformer ca en refus : le monde a change, et
			// mentir a l'appelant sur ce point lui ferait rejouer la commande.
			Result.Errors.Add(SnapshotError);
		}
	}

	return Result;
}
