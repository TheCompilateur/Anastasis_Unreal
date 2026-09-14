// GENERE AUTOMATIQUEMENT - ne pas editer a la main.
// Source: tools/migration/gen-trace-vectors.mjs
//
// Empreintes attendues d'une trace synthetique, et le code qui rebatit
// chaque section. La trace JS des MEMES echantillons est dans
// tools/migration/fixtures/trace-synthetique.jsonl: le test C++ ecrit la
// sienne a cote, et compare-digests.mjs — l'outil du harnais — declare.

// clang-format off

static constexpr int32 TraceSpecVersion = 1;
static constexpr uint32 TraceSeed = 33344u;
static const uint64 TraceDtBits = 0x3fa1111111111111ull;
static const uint64 TraceDayLengthBits = 0x4056800000000000ull;
static constexpr int32 TraceSampleCount = 3;

static const ANSICHAR* const TraceSectionNames[] = { "clock", "counters", "village" };
static constexpr int32 TraceSectionCount = 3;

// echantillon 0, section clock
static void BuildTraceSection0_clock(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("day"));
		W.Number(FromBits(0x3ff0000000000000ull));
		W.Key(UTF8_TO_TCHAR("time"));
		W.Number(FromBits(0x4042e66666666666ull));
	W.EndObject();
}
// echantillon 0, section counters
static void BuildTraceSection0_counters(AnastasisDigest::FStateWriter& W)
{
	W.BeginArray(3);
		W.Number(FromBits(0x0000000000000000ull));
		W.Number(FromBits(0x0000000000000000ull));
		W.Number(FromBits(0x0000000000000000ull));
	W.EndArray();
}
// echantillon 0, section village
static void BuildTraceSection0_village(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("name"));
		W.String(UTF8_TO_TCHAR("Valmire"));
		W.Key(UTF8_TO_TCHAR("founded"));
		W.Bool(false);
		W.Key(UTF8_TO_TCHAR("treasury"));
		W.Number(FromBits(0x0000000000000000ull));
		W.Key(UTF8_TO_TCHAR("patron"));
		W.Null();
	W.EndObject();
}

// echantillon 1, section clock
static void BuildTraceSection1_clock(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("day"));
		W.Number(FromBits(0x3ff0000000000000ull));
		W.Key(UTF8_TO_TCHAR("time"));
		W.Number(FromBits(0x4042eaaaaaaaaaaaull));
	W.EndObject();
}
// echantillon 1, section counters
static void BuildTraceSection1_counters(AnastasisDigest::FStateWriter& W)
{
	W.BeginArray(3);
		W.Number(FromBits(0x3ff0000000000000ull));
		W.Number(FromBits(0x0000000000000000ull));
		W.Number(FromBits(0x0000000000000000ull));
	W.EndArray();
}
// echantillon 1, section village
static void BuildTraceSection1_village(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("name"));
		W.String(UTF8_TO_TCHAR("Valmire"));
		W.Key(UTF8_TO_TCHAR("founded"));
		W.Bool(true);
		W.Key(UTF8_TO_TCHAR("treasury"));
		W.Number(FromBits(0x4029000000000000ull));
		W.Key(UTF8_TO_TCHAR("patron"));
		W.String(UTF8_TO_TCHAR("Theodoros Kalligas"));
	W.EndObject();
}

// echantillon 2, section clock
static void BuildTraceSection2_clock(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("day"));
		W.Number(FromBits(0x4000000000000000ull));
		W.Key(UTF8_TO_TCHAR("time"));
		W.Number(FromBits(0x4056800000000002ull));
	W.EndObject();
}
// echantillon 2, section counters
static void BuildTraceSection2_counters(AnastasisDigest::FStateWriter& W)
{
	W.BeginArray(3);
		W.Number(FromBits(0x40a5180000000000ull));
		W.Number(FromBits(0x3ff0000000000000ull));
		W.Number(FromBits(0x3fd5555555555555ull));
	W.EndArray();
}
// echantillon 2, section village
static void BuildTraceSection2_village(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("name"));
		W.String(UTF8_TO_TCHAR("Valmire"));
		W.Key(UTF8_TO_TCHAR("founded"));
		W.Bool(true);
		W.Key(UTF8_TO_TCHAR("treasury"));
		W.Number(FromBits(0xbfb999999999999aull));
		W.Key(UTF8_TO_TCHAR("patron"));
		W.String(UTF8_TO_TCHAR("\xce\x98\xce\xb5\xcf\x8c\xce\xb4\xcf\x89\xcf\x81\xce\xbf\xcf\x82"));
		W.Key(UTF8_TO_TCHAR("roads"));
		W.BeginArray(2);
			W.BeginArray(2);
				W.Number(FromBits(0x4010000000000000ull));
				W.Number(FromBits(0x401c000000000000ull));
			W.EndArray();
			W.BeginArray(2);
				W.Number(FromBits(0x4022000000000000ull));
				W.Number(FromBits(0x4000000000000000ull));
			W.EndArray();
		W.EndArray();
	W.EndObject();
}

struct FTraceSampleVector {
	int32 T; int32 Day; uint64 TimeBits;
	const ANSICHAR* GlobalHex;
	const ANSICHAR* SectionHex[TraceSectionCount];
	void (*Build[TraceSectionCount])(AnastasisDigest::FStateWriter&);
};
static const FTraceSampleVector TraceSamples[] = {
	{ 0, 1, 0x4042e66666666666ull, "223324cc9dc1424e", { "43ace8281a4595b3", "19531d2a69b9a8ba", "6cb0f202c4fa449f" }, { &BuildTraceSection0_clock, &BuildTraceSection0_counters, &BuildTraceSection0_village } },
	{ 1, 1, 0x4042eaaaaaaaaaaaull, "02f3d86d3c5d14c3", { "72939a006aa81f4b", "f0db60d775221db3", "d0890273f74ea41c" }, { &BuildTraceSection1_clock, &BuildTraceSection1_counters, &BuildTraceSection1_village } },
	{ 2700, 2, 0x4056800000000002ull, "818368de6a59a5bd", { "a52f46c806c92698", "84bcd1fcfb9b55be", "83f43ba264adc74d" }, { &BuildTraceSection2_clock, &BuildTraceSection2_counters, &BuildTraceSection2_village } },
};
