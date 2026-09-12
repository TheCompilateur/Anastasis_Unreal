#include "AnastasisActorKinematics.h"

#include "AnastasisNumeric.h"

const TCHAR* const AnastasisActorKinematicsVersion =
	TEXT("anastasis-actor-kinematics-v1-player-drive");

namespace AnastasisKinematics
{
	using namespace Anastasis;

	FAnastasisDrive NormalizePlayerDrive(double DX, double DY)
	{
		FAnastasisDrive Drive;
		const double RawX = FiniteNumber(DX);
		const double RawY = FiniteNumber(DY);
		const double Length = Hypot(RawX, RawY);
		if (Length <= 1e-5)
		{
			return Drive;
		}

		Drive.bIsSet = true;
		Drive.X = RawX / Length;
		Drive.Y = RawY / Length;
		return Drive;
	}

	bool AdvanceActorKinematics(FAnastasisKinematicBody& Body, const FAnastasisDrive& Drive,
		double Dt, const FAnastasisKinematicParams& Params)
	{
		if (!Drive.bIsSet || Body.bInside)
		{
			return false;
		}

		const double Delta = FMath::Max(0.0, FiniteNumber(Dt));
		const double EffectiveSpeed = FMath::Max(0.0, FiniteNumber(Params.Speed))
			* FMath::Max(0.0, FiniteNumber(Params.SpeedFactor, 1.0));
		const double Step = EffectiveSpeed * Delta;

		const double DriveX = FiniteNumber(Drive.X);
		const double DriveY = FiniteNumber(Drive.Y);
		const double NextX = Body.X + DriveX * Step;
		const double NextY = Body.Y + DriveY * Step;

		// Les bornes par defaut du JS sont `width - 2` / `height - 2`. Les
		// laisser calculer ici plutot que dans la structure evite qu'un
		// appelant qui ne renseigne que `Width` herite d'un `MaxX` perime.
		const double MaxX = Params.MaxX.Get(Params.Width - 2.0);
		const double MaxY = Params.MaxY.Get(Params.Height - 2.0);

		const auto IsBlocked = [&Params](double X, double Y) -> bool
		{
			return Params.IsFootBlocked ? Params.IsFootBlocked(X, Y) : false;
		};

		// `stuck` est evalue UNE FOIS, sur la position de depart. Un acteur
		// deja coince traverse : sans cette porte il resterait bloque a vie
		// dans une tuile devenue infranchissable derriere lui.
		const bool bStuck = IsBlocked(Body.X, Body.Y);
		const double BeforeX = Body.X;
		const double BeforeY = Body.Y;

		if (bStuck || !IsBlocked(NextX, Body.Y))
		{
			Body.X = FMath::Clamp(NextX, Params.MinX, MaxX);
		}
		// L'axe Y teste la position X DEJA MISE A JOUR, pas `BeforeX`.
		if (bStuck || !IsBlocked(Body.X, NextY))
		{
			Body.Y = FMath::Clamp(NextY, Params.MinY, MaxY);
		}

		return Hypot(Body.X - BeforeX, Body.Y - BeforeY) > 1e-5;
	}
}
