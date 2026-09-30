// GENERE AUTOMATIQUEMENT - ne pas editer a la main.
// Source: tools/migration/gen-parity.mjs
// Declaration: migration/parity/nous-inertia.mjs
// Reference: src/ai/algorithmic/inertia.js @ fee66ae
//
// Les valeurs attendues viennent de la reference EXECUTEE, et les doubles de
// leur motif binaire: un litteral decimal perdrait le dernier bit, et c'est
// ce bit qu'on teste.
//
// Si un cas ne passe plus: soit le portage a devie, soit la reference a change
// et il faut regenerer. Ne jamais corriger une valeur attendue a la main.

// clang-format off

// Inertia — evaluateInertia (sans danger)
struct FInertiaVector { const ANSICHAR* A0; uint64 A1Bits; const ANSICHAR* A2; uint64 A3Bits; uint64 A4Bits; uint64 A5Bits; uint64 A6Bits; int32 AttenduKeep; const ANSICHAR* AttenduReason; };
static const FInertiaVector InertiaVectors[] = {
	{ "work", 0x3fc999999999999aull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x3fe0000000000000ull, 0x0000000000000000ull, 1, "min_commit" },
	{ "work", 0x3fc999999999999aull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x3fe0000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "work", 0x3fc999999999999aull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x4004000000000000ull, 0x0000000000000000ull, 1, "switch_margin" },
	{ "work", 0x3fc999999999999aull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x4004000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "work", 0x3fc999999999999aull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x4024000000000000ull, 0x0000000000000000ull, 1, "switch_margin" },
	{ "work", 0x3fc999999999999aull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x4024000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x3fe0000000000000ull, 0x0000000000000000ull, 1, "min_commit" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x3fe0000000000000ull, 0x3ff8000000000000ull, 1, "min_commit" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x4004000000000000ull, 0x0000000000000000ull, 1, "switch_margin" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x4004000000000000ull, 0x3ff8000000000000ull, 1, "switch_margin" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x4024000000000000ull, 0x0000000000000000ull, 1, "switch_margin" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x4024000000000000ull, 0x3ff8000000000000ull, 1, "switch_margin" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x3fe0000000000000ull, 0x0000000000000000ull, 1, "min_commit" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x3fe0000000000000ull, 0x3ff8000000000000ull, 1, "min_commit" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x4004000000000000ull, 0x0000000000000000ull, 0, "candidate_better" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x4004000000000000ull, 0x3ff8000000000000ull, 0, "candidate_better" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x4024000000000000ull, 0x0000000000000000ull, 0, "candidate_better" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x4024000000000000ull, 0x3ff8000000000000ull, 0, "candidate_better" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x3fe0000000000000ull, 0x0000000000000000ull, 0, "urgency_interrupt" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x3fe0000000000000ull, 0x3ff8000000000000ull, 0, "urgency_interrupt" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x4004000000000000ull, 0x0000000000000000ull, 0, "urgency_interrupt" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x4004000000000000ull, 0x3ff8000000000000ull, 0, "urgency_interrupt" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x4024000000000000ull, 0x0000000000000000ull, 0, "urgency_interrupt" },
	{ "work", 0x3fc999999999999aull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x4024000000000000ull, 0x3ff8000000000000ull, 0, "urgency_interrupt" },
	{ "work", 0x3fc999999999999aull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x3fe0000000000000ull, 0x0000000000000000ull, 0, "urgency_interrupt" },
	{ "work", 0x3fc999999999999aull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x3fe0000000000000ull, 0x3ff8000000000000ull, 0, "urgency_interrupt" },
	{ "work", 0x3fc999999999999aull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x4004000000000000ull, 0x0000000000000000ull, 0, "urgency_interrupt" },
	{ "work", 0x3fc999999999999aull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x4004000000000000ull, 0x3ff8000000000000ull, 0, "urgency_interrupt" },
	{ "work", 0x3fc999999999999aull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x4024000000000000ull, 0x0000000000000000ull, 0, "urgency_interrupt" },
	{ "work", 0x3fc999999999999aull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x4024000000000000ull, 0x3ff8000000000000ull, 0, "urgency_interrupt" },
	{ "seek_food", 0x3fd3333333333333ull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x3fe0000000000000ull, 0x0000000000000000ull, 1, "min_commit" },
	{ "seek_food", 0x3fd3333333333333ull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x3fe0000000000000ull, 0x3ff8000000000000ull, 1, "min_commit" },
	{ "seek_food", 0x3fd3333333333333ull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x4004000000000000ull, 0x0000000000000000ull, 1, "switch_margin" },
	{ "seek_food", 0x3fd3333333333333ull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x4004000000000000ull, 0x3ff8000000000000ull, 1, "switch_margin" },
	{ "seek_food", 0x3fd3333333333333ull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x4024000000000000ull, 0x0000000000000000ull, 1, "switch_margin" },
	{ "seek_food", 0x3fd3333333333333ull, "work", 0x3fd0000000000000ull, 0x3fd3333333333333ull, 0x4024000000000000ull, 0x3ff8000000000000ull, 1, "switch_margin" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x3fe0000000000000ull, 0x0000000000000000ull, 1, "min_commit" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x3fe0000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x4004000000000000ull, 0x0000000000000000ull, 1, "switch_margin" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x4004000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x4024000000000000ull, 0x0000000000000000ull, 1, "switch_margin" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd6666666666666ull, 0x3fe0000000000000ull, 0x4024000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x3fe0000000000000ull, 0x0000000000000000ull, 1, "min_commit" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x3fe0000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x4004000000000000ull, 0x0000000000000000ull, 0, "candidate_better" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x4004000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x4024000000000000ull, 0x0000000000000000ull, 0, "candidate_better" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fe3333333333333ull, 0x3fe0000000000000ull, 0x4024000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x3fe0000000000000ull, 0x0000000000000000ull, 1, "switch_margin" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x3fe0000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x4004000000000000ull, 0x0000000000000000ull, 1, "switch_margin" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x4004000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x4024000000000000ull, 0x0000000000000000ull, 1, "switch_margin" },
	{ "seek_food", 0x3fd3333333333333ull, "seek_food", 0x3fd999999999999aull, 0x3feccccccccccccdull, 0x4024000000000000ull, 0x3ff8000000000000ull, 0, "failure_cooldown_same_action" },
	{ "seek_food", 0x3fd3333333333333ull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x3fe0000000000000ull, 0x0000000000000000ull, 0, "urgency_interrupt" },
	{ "seek_food", 0x3fd3333333333333ull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x3fe0000000000000ull, 0x3ff8000000000000ull, 0, "urgency_interrupt" },
	{ "seek_food", 0x3fd3333333333333ull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x4004000000000000ull, 0x0000000000000000ull, 0, "urgency_interrupt" },
	{ "seek_food", 0x3fd3333333333333ull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x4004000000000000ull, 0x3ff8000000000000ull, 0, "urgency_interrupt" },
	{ "seek_food", 0x3fd3333333333333ull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x4024000000000000ull, 0x0000000000000000ull, 0, "urgency_interrupt" },
	{ "seek_food", 0x3fd3333333333333ull, "sleep", 0x3fe0000000000000ull, 0x3feb851eb851eb85ull, 0x4024000000000000ull, 0x3ff8000000000000ull, 0, "urgency_interrupt" },
};

