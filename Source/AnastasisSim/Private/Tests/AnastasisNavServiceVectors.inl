// GENERE AUTOMATIQUEMENT - ne pas editer a la main.
// Source: tools/migration/gen-parity.mjs
// Declaration: migration/parity/nav-service.mjs
// Reference: src/sim/navService.js @ fee66ae
//
// Les valeurs attendues viennent de la reference EXECUTEE, et les doubles de
// leur motif binaire: un litteral decimal perdrait le dernier bit, et c'est
// ce bit qu'on teste.
//
// Si un cas ne passe plus: soit le portage a devie, soit la reference a change
// et il faut regenerer. Ne jamais corriger une valeur attendue a la main.

// clang-format off

// NavStepMult — navStepMultForSpeed — stepDt / SIM_FIXED_DT
struct FNavStepMultVector { uint64 A0Bits; uint64 AttenduBits; };
static const FNavStepMultVector NavStepMultVectors[] = {
	{ 0x0000000000000000ull, 0x3ff0000000000000ull },
	{ 0x3fe0000000000000ull, 0x3ff0000000000000ull },
	{ 0x3ff0000000000000ull, 0x3ff0000000000000ull },
	{ 0x3ff000010c6f7a0bull, 0x3ff000010c6f7a0bull },
	{ 0x3ff8000000000000ull, 0x3ff8000000000000ull },
	{ 0x4000000000000000ull, 0x4000000000000000ull },
	{ 0x4008000000000000ull, 0x4008000000000000ull },
	{ 0x4014000000000000ull, 0x4014000000000000ull },
	{ 0x4020000000000000ull, 0x4020000000000000ull },
	{ 0x4024000000000000ull, 0x4024000000000000ull },
	{ 0x4028000000000000ull, 0x4024000000000000ull },
	{ 0x4034000000000000ull, 0x4024000000000000ull },
	{ 0x7ff8000000000000ull, 0x3ff0000000000000ull },
};

// NavCacheTtl — navCacheTtlForSpeed — TTL sim qui suit le fat-step
struct FNavCacheTtlVector { uint64 A0Bits; uint64 AttenduBits; };
static const FNavCacheTtlVector NavCacheTtlVectors[] = {
	{ 0x0000000000000000ull, 0x4034000000000000ull },
	{ 0x3fe0000000000000ull, 0x4034000000000000ull },
	{ 0x3ff0000000000000ull, 0x4034000000000000ull },
	{ 0x3ff000010c6f7a0bull, 0x403400014f8b588eull },
	{ 0x3ff8000000000000ull, 0x403e000000000000ull },
	{ 0x4000000000000000ull, 0x4044000000000000ull },
	{ 0x4008000000000000ull, 0x404e000000000000ull },
	{ 0x4014000000000000ull, 0x4059000000000000ull },
	{ 0x4020000000000000ull, 0x4064000000000000ull },
	{ 0x4024000000000000ull, 0x4069000000000000ull },
	{ 0x4028000000000000ull, 0x4069000000000000ull },
	{ 0x4034000000000000ull, 0x4069000000000000ull },
	{ 0x7ff8000000000000ull, 0x4034000000000000ull },
};

// NavSweepInterval — navCacheSweepIntervalForSpeed
struct FNavSweepIntervalVector { uint64 A0Bits; uint64 AttenduBits; };
static const FNavSweepIntervalVector NavSweepIntervalVectors[] = {
	{ 0x0000000000000000ull, 0x4010000000000000ull },
	{ 0x3fe0000000000000ull, 0x4010000000000000ull },
	{ 0x3ff0000000000000ull, 0x4010000000000000ull },
	{ 0x3ff000010c6f7a0bull, 0x401000010c6f7a0bull },
	{ 0x3ff8000000000000ull, 0x4018000000000000ull },
	{ 0x4000000000000000ull, 0x4020000000000000ull },
	{ 0x4008000000000000ull, 0x4028000000000000ull },
	{ 0x4014000000000000ull, 0x4034000000000000ull },
	{ 0x4020000000000000ull, 0x4040000000000000ull },
	{ 0x4024000000000000ull, 0x4044000000000000ull },
	{ 0x4028000000000000ull, 0x4044000000000000ull },
	{ 0x4034000000000000ull, 0x4044000000000000ull },
	{ 0x7ff8000000000000ull, 0x4010000000000000ull },
};

// NavPathBudget — pathBudgetForSpeed — un NOMBRE DE CALCULS, jamais une duree
struct FNavPathBudgetVector { uint64 A0Bits; int32 Attendu; };
static const FNavPathBudgetVector NavPathBudgetVectors[] = {
	{ 0x0000000000000000ull, 6 },
	{ 0x3fe0000000000000ull, 6 },
	{ 0x3ff0000000000000ull, 6 },
	{ 0x3ff000010c6f7a0bull, 8 },
	{ 0x3ff8000000000000ull, 8 },
	{ 0x4000000000000000ull, 10 },
	{ 0x4008000000000000ull, 14 },
	{ 0x4014000000000000ull, 20 },
	{ 0x4020000000000000ull, 29 },
	{ 0x4024000000000000ull, 34 },
	{ 0x4028000000000000ull, 34 },
	{ 0x4034000000000000ull, 34 },
	{ 0x7ff8000000000000ull, 6 },
};

// NavCacheKey — cacheKeyFor — exacte et par zone
struct FNavCacheKeyVector { uint64 A0Bits; uint64 A1Bits; uint64 A2Bits; uint64 A3Bits; int32 A4; int32 A5; const ANSICHAR* Attendu; };
static const FNavCacheKeyVector NavCacheKeyVectors[] = {
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0, 0, "0,0:0,0:0" },
	{ 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0, 1, "z0,0:0,0:0" },
	{ 0x400d99999999999aull, 0x4022666666666666ull, 0x40440ccccccccccdull, 0x4029cccccccccccdull, 0, 0, "3,9:40,12:0" },
	{ 0x400d99999999999aull, 0x4022666666666666ull, 0x40440ccccccccccdull, 0x4029cccccccccccdull, 0, 1, "z0,1:40,12:0" },
	{ 0x400d99999999999aull, 0x4022666666666666ull, 0x40440ccccccccccdull, 0x4029cccccccccccdull, 7, 0, "3,9:40,12:7" },
	{ 0x400d99999999999aull, 0x4022666666666666ull, 0x40440ccccccccccdull, 0x4029cccccccccccdull, 7, 1, "z0,1:40,12:7" },
	{ 0x4020000000000000ull, 0x4020000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 3, 1, "z1,1:1,1:3" },
	{ 0x401fffffbce4217dull, 0x401fffffbce4217dull, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 3, 1, "z0,0:1,1:3" },
	{ 0x4030000000000000ull, 0x0000000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 3, 1, "z2,0:1,1:3" },
	{ 0xbfe0000000000000ull, 0xbfe0000000000000ull, 0x4010000000000000ull, 0x4010000000000000ull, 1, 0, "-1,-1:4,4:1" },
	{ 0xbfe0000000000000ull, 0xbfe0000000000000ull, 0x4010000000000000ull, 0x4010000000000000ull, 1, 1, "z-1,-1:4,4:1" },
	{ 0xc021000000000000ull, 0xc030800000000000ull, 0xc004000000000000ull, 0xc00c000000000000ull, 2, 1, "z-2,-3:-3,-4:2" },
	{ 0xbff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0, 1, "z-1,-1:-1,-1:0" },
	{ 0x408ffb3333333333ull, 0x409ffe6666666666ull, 0x40affe3333333333ull, 0x40bfffe666666666ull, 123456, 0, "1023,2047:4095,8191:123456" },
};

