#include "Misc/AutomationTest.h"

#include "Core/AnastasisStateDigest.h"
#include "World/AnastasisEntityTable.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FTestEntity
	{
		FString Id;
		double X = 0.0;
	};

	TAnastasisEntityTable<FTestEntity> MakeTable(int32 Count)
	{
		TAnastasisEntityTable<FTestEntity> Table;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Table.Add(FTestEntity{ FString::Printf(TEXT("npc-%d"), Index), static_cast<double>(Index) });
		}
		return Table;
	}

	FString Order(const TAnastasisEntityTable<FTestEntity>& Table)
	{
		TArray<FString> Ids;
		for (const FTestEntity& Item : Table.GetItems())
		{
			Ids.Add(Item.Id);
		}
		return FString::Join(Ids, TEXT(","));
	}

	/** Projection canonique d'une table, comme `serialize` emet `actors`. */
	uint64 DigestOf(const TAnastasisEntityTable<FTestEntity>& Table)
	{
		AnastasisDigest::FStateWriter Writer;
		Writer.BeginArray(Table.Num());
		for (const FTestEntity& Item : Table.GetItems())
		{
			Writer.BeginObject();
			Writer.Key(TEXT("id")).String(Item.Id);
			Writer.Key(TEXT("x")).Number(Item.X);
			Writer.EndObject();
		}
		Writer.EndArray();
		return Writer.Digest();
	}
}

/**
 * La table d'entites doit se comporter comme un tableau JavaScript.
 *
 * Ce n'est pas du zele: l'ordre de `sim.actors` est observable. La boucle de
 * simulation l'itere dans l'ordre — deux habitants qui convoitent la meme
 * ressource sont departages par leur rang — et `serialize` l'emet dans l'ordre,
 * donc l'empreinte du harnais differentiel en depend.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisEntityTableOrderTest,
	"Anastasis.Sim.Entites.Ordre",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisEntityTableOrderTest::RunTest(const FString& Parameters)
{
	// 1. `push` ajoute a la fin.
	{
		TAnastasisEntityTable<FTestEntity> Table = MakeTable(4);
		TestEqual(TEXT("ajout a la fin"), Order(Table), TEXT("npc-0,npc-1,npc-2,npc-3"));
	}

	// 2. `splice(1, 1)` decale: l'ordre relatif survit.
	//
	//    C'est LE test qui compte. Le reflexe C++ est RemoveAtSwap, en O(1),
	//    qui donnerait ici "npc-0,npc-3,npc-2". Memes habitants, meme etat,
	//    ordre different — et le harnais accuserait une divergence au premier
	//    tick ou deux PNJ se disputent quelque chose. Avec trois elements
	//    l'echange donne par hasard le bon resultat; il en faut quatre pour que
	//    l'erreur se voie, et c'est pour ca que le cas en compte quatre.
	{
		TAnastasisEntityTable<FTestEntity> Table = MakeTable(4);
		Table.RemoveAt(1);
		TestEqual(TEXT("retrait par decalage, pas par echange"), Order(Table), TEXT("npc-0,npc-2,npc-3"));
		TestNotEqual(TEXT("et ce n'est pas ce qu'un echange aurait donne"), Order(Table), FString(TEXT("npc-0,npc-3,npc-2")));
	}

	// 3. L'ordre change l'empreinte. Sans cela, les deux cas ci-dessus seraient
	//    une question de gout; avec, ils sont une question de parite.
	{
		TAnastasisEntityTable<FTestEntity> Decalage = MakeTable(4);
		Decalage.RemoveAt(1);

		TAnastasisEntityTable<FTestEntity> Echange;
		Echange.Add(FTestEntity{ TEXT("npc-0"), 0.0 });
		Echange.Add(FTestEntity{ TEXT("npc-3"), 3.0 });
		Echange.Add(FTestEntity{ TEXT("npc-2"), 2.0 });

		TestNotEqual(TEXT("deux ordres = deux empreintes"), DigestOf(Decalage), DigestOf(Echange));
	}

	return true;
}

/**
 * L'index par identifiant reproduit `_actorsById`: paresseux, invalide a chaque
 * mutation. Un index perime ne rend pas une erreur — il rend le MAUVAIS
 * habitant, et la simulation continue sans rien signaler.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisEntityTableIndexTest,
	"Anastasis.Sim.Entites.Index",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisEntityTableIndexTest::RunTest(const FString& Parameters)
{
	TAnastasisEntityTable<FTestEntity> Table = MakeTable(4);

	TestEqual(TEXT("rang initial"), Table.IndexOfId(TEXT("npc-2")), 2);

	// Apres un retrait, les rangs des suivants ont bouge. Un index non invalide
	// rendrait ici l'ancien rang, donc un autre habitant.
	Table.RemoveAt(0);
	TestEqual(TEXT("rang apres retrait"), Table.IndexOfId(TEXT("npc-2")), 1);

	const FTestEntity* Found = Table.FindById(TEXT("npc-2"));
	if (TestNotNull(TEXT("retrouve par identifiant"), Found))
	{
		TestEqual(TEXT("et c'est bien le bon"), Found->X, 2.0);
	}

	// Un ajout deplace aussi la fin du tableau.
	Table.Add(FTestEntity{ TEXT("npc-9"), 9.0 });
	TestEqual(TEXT("rang d'un nouvel arrivant"), Table.IndexOfId(TEXT("npc-9")), 3);

	TestEqual(TEXT("un disparu ne se retrouve pas"), Table.IndexOfId(TEXT("npc-0")), static_cast<int32>(INDEX_NONE));
	TestNull(TEXT("et ne rend rien"), Table.FindById(TEXT("npc-0")));

	TestTrue(TEXT("retrait par identifiant"), Table.RemoveById(TEXT("npc-2")));
	TestFalse(TEXT("deux fois, non"), Table.RemoveById(TEXT("npc-2")));
	TestEqual(TEXT("ordre final"), Order(Table), TEXT("npc-1,npc-3,npc-9"));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
