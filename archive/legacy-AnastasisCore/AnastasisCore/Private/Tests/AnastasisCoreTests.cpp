#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AnastasisActorKinematics.h"
#include "AnastasisInfrastructure.h"
#include "AnastasisTimeControl.h"
#include "AnastasisWorldContract.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <limits>

/**
 * Ces tests epinglent le COMPORTEMENT TRADUIT, pas le comportement souhaite.
 *
 * Chaque attente est derivee a la main du fichier JS cite en commentaire, et
 * non de ce que le C++ fait une fois ecrit. Un test ecrit dans l'autre sens
 * verrouillerait une erreur de traduction au lieu de la trouver — c'est
 * exactement ainsi que le double encodage gamma du piege n.1
 * (PITFALLS_FROM_WONDERLAND.md) a survecu une journee entiere.
 *
 * CE QU'ILS NE PROUVENT PAS : la parite [SCN]. Aucune de ces valeurs ne vient
 * d'un run de `Jeux IV Kingdoms`. Prouver la parite demande de rejouer une
 * meme graine des deux cotes et de comparer les checkpoints ; ces tests ne
 * couvrent que la frontiere, qui est ce qui a ete porte a ce stade.
 */

static const double AnastasisTestTolerance = 1e-12;

/** UE n'expose pas de constante NaN ; `<limits>` en fournit une portable. */
static const double AnastasisTestNaN = std::numeric_limits<double>::quiet_NaN();

namespace
{
	bool NearlyEqual(double A, double B, double Tolerance = AnastasisTestTolerance)
	{
		return FMath::Abs(A - B) <= Tolerance;
	}

	/** Instantane minimal valide : de quoi franchir `Validate` et rien de plus. */
	FAnastasisWorldSnapshot MakeMinimalSnapshot()
	{
		FAnastasisWorldSnapshot Snapshot;
		Snapshot.ContractVersion = AnastasisWorldContractVersion;
		Snapshot.Seed = 4242;
		Snapshot.Time = 0.0;
		Snapshot.Day = 1;
		Snapshot.DayFraction = 0.0;
		Snapshot.Dimensions.Width = 256;
		Snapshot.Dimensions.Height = 256;
		return Snapshot;
	}

	/** Simulation factice : elle ne simule rien, elle enregistre ce qu'on lui demande. */
	class FFakeSimulation : public IAnastasisSimulation
	{
	public:
		FAnastasisWorldSnapshot World = MakeMinimalSnapshot();
		double AccumulatedDt = 0.0;
		int32 TickCount = 0;
		int32 FocusCalls = 0;
		double FocusX = 0.0;
		double FocusY = 0.0;
		bool bAcceptFocus = true;
		bool bCommandHandlerWired = false;
		bool bProjectionSucceeds = true;

		virtual bool ProjectWorld(FAnastasisWorldSnapshot& OutSnapshot) const override
		{
			if (!bProjectionSucceeds)
			{
				return false;
			}
			OutSnapshot = World;
			return true;
		}

		virtual void Tick(double Dt) override
		{
			AccumulatedDt += Dt;
			TickCount += 1;
		}

		virtual bool SetViewFocus(double X, double Y) override
		{
			FocusCalls += 1;
			FocusX = X;
			FocusY = Y;
			return bAcceptFocus;
		}

		virtual bool HandleCommand(const FAnastasisCommand& Command,
			FAnastasisCommandResult& OutResult) override
		{
			if (!bCommandHandlerWired)
			{
				return false;
			}
			OutResult.bAccepted = Command.Kind == TEXT("player.incarnate");
			OutResult.Reason = OutResult.bAccepted ? TEXT("incarnated") : TEXT("unsupported-command");
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisStepPlanTest,
	"Anastasis.Core.Infrastructure.StepPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter)

bool FAnastasisStepPlanTest::RunTest(const FString&)
{
	const double Dt = AnastasisSimFixedDt;

	// `s <= 1` : chemin court, un pas d'un dt, aucun gain.
	const FAnastasisStepPlan Single = FAnastasisInfrastructure::MakeStepPlan(1.0, Dt);
	TestTrue(TEXT("scale 1 -> stepDt = fixedDt"), NearlyEqual(Single.StepDt, Dt));
	TestEqual(TEXT("scale 1 -> un seul pas"), Single.TargetSteps, 1);
	TestTrue(TEXT("scale 1 -> gain 1"), NearlyEqual(Single.TimeGain, 1.0));

	// Sous le plafond : le gain vient de la TAILLE du pas, pas de leur nombre.
	const FAnastasisStepPlan Fast = FAnastasisInfrastructure::MakeStepPlan(5.0, Dt);
	TestTrue(TEXT("scale 5 -> stepDt = 5 dt"), NearlyEqual(Fast.StepDt, Dt * 5.0));
	TestEqual(TEXT("scale 5 -> toujours un seul pas"), Fast.TargetSteps, 1);
	TestTrue(TEXT("scale 5 -> gain 5"), NearlyEqual(Fast.TimeGain, 5.0));

	// Au plafond exactement : deux pas de 10 dt donneraient 20, donc un seul.
	const FAnastasisStepPlan Capped = FAnastasisInfrastructure::MakeStepPlan(10.0, Dt);
	TestTrue(TEXT("scale 10 -> stepDt = 10 dt"), NearlyEqual(Capped.StepDt, Dt * 10.0));
	TestEqual(TEXT("scale 10 -> un seul pas"), Capped.TargetSteps, 1);
	TestTrue(TEXT("scale 10 -> gain 10"), NearlyEqual(Capped.TimeGain, 10.0));

	// Au-dela du plafond, le gain demande N'EST PAS rendu : `Math.min(2, ...)`
	// coupe a deux pas, donc 30x demande produit 20x obtenu. C'est une PERTE
	// deliberee et non un arrondi — un port qui "corrige" ce plafond fait
	// avancer le monde plus vite que la source pour la meme entree.
	const FAnastasisStepPlan Beyond = FAnastasisInfrastructure::MakeStepPlan(30.0, Dt);
	TestEqual(TEXT("scale 30 -> deux pas au maximum"), Beyond.TargetSteps, 2);
	TestTrue(TEXT("scale 30 -> gain plafonne a 20"), NearlyEqual(Beyond.TimeGain, 20.0));

	// `Math.max(1, Number(scale) || 1)` : zero, negatif et non fini valent 1.
	TestTrue(TEXT("scale 0 -> gain 1"),
		NearlyEqual(FAnastasisInfrastructure::MakeStepPlan(0.0, Dt).TimeGain, 1.0));
	TestTrue(TEXT("scale negatif -> gain 1"),
		NearlyEqual(FAnastasisInfrastructure::MakeStepPlan(-4.0, Dt).TimeGain, 1.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisDriveTest,
	"Anastasis.Core.Kinematics.NormalizeDrive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter)

bool FAnastasisDriveTest::RunTest(const FString&)
{
	const FAnastasisDrive Zero = AnastasisKinematics::NormalizePlayerDrive(0.0, 0.0);
	TestFalse(TEXT("entree nulle -> null, pas un vecteur de longueur zero"), Zero.bIsSet);

	// TOUTE entree non nulle est normalisee, meme deja unitaire ou tres longue.
	const FAnastasisDrive Diagonal = AnastasisKinematics::NormalizePlayerDrive(3.0, 4.0);
	TestTrue(TEXT("(3,4) est normalise"), Diagonal.bIsSet);
	TestTrue(TEXT("(3,4) -> x = 0.6"), NearlyEqual(Diagonal.X, 0.6, 1e-9));
	TestTrue(TEXT("(3,4) -> y = 0.8"), NearlyEqual(Diagonal.Y, 0.8, 1e-9));

	// Le seuil est 1e-5 : en dessous c'est du bruit de manette, pas une intention.
	TestFalse(TEXT("sous 1e-5 -> null"),
		AnastasisKinematics::NormalizePlayerDrive(1e-6, 0.0).bIsSet);

	TestFalse(TEXT("entree non finie -> null"),
		AnastasisKinematics::NormalizePlayerDrive(
			AnastasisTestNaN, 0.0).bIsSet);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisAdvanceTest,
	"Anastasis.Core.Kinematics.Advance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter)

bool FAnastasisAdvanceTest::RunTest(const FString&)
{
	FAnastasisKinematicParams Params;
	Params.Speed = 3.0;
	Params.Width = 64.0;
	Params.Height = 64.0;

	const FAnastasisDrive East = AnastasisKinematics::NormalizePlayerDrive(1.0, 0.0);

	FAnastasisKinematicBody Body;
	Body.X = 10.0;
	Body.Y = 10.0;
	TestTrue(TEXT("un pas vers l'est bouge"),
		AnastasisKinematics::AdvanceActorKinematics(Body, East, 1.0, Params));
	TestTrue(TEXT("un pas d'une seconde a vitesse 3 avance de 3"), NearlyEqual(Body.X, 13.0, 1e-9));
	TestTrue(TEXT("l'axe y ne bouge pas"), NearlyEqual(Body.Y, 10.0, 1e-9));

	// A l'interieur d'un batiment, aucun pas ne s'applique.
	FAnastasisKinematicBody Inside;
	Inside.X = 10.0;
	Inside.Y = 10.0;
	Inside.bInside = true;
	TestFalse(TEXT("un acteur a l'interieur ne bouge pas"),
		AnastasisKinematics::AdvanceActorKinematics(Inside, East, 1.0, Params));
	TestTrue(TEXT("sa position est intacte"), NearlyEqual(Inside.X, 10.0));

	// Bornes : `maxX = width - 2`, donc 62 pour une carte de 64.
	FAnastasisKinematicBody AtEdge;
	AtEdge.X = 61.5;
	AtEdge.Y = 10.0;
	AnastasisKinematics::AdvanceActorKinematics(AtEdge, East, 1.0, Params);
	TestTrue(TEXT("borne a width - 2"), NearlyEqual(AtEdge.X, 62.0, 1e-9));

	// ORDRE DES AXES. Un mur qui ne bloque que la colonne d'arrivee en x doit
	// laisser passer le glissement en y. Si les deux axes etaient testes contre
	// la position de depart, ou appliques dans l'autre ordre, l'acteur
	// resterait colle au mur au lieu de longer.
	FAnastasisKinematicParams Walled = Params;
	Walled.IsFootBlocked = [](double X, double /*Y*/) { return X >= 12.0; };

	FAnastasisKinematicBody Sliding;
	Sliding.X = 10.0;
	Sliding.Y = 10.0;
	const FAnastasisDrive NorthEast = AnastasisKinematics::NormalizePlayerDrive(1.0, 1.0);
	AnastasisKinematics::AdvanceActorKinematics(Sliding, NorthEast, 1.0, Walled);
	TestTrue(TEXT("l'axe x est bloque par le mur"), NearlyEqual(Sliding.X, 10.0, 1e-9));
	TestTrue(TEXT("l'axe y glisse quand meme"), Sliding.Y > 10.0);

	// Un acteur DEJA dans une tuile bloquee traverse : sans cette porte il
	// resterait coince a vie derriere un obstacle apparu sous ses pieds.
	FAnastasisKinematicParams FullyBlocked = Params;
	FullyBlocked.IsFootBlocked = [](double, double) { return true; };

	FAnastasisKinematicBody Stuck;
	Stuck.X = 10.0;
	Stuck.Y = 10.0;
	TestTrue(TEXT("un acteur deja coince peut sortir"),
		AnastasisKinematics::AdvanceActorKinematics(Stuck, East, 1.0, FullyBlocked));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisWorldValidationTest,
	"Anastasis.Core.Contract.Validate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldValidationTest::RunTest(const FString&)
{
	TestTrue(TEXT("l'instantane minimal est valide"),
		AnastasisWorld::Validate(MakeMinimalSnapshot()).bValid);

	// Les messages sont recopies mot pour mot depuis le JS : un outil de
	// comparaison qui lit les deux cotes doit voir le meme texte.
	FAnastasisWorldSnapshot WrongVersion = MakeMinimalSnapshot();
	WrongVersion.ContractVersion = TEXT("anastasis-world-v0");
	const FAnastasisValidation VersionResult = AnastasisWorld::Validate(WrongVersion);
	TestFalse(TEXT("une version inconnue invalide"), VersionResult.bValid);
	TestTrue(TEXT("message de version inchange"),
		VersionResult.Errors.Contains(TEXT("unsupported contractVersion")));

	FAnastasisWorldSnapshot BadDay = MakeMinimalSnapshot();
	BadDay.Day = 0;
	TestTrue(TEXT("jour 0 invalide"),
		AnastasisWorld::Validate(BadDay).Errors.Contains(TEXT("day is not a positive integer")));

	FAnastasisWorldSnapshot BadFraction = MakeMinimalSnapshot();
	BadFraction.DayFraction = 1.5;
	TestTrue(TEXT("fraction hors [0,1] invalide"),
		AnastasisWorld::Validate(BadFraction).Errors
			.Contains(TEXT("dayFraction is not within [0, 1]")));

	FAnastasisWorldSnapshot BadTime = MakeMinimalSnapshot();
	BadTime.Time = AnastasisTestNaN;
	TestTrue(TEXT("temps non fini invalide"),
		AnastasisWorld::Validate(BadTime).Errors.Contains(TEXT("time is not finite")));

	// `Assert` rapporte, il ne fabrique pas d'etat de secours.
	FString Error;
	TestFalse(TEXT("Assert echoue sur un instantane invalide"),
		AnastasisWorld::Assert(WrongVersion, Error));
	TestTrue(TEXT("le message d'Assert porte le prefixe du contrat"),
		Error.StartsWith(TEXT("Invalid ANASTASIS world contract:")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisWorldJsonTest,
	"Anastasis.Core.Contract.Json",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldJsonTest::RunTest(const FString&)
{
	// Document ecrit a la main d'apres la forme produite par
	// `projectAnastasisWorld`, pas capture depuis le C++ : un document capture
	// prouverait seulement que le decodeur relit son propre encodeur.
	//
	// Les apostrophes sont converties en guillemets juste apres : ecrire le
	// JSON avec des guillemets echappes le rendrait illisible, et un littoral
	// brut (`R"..."`) ne se combine pas proprement avec la macro TEXT.
	FString Document = TEXT("{")
		TEXT("'contractVersion': 'anastasis-world-v1',")
		TEXT("'seed': 4242,")
		TEXT("'time': 12.5,")
		TEXT("'day': 3,")
		TEXT("'dayFraction': 0.25,")
		TEXT("'dimensions': {'width': 256, 'height': 256},")
		TEXT("'settlement': {'name': 'Anastasis', 'x': 128.5, 'y': 130.25},")
		TEXT("'actors': [{")
		TEXT("  'id': 'npc-7', 'name': 'Lyra', 'x': 120.5, 'y': 131.0,")
		TEXT("  'jobId': 'farmer', 'gender': 'f', 'lifeStage': 'adult', 'age': 31.5,")
		TEXT("  'biome': 'plain', 'socialClass': null,")
		TEXT("  'homeId': 'building-2', 'insideBuildingId': 'building-2',")
		TEXT("  'activity': {'jobId': 'farmer', 'state': 'sleep', 'goal': null,")
		TEXT("               'workplaceId': 'building-9',")
		TEXT("               'destinationBuildingId': null, 'destination': null},")
		TEXT("  'needs': {'hunger': 0.4, 'energy': 0.9, 'thirst': 0.2, 'social': 0.5,")
		TEXT("            'leisure': 0.3, 'hygiene': 0.6, 'health': 1.0, 'morale': 0.7},")
		TEXT("  'navigation': {'pathLength': 4, 'pathIndex': 2, 'stuckTicks': 0,")
		TEXT("                 'stuckStage': 0, 'awaitingPath': false}")
		TEXT("}],")
		TEXT("'buildings': [{'id': 'building-2', 'type': 'house', 'x': 127.0, 'y': 129.0}],")
		TEXT("'resources': {'wood': 42.0, 'grain': -3.0}")
		TEXT("}");
	Document.ReplaceInline(TEXT("'"), TEXT("\""));

	TSharedPtr<FJsonObject> Parsed;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
	if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
	{
		AddError(TEXT("le document de test n'est pas du JSON valide"));
		return false;
	}

	FAnastasisWorldSnapshot Snapshot;
	FString Error;
	if (!AnastasisWorld::FromJson(Parsed, Snapshot, Error))
	{
		AddError(FString::Printf(TEXT("FromJson a echoue: %s"), *Error));
		return false;
	}

	TestTrue(TEXT("l'instantane decode respecte le contrat"),
		AnastasisWorld::Validate(Snapshot).bValid);
	TestEqual(TEXT("graine"), Snapshot.Seed, static_cast<int64>(4242));
	TestEqual(TEXT("jour"), Snapshot.Day, 3);
	TestTrue(TEXT("fraction de jour"), NearlyEqual(Snapshot.DayFraction, 0.25));
	TestTrue(TEXT("le peuplement est present"), Snapshot.Settlement.bIsSet);
	TestEqual(TEXT("un acteur"), Snapshot.Actors.Num(), 1);
	TestEqual(TEXT("un batiment"), Snapshot.Buildings.Num(), 1);

	const FAnastasisActor& Actor = Snapshot.Actors[0];
	TestEqual(TEXT("identifiant d'acteur"), Actor.Id, FString(TEXT("npc-7")));
	// `socialClass: null` devient une chaine VIDE, pas la chaine "null".
	TestTrue(TEXT("un champ null devient une chaine vide"), Actor.SocialClass.IsEmpty());
	// La situation interieure survit au decodage : c'est elle qui evite de
	// dessiner un dormeur debout dehors.
	TestEqual(TEXT("batiment occupe"), Actor.InsideBuildingId, FString(TEXT("building-2")));
	TestFalse(TEXT("une destination nulle reste nulle"), Actor.Activity.Destination.bIsSet);
	TestEqual(TEXT("longueur de chemin"), Actor.Navigation.PathLength, 4);

	// `progress` absent vaut 1 (batiment acheve), jamais 0.
	TestTrue(TEXT("progression absente = acheve"), NearlyEqual(Snapshot.Buildings[0].Progress, 1.0));

	// `copyStock` borne a >= 0 sans retirer la ressource de la liste.
	const double* Grain = Snapshot.Resources.Find(TEXT("grain"));
	TestTrue(TEXT("une quantite negative reste listee"), Grain != nullptr);
	if (Grain)
	{
		TestTrue(TEXT("une quantite negative est ramenee a 0"), NearlyEqual(*Grain, 0.0));
	}

	// Aller-retour : on compare les VALEURS DECODEES, jamais le texte.
	const TSharedRef<FJsonObject> Reencoded = AnastasisWorld::ToJson(Snapshot);
	FAnastasisWorldSnapshot RoundTrip;
	if (!AnastasisWorld::FromJson(Reencoded, RoundTrip, Error))
	{
		AddError(FString::Printf(TEXT("l'aller-retour a echoue: %s"), *Error));
		return false;
	}
	TestEqual(TEXT("aller-retour: nombre d'acteurs"), RoundTrip.Actors.Num(), Snapshot.Actors.Num());
	TestEqual(TEXT("aller-retour: identifiant"), RoundTrip.Actors[0].Id, Snapshot.Actors[0].Id);
	TestTrue(TEXT("aller-retour: position x"),
		NearlyEqual(RoundTrip.Actors[0].X, Snapshot.Actors[0].X));
	TestTrue(TEXT("aller-retour: le champ null le reste"),
		RoundTrip.Actors[0].SocialClass.IsEmpty());

	// Les defauts que `Validate` ne peut plus voir vivent ici.
	TSharedPtr<FJsonObject> NoActors = MakeShared<FJsonObject>();
	NoActors->SetStringField(TEXT("contractVersion"), AnastasisWorldContractVersion);
	FAnastasisWorldSnapshot Rejected;
	TestFalse(TEXT("actors manquant est rejete"),
		AnastasisWorld::FromJson(NoActors, Rejected, Error));
	TestEqual(TEXT("message inchange depuis le JS"), Error, FString(TEXT("actors is not an array")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisInfrastructureTest,
	"Anastasis.Core.Infrastructure.Boundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter)

bool FAnastasisInfrastructureTest::RunTest(const FString&)
{
	FString Error;

	// Un pas fixe invalide ne produit PAS une frontiere a moitie construite.
	{
		const TSharedRef<FFakeSimulation> Simulation = MakeShared<FFakeSimulation>();
		TestFalse(TEXT("un fixedDt nul est refuse"),
			FAnastasisInfrastructure::Create(Simulation, 0.0, Error).IsValid());
		TestEqual(TEXT("message de refus inchange"), Error,
			FString(TEXT("ANASTASIS infrastructure requires a positive fixedDt")));
	}

	// Un monde initial hors contrat echoue a la CONSTRUCTION, pas au premier tick.
	{
		const TSharedRef<FFakeSimulation> Simulation = MakeShared<FFakeSimulation>();
		Simulation->World.ContractVersion = TEXT("anastasis-world-v0");
		TestFalse(TEXT("un monde initial invalide est refuse"),
			FAnastasisInfrastructure::Create(Simulation, AnastasisSimFixedDt, Error).IsValid());
	}

	// LE POINT QUI COUTE CHER SI ON L'OUBLIE : le focus est ancre sur le
	// peuplement des la construction, avant le premier tick.
	{
		const TSharedRef<FFakeSimulation> Simulation = MakeShared<FFakeSimulation>();
		Simulation->World.Settlement.bIsSet = true;
		Simulation->World.Settlement.X = 128.0;
		Simulation->World.Settlement.Y = 130.0;

		const TSharedPtr<FAnastasisInfrastructure> Infrastructure =
			FAnastasisInfrastructure::Create(Simulation, AnastasisSimFixedDt, Error);
		if (!Infrastructure.IsValid())
		{
			AddError(FString::Printf(TEXT("construction impossible: %s"), *Error));
			return false;
		}

		TestEqual(TEXT("le focus est pose une fois a la construction"), Simulation->FocusCalls, 1);
		TestTrue(TEXT("le focus est sur le peuplement, pas sur l'origine"),
			NearlyEqual(Simulation->FocusX, 128.0) && NearlyEqual(Simulation->FocusY, 130.0));
		TestTrue(TEXT("la frontiere se sait focalisee"), Infrastructure->IsViewFocused());

		// `Step` NE PROJETTE PAS : il fait avancer, rien d'autre.
		TestTrue(TEXT("un pas positif est accepte"),
			Infrastructure->Step(AnastasisSimFixedDt, Error));
		TestEqual(TEXT("la simulation a bien tique"), Simulation->TickCount, 1);
		TestEqual(TEXT("le compteur de pas avance"), Infrastructure->GetStepCount(),
			static_cast<int64>(1));

		TestFalse(TEXT("un dt nul est refuse"), Infrastructure->Step(0.0, Error));
		TestEqual(TEXT("message de refus de pas inchange"), Error,
			FString(TEXT("ANASTASIS infrastructure step requires positive dt")));
		TestEqual(TEXT("un pas refuse ne fait pas tiquer la simulation"),
			Simulation->TickCount, 1);

		// Commande invalide : refusee AVANT d'atteindre la simulation.
		FAnastasisCommand Invalid;
		Invalid.Kind = TEXT("player.move");
		const FAnastasisCommandResult InvalidResult = Infrastructure->Dispatch(Invalid);
		TestFalse(TEXT("une commande sans id est refusee"), InvalidResult.bAccepted);
		TestEqual(TEXT("raison du refus"), InvalidResult.Reason, FString(TEXT("invalid-command")));
		TestTrue(TEXT("l'erreur nomme le champ manquant"),
			InvalidResult.Errors.Contains(TEXT("id is required")));
		TestFalse(TEXT("une commande refusee ne porte pas d'instantane"),
			InvalidResult.bHasSnapshot);

		// Gestionnaire non cable : la frontiere le DIT au lieu de refuser en silence.
		FAnastasisCommand Valid;
		Valid.Id = TEXT("cmd-1");
		Valid.Kind = TEXT("player.incarnate");
		const FAnastasisCommandResult Unwired = Infrastructure->Dispatch(Valid);
		TestFalse(TEXT("sans gestionnaire, rien n'est accepte"), Unwired.bAccepted);
		TestEqual(TEXT("raison explicite"), Unwired.Reason,
			FString(TEXT("command-handler-unwired")));

		// Gestionnaire cable : une commande acceptee joint un instantane.
		Simulation->bCommandHandlerWired = true;
		const FAnastasisCommandResult Accepted = Infrastructure->Dispatch(Valid);
		TestTrue(TEXT("la commande est acceptee"), Accepted.bAccepted);
		TestTrue(TEXT("une commande acceptee joint un instantane"), Accepted.bHasSnapshot);

		FAnastasisCommand Unsupported;
		Unsupported.Id = TEXT("cmd-2");
		Unsupported.Kind = TEXT("player.fly");
		const FAnastasisCommandResult Refused = Infrastructure->Dispatch(Unsupported);
		TestFalse(TEXT("une commande inconnue est refusee"), Refused.bAccepted);
		TestFalse(TEXT("et ne joint pas d'instantane"), Refused.bHasSnapshot);
	}

	// Sans peuplement, aucun focus n'est pose : la frontiere ne devine pas un
	// point de la carte.
	{
		const TSharedRef<FFakeSimulation> Simulation = MakeShared<FFakeSimulation>();
		const TSharedPtr<FAnastasisInfrastructure> Infrastructure =
			FAnastasisInfrastructure::Create(Simulation, AnastasisSimFixedDt, Error);
		TestTrue(TEXT("construction sans peuplement"), Infrastructure.IsValid());
		TestEqual(TEXT("aucun focus pose"), Simulation->FocusCalls, 0);
		TestFalse(TEXT("la frontiere ne se croit pas focalisee"),
			Infrastructure.IsValid() && Infrastructure->IsViewFocused());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTimeControlTest,
	"Anastasis.Core.Time.Speeds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter)

bool FAnastasisTimeControlTest::RunTest(const FString&)
{
	TestEqual(TEXT("quatre vitesses"), AnastasisTime::Speeds().Num(), 4);
	TestEqual(TEXT("vitesse connue conservee"), AnastasisTime::NormalizeSpeed(5), 5);
	TestEqual(TEXT("vitesse inconnue ramenee a 1"), AnastasisTime::NormalizeSpeed(3), 1);
	TestEqual(TEXT("vitesse negative ramenee a 1"), AnastasisTime::NormalizeSpeed(-2), 1);

	int32 Speed = 0;
	TestTrue(TEXT("la touche 0 est reconnue"), AnastasisTime::SpeedFromKey(TEXT('0'), Speed));
	TestEqual(TEXT("la touche 0 porte la vitesse 10"), Speed, 10);
	TestFalse(TEXT("une touche inconnue est ignoree"),
		AnastasisTime::SpeedFromKey(TEXT('9'), Speed));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
