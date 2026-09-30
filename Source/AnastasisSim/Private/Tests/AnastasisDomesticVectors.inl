// GENERE AUTOMATIQUEMENT - ne pas editer a la main.
// Source: tools/migration/gen-parity.mjs
// Declaration: migration/parity/domestic.mjs
// Reference: src/life/domestic.js @ fee66ae
//
// Les valeurs attendues viennent de la reference EXECUTEE, et les doubles de
// leur motif binaire: un litteral decimal perdrait le dernier bit, et c'est
// ce bit qu'on teste.
//
// Si un cas ne passe plus: soit le portage a devie, soit la reference a change
// et il faut regenerer. Ne jamais corriger une valeur attendue a la main.

// clang-format off

// SleepQuality — sleepQuality(npc) : interieur, foyer, abri
struct FSleepQualityVector { const ANSICHAR* A0; const ANSICHAR* A1; const ANSICHAR* A2; uint64 AttenduBits; };
static const FSleepQualityVector SleepQualityVectors[] = {
	{ "", "", "", 0x3fdae147ae147ae1ull },
	{ "", "b1", "", 0x3fdae147ae147ae1ull },
	{ "b1", "b1", "", 0x3ff1eb851eb851ecull },
	{ "b2", "b1", "", 0x3fdae147ae147ae1ull },
	{ "b1", "", "b1", 0x3fe8f5c28f5c28f6ull },
	{ "", "", "b1", 0x3fdae147ae147ae1ull },
	{ "b3", "", "", 0x3fdae147ae147ae1ull },
};

