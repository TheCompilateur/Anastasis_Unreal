// GENERE AUTOMATIQUEMENT - ne pas editer a la main.
// Source: tools/migration/gen-parity.mjs
// Declaration: migration/parity/simulation-budget.mjs
// Reference: src/sim/simulationBudget.js @ fee66ae
//
// Les valeurs attendues viennent de la reference EXECUTEE, et les doubles de
// leur motif binaire: un litteral decimal perdrait le dernier bit, et c'est
// ce bit qu'on teste.
//
// Si un cas ne passe plus: soit le portage a devie, soit la reference a change
// et il faut regenerer. Ne jamais corriger une valeur attendue a la main.

// clang-format off

// PressureTier — simulationPressureTier
struct FPressureTierVector { uint64 A0Bits; const ANSICHAR* Attendu; };
static const FPressureTierVector PressureTierVectors[] = {
	{ 0x0000000000000000ull, "normal" },
	{ 0x3fb999999999999aull, "normal" },
	{ 0x3fd7ae147ae147aeull, "normal" },
	{ 0x3fd851eb851eb852ull, "lean" },
	{ 0x3fd851e75360d024ull, "normal" },
	{ 0x3fe0000000000000ull, "lean" },
	{ 0x3fe6b851eb851eb8ull, "lean" },
	{ 0x3fe70a3d70a3d70aull, "severe" },
	{ 0x3fe70a3b57c4e2f3ull, "lean" },
	{ 0x3feccccccccccccdull, "severe" },
	{ 0x3ff0000000000000ull, "severe" },
	{ 0x3ff8000000000000ull, "severe" },
	{ 0xbfc999999999999aull, "normal" },
	{ 0x7ff8000000000000ull, "normal" },
};

// BudgetMultipliers — simulationBudgetMultipliers
struct FBudgetMultipliersVector { uint64 A0Bits; uint64 AttenduPathBudgetMulBits; uint64 AttenduAnimalTickMulBits; uint64 AttenduTransportTickMulBits; };
static const FBudgetMultipliersVector BudgetMultipliersVectors[] = {
	{ 0x0000000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull },
	{ 0x3fb999999999999aull, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull },
	{ 0x3fd7ae147ae147aeull, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull },
	{ 0x3fd851eb851eb852ull, 0x3fe5c28f5c28f5c3ull, 0x3fe3333333333333ull, 0x3fe8000000000000ull },
	{ 0x3fd851e75360d024ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull },
	{ 0x3fe0000000000000ull, 0x3fe5c28f5c28f5c3ull, 0x3fe3333333333333ull, 0x3fe8000000000000ull },
	{ 0x3fe6b851eb851eb8ull, 0x3fe5c28f5c28f5c3ull, 0x3fe3333333333333ull, 0x3fe8000000000000ull },
	{ 0x3fe70a3d70a3d70aull, 0x3fdae147ae147ae1ull, 0x3fd6666666666666ull, 0x3fe199999999999aull },
	{ 0x3fe70a3b57c4e2f3ull, 0x3fe5c28f5c28f5c3ull, 0x3fe3333333333333ull, 0x3fe8000000000000ull },
	{ 0x3feccccccccccccdull, 0x3fdae147ae147ae1ull, 0x3fd6666666666666ull, 0x3fe199999999999aull },
	{ 0x3ff0000000000000ull, 0x3fdae147ae147ae1ull, 0x3fd6666666666666ull, 0x3fe199999999999aull },
	{ 0x3ff8000000000000ull, 0x3fdae147ae147ae1ull, 0x3fd6666666666666ull, 0x3fe199999999999aull },
	{ 0xbfc999999999999aull, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull },
	{ 0x7ff8000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull },
};

// BandInterval — npcSimulationIntervalForBand
struct FBandIntervalVector { uint64 A0Bits; const ANSICHAR* A1; uint64 AttenduBits; };
static const FBandIntervalVector BandIntervalVectors[] = {
	{ 0x0000000000000000ull, "near", 0x3f91111111111111ull },
	{ 0x0000000000000000ull, "medium", 0x3fb999999999999aull },
	{ 0x0000000000000000ull, "far", 0x3ff0000000000000ull },
	{ 0x0000000000000000ull, "invisible", 0x4010000000000000ull },
	{ 0x0000000000000000ull, "inconnue", 0x4010000000000000ull },
	{ 0x3fb999999999999aull, "near", 0x3f91111111111111ull },
	{ 0x3fb999999999999aull, "medium", 0x3fbe353f7ced9168ull },
	{ 0x3fb999999999999aull, "far", 0x3ff4cccccccccccdull },
	{ 0x3fb999999999999aull, "invisible", 0x4013333333333333ull },
	{ 0x3fb999999999999aull, "inconnue", 0x4013333333333333ull },
	{ 0x3fd7ae147ae147aeull, "near", 0x3f91111111111111ull },
	{ 0x3fd7ae147ae147aeull, "medium", 0x3fc5532617c1bda5ull },
	{ 0x3fd7ae147ae147aeull, "far", 0x4000e147ae147ae1ull },
	{ 0x3fd7ae147ae147aeull, "invisible", 0x401bd70a3d70a3d7ull },
	{ 0x3fd7ae147ae147aeull, "inconnue", 0x401bd70a3d70a3d7ull },
	{ 0x3fd851eb851eb852ull, "near", 0x3f91111111111111ull },
	{ 0x3fd851eb851eb852ull, "medium", 0x3fc58e219652bd3dull },
	{ 0x3fd851eb851eb852ull, "far", 0x40011eb851eb851full },
	{ 0x3fd851eb851eb852ull, "invisible", 0x401c28f5c28f5c29ull },
	{ 0x3fd851eb851eb852ull, "inconnue", 0x401c28f5c28f5c29ull },
	{ 0x3fd851e75360d024ull, "near", 0x3f91111111111111ull },
	{ 0x3fd851e75360d024ull, "medium", 0x3fc58e2013c6b155ull },
	{ 0x3fd851e75360d024ull, "far", 0x40011eb6bf444e0eull },
	{ 0x3fd851e75360d024ull, "invisible", 0x401c28f3a9b06812ull },
	{ 0x3fd851e75360d024ull, "inconnue", 0x401c28f3a9b06812ull },
	{ 0x3fe0000000000000ull, "near", 0x3f91111111111111ull },
	{ 0x3fe0000000000000ull, "medium", 0x3fc851eb851eb852ull },
	{ 0x3fe0000000000000ull, "far", 0x4004000000000000ull },
	{ 0x3fe0000000000000ull, "invisible", 0x4020000000000000ull },
	{ 0x3fe0000000000000ull, "inconnue", 0x4020000000000000ull },
	{ 0x3fe6b851eb851eb8ull, "near", 0x3f91111111111111ull },
	{ 0x3fe6b851eb851eb8ull, "medium", 0x3fcd288ce703afb8ull },
	{ 0x3fe6b851eb851eb8ull, "far", 0x40090a3d70a3d70aull },
	{ 0x3fe6b851eb851eb8ull, "invisible", 0x40235c28f5c28f5cull },
	{ 0x3fe6b851eb851eb8ull, "inconnue", 0x40235c28f5c28f5cull },
	{ 0x3fe70a3d70a3d70aull, "near", 0x3f91111111111111ull },
	{ 0x3fe70a3d70a3d70aull, "medium", 0x3fcd63886594af50ull },
	{ 0x3fe70a3d70a3d70aull, "far", 0x400947ae147ae148ull },
	{ 0x3fe70a3d70a3d70aull, "invisible", 0x4023851eb851eb85ull },
	{ 0x3fe70a3d70a3d70aull, "inconnue", 0x4023851eb851eb85ull },
	{ 0x3fe70a3b57c4e2f3ull, "near", 0x3f91111111111111ull },
	{ 0x3fe70a3b57c4e2f3ull, "medium", 0x3fcd6386e308a367ull },
	{ 0x3fe70a3b57c4e2f3ull, "far", 0x400947ac81d3aa36ull },
	{ 0x3fe70a3b57c4e2f3ull, "invisible", 0x4023851dabe2717aull },
	{ 0x3fe70a3b57c4e2f3ull, "inconnue", 0x4023851dabe2717aull },
	{ 0x3feccccccccccccdull, "near", 0x3f91111111111111ull },
	{ 0x3feccccccccccccdull, "medium", 0x3fd0c49ba5e353f8ull },
	{ 0x3feccccccccccccdull, "far", 0x400d99999999999aull },
	{ 0x3feccccccccccccdull, "invisible", 0x4026666666666666ull },
	{ 0x3feccccccccccccdull, "inconnue", 0x4026666666666666ull },
	{ 0x3ff0000000000000ull, "near", 0x3f91111111111111ull },
	{ 0x3ff0000000000000ull, "medium", 0x3fd1eb851eb851ebull },
	{ 0x3ff0000000000000ull, "far", 0x4010000000000000ull },
	{ 0x3ff0000000000000ull, "invisible", 0x4028000000000000ull },
	{ 0x3ff0000000000000ull, "inconnue", 0x4028000000000000ull },
	{ 0x3ff8000000000000ull, "near", 0x3f91111111111111ull },
	{ 0x3ff8000000000000ull, "medium", 0x3fd1eb851eb851ebull },
	{ 0x3ff8000000000000ull, "far", 0x4010000000000000ull },
	{ 0x3ff8000000000000ull, "invisible", 0x4028000000000000ull },
	{ 0x3ff8000000000000ull, "inconnue", 0x4028000000000000ull },
	{ 0xbfc999999999999aull, "near", 0x3f91111111111111ull },
	{ 0xbfc999999999999aull, "medium", 0x3fb999999999999aull },
	{ 0xbfc999999999999aull, "far", 0x3ff0000000000000ull },
	{ 0xbfc999999999999aull, "invisible", 0x4010000000000000ull },
	{ 0xbfc999999999999aull, "inconnue", 0x4010000000000000ull },
	{ 0x7ff8000000000000ull, "near", 0x3f91111111111111ull },
	{ 0x7ff8000000000000ull, "medium", 0x3fb999999999999aull },
	{ 0x7ff8000000000000ull, "far", 0x3ff0000000000000ull },
	{ 0x7ff8000000000000ull, "invisible", 0x4010000000000000ull },
	{ 0x7ff8000000000000ull, "inconnue", 0x4010000000000000ull },
};

// NpcBand — classifyNpcSimulationBand (passe par Math.hypot)
struct FNpcBandVector { uint64 A0Bits; uint64 A1Bits; uint64 A2Bits; uint64 A3Bits; uint64 A4Bits; int32 A5; const ANSICHAR* Attendu; };
static const FNpcBandVector NpcBandVectors[] = {
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0, "near" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4032000000000000ull, 0x0000000000000000ull, 0, "near" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4031ffffef39085full, 0x0000000000000000ull, 0, "near" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4032000010c6f7a1ull, 0x0000000000000000ull, 0, "medium" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4045000000000000ull, 0x0000000000000000ull, 0, "medium" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4055000000000000ull, 0x0000000000000000ull, 0, "far" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x405500000431bde8ull, 0x0000000000000000ull, 0, "invisible" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x402974b2334f2327ull, 0x402974b2334f2327ull, 0, "near" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4008000000000000ull, 0x4010000000000000ull, 0, "near" },
	{ 0x0000000000000000ull, 0x4025000000000000ull, 0xc01d000000000000ull, 0x4035c00000000000ull, 0x402b000000000000ull, 0, "medium" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4014000000000000ull, 0x4014000000000000ull, 1, "medium" },
	{ 0x3fe0000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4032000000000000ull, 0x0000000000000000ull, 0, "medium" },
	{ 0x3fe0000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4029eb851eb851ecull, 0x0000000000000000ull, 0, "near" },
	{ 0x3ff0000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4029eb851eb851ecull, 0x0000000000000000ull, 0, "medium" },
	{ 0x3ff0000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x40513851eb851eb8ull, 0x0000000000000000ull, 0, "far" },
	{ 0x3fe70a3d70a3d70aull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4045000000000000ull, 0x0000000000000000ull, 0, "far" },
	{ 0x3fd851eb851eb852ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4055000000000000ull, 0x0000000000000000ull, 0, "invisible" },
	{ 0x0000000000000000ull, 0xc12e848000000000ull, 0x412e848000000000ull, 0x412e848000000000ull, 0xc12e848000000000ull, 0, "invisible" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4031fffffff118f0ull, 0x3f37298ebddd81fbull, 0, "near" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4044ffffffee9d18ull, 0x3f4b05d132d7c24full, 0, "medium" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x4054ffffffee9d18ull, 0x3f5b05d132d7c24full, 0, "far" },
};

