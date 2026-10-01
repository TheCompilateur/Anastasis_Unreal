// GENERE AUTOMATIQUEMENT - ne pas editer a la main.
// Source: tools/migration/gen-scenario-vectors.mjs
// Scenario: tools/migration/scenarios/endurance.json (endurance, empreinte 52f66b01c0766137)
// Reference: anastasis-ref-p3 @ fee66ae
//
// Empreintes de `serialize` au tick 0 de la REFERENCE: deserialize(scenario.save)
// dans un processus neuf, puis serialize, puis digestState (state-digest.mjs).
// Si un cas ne passe plus: soit le lecteur C++ a devie, soit le scenario a ete
// reconstruit et il faut regenerer. Ne jamais corriger une valeur a la main.

// clang-format off

static const TCHAR* const ScenarioPath = TEXT("tools/migration/scenarios/endurance.json");
static const TCHAR* const ScenarioName = TEXT("endurance");
static const TCHAR* const ScenarioEmpreinte = TEXT("52f66b01c0766137");
static constexpr int32 ScenarioW = 108;
static constexpr int32 ScenarioH = 114;
static constexpr int32 ScenarioTileDiffRows = 590;
static constexpr int32 ScenarioBuildings = 3;
static constexpr int32 ScenarioActors = 5;
static constexpr uint64 ScenarioGlobalDigest = 0x47a2a2ffc0e5d98aull;

struct FScenarioSection
{
	const TCHAR* Name;
	uint64 Digest;
	/** Dans le perimetre du scenario (`sections`). */
	bool bPerimetre;
};

static const FScenarioSection ScenarioSections[] =
{
	{ TEXT("actors"), 0xf6c5317de127a90aull, true },
	{ TEXT("animalState"), 0xbbca76b7ab229731ull, false },
	{ TEXT("animals"), 0xcb5e89842a8d1489ull, false },
	{ TEXT("buildings"), 0x3ef0d520c50b4b05ull, true },
	{ TEXT("colony"), 0xabf8d2dc8ee22955ull, false },
	{ TEXT("day"), 0x9a4469c3d3c3dd0aull, true },
	{ TEXT("districtLandmarks"), 0xfcaa0e865b93d656ull, false },
	{ TEXT("economy"), 0x4642d5ee83cd54e3ull, false },
	{ TEXT("game"), 0x0b0610171d750e71ull, false },
	{ TEXT("h"), 0xedae74c646a31ebfull, true },
	{ TEXT("life"), 0x6bcfef09dd912cc9ull, false },
	{ TEXT("logs"), 0x477cfc27c6a2c094ull, false },
	{ TEXT("market"), 0xbb6aa79fcbb05598ull, false },
	{ TEXT("mealReservations"), 0xf38bc2b4d775df77ull, true },
	{ TEXT("navCache"), 0xcb5e89842a8d1489ull, false },
	{ TEXT("navVersion"), 0x9816f4c3d1ea257full, false },
	{ TEXT("nextBuildingId"), 0x98403cc3d20da4ebull, false },
	{ TEXT("nextId"), 0x983224c3d2013ec7ull, false },
	{ TEXT("player"), 0xce921f16cbf3c2e9ull, false },
	{ TEXT("playerPersonId"), 0xaf63bc4c8601b62cull, false },
	{ TEXT("playerRng"), 0x8af35d3dad42b460ull, false },
	{ TEXT("playerTalkIntent"), 0xaf63bc4c8601b62cull, false },
	{ TEXT("playerTalkOutcome"), 0xaf63bc4c8601b62cull, false },
	{ TEXT("rng"), 0x5f8aeaf48cce9f92ull, true },
	{ TEXT("roadEvents"), 0x985b2cc3d2245173ull, false },
	{ TEXT("sagas"), 0xb599edc1e62ee604ull, false },
	{ TEXT("save"), 0x9a4469c3d3c3dd0aull, false },
	{ TEXT("seed"), 0xecd31c985456accfull, true },
	{ TEXT("settlement"), 0xfb3bfd588f80a5d0ull, false },
	{ TEXT("tileDiff"), 0xab93e285f337d4a9ull, true },
	{ TEXT("time"), 0xdf75272e7b246041ull, true },
	{ TEXT("traffic"), 0xcb5e89842a8d1489ull, false },
	{ TEXT("transport"), 0x9923859c5b14c794ull, false },
	{ TEXT("transportPhase"), 0xfead95b6b344617full, false },
	{ TEXT("w"), 0x98d92ac3d28f9320ull, true },
};

// clang-format on
