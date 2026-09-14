// GENERE AUTOMATIQUEMENT - ne pas editer a la main.
// Source: tools/migration/gen-digest-vectors.mjs
//
// Chaque cas est declare une seule fois, en instructions pour l'ecrivain
// d'etat. L'empreinte attendue vient de tools/migration/state-digest.mjs,
// c'est-a-dire du code que le harnais fait tourner pour de vrai.
//
// Si un cas ne passe plus: soit le C++ a devie, soit la specification JS a
// change et il faut regenerer. Ne jamais corriger une valeur attendue a la
// main pour faire passer un test.

static constexpr int32 DigestSpecVersion = 1;

// clang-format off

// scalaire.null
static void BuildDigestVector0(AnastasisDigest::FStateWriter& W)
{
	W.Null();
}

// scalaire.vrai
static void BuildDigestVector1(AnastasisDigest::FStateWriter& W)
{
	W.Bool(true);
}

// scalaire.faux
static void BuildDigestVector2(AnastasisDigest::FStateWriter& W)
{
	W.Bool(false);
}

// nombre.zero
static void BuildDigestVector3(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x0000000000000000ull));
}

// nombre.zero-negatif
static void BuildDigestVector4(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x8000000000000000ull));
}

// nombre.un
static void BuildDigestVector5(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x3ff0000000000000ull));
}

// nombre.demi
static void BuildDigestVector6(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x3fe0000000000000ull));
}

// nombre.tiers
static void BuildDigestVector7(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x3fd5555555555555ull));
}

// nombre.dt-du-harnais
static void BuildDigestVector8(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x3fa1111111111111ull));
}

// nombre.negatif
static void BuildDigestVector9(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0xc071126666666666ull));
}

// nombre.tres-grand
static void BuildDigestVector10(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x444b1ae4d6e2ef50ull));
}

// nombre.tres-petit
static void BuildDigestVector11(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x0000000000000001ull));
}

// nombre.infini
static void BuildDigestVector12(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x7ff0000000000000ull));
}

// nombre.infini-negatif
static void BuildDigestVector13(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0xfff0000000000000ull));
}

// nombre.nan
static void BuildDigestVector14(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x7ff8000000000000ull));
}

// nombre.ulp-a
static void BuildDigestVector15(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x4029000000000000ull));
}

// nombre.ulp-b
static void BuildDigestVector16(AnastasisDigest::FStateWriter& W)
{
	W.Number(FromBits(0x4029000000000001ull));
}

// chaine.vide
static void BuildDigestVector17(AnastasisDigest::FStateWriter& W)
{
	W.String(UTF8_TO_TCHAR(""));
}

// chaine.ascii
static void BuildDigestVector18(AnastasisDigest::FStateWriter& W)
{
	W.String(UTF8_TO_TCHAR("res:wood"));
}

// chaine.accents
static void BuildDigestVector19(AnastasisDigest::FStateWriter& W)
{
	W.String(UTF8_TO_TCHAR("Theodoros Kalligas \xe2\x80\x94 fondateur"));
}

// chaine.grec
static void BuildDigestVector20(AnastasisDigest::FStateWriter& W)
{
	W.String(UTF8_TO_TCHAR("\xe1\xbc\x80\xce\xbd\xce\xac\xcf\x83\xcf\x84\xce\xb1\xcf\x83\xce\xb9\xcf\x82"));
}

// chaine.hors-bmp
static void BuildDigestVector21(AnastasisDigest::FStateWriter& W)
{
	W.String(UTF8_TO_TCHAR("\xf0\x9f\x8f\x9b\xef\xb8\x8f"));
}

// tableau.vide
static void BuildDigestVector22(AnastasisDigest::FStateWriter& W)
{
	W.BeginArray(0);
	W.EndArray();
}

// tableau.nombres
static void BuildDigestVector23(AnastasisDigest::FStateWriter& W)
{
	W.BeginArray(3);
		W.Number(FromBits(0x3ff0000000000000ull));
		W.Number(FromBits(0x4000000000000000ull));
		W.Number(FromBits(0x4008000000000000ull));
	W.EndArray();
}

// tableau.mixte
static void BuildDigestVector24(AnastasisDigest::FStateWriter& W)
{
	W.BeginArray(4);
		W.Null();
		W.Bool(true);
		W.Number(FromBits(0xbff8000000000000ull));
		W.String(UTF8_TO_TCHAR("x"));
	W.EndArray();
}

// objet.vide
static void BuildDigestVector25(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
	W.EndObject();
}

// objet.une-cle
static void BuildDigestVector26(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("a"));
		W.Number(FromBits(0x3ff0000000000000ull));
	W.EndObject();
}

// objet.tri-a
static void BuildDigestVector27(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("b"));
		W.Number(FromBits(0x4000000000000000ull));
		W.Key(UTF8_TO_TCHAR("a"));
		W.Number(FromBits(0x3ff0000000000000ull));
	W.EndObject();
}

// objet.tri-b
static void BuildDigestVector28(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("a"));
		W.Number(FromBits(0x3ff0000000000000ull));
		W.Key(UTF8_TO_TCHAR("b"));
		W.Number(FromBits(0x4000000000000000ull));
	W.EndObject();
}

// objet.cles-majuscules
static void BuildDigestVector29(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("Z"));
		W.Number(FromBits(0x3ff0000000000000ull));
		W.Key(UTF8_TO_TCHAR("a"));
		W.Number(FromBits(0x4000000000000000ull));
		W.Key(UTF8_TO_TCHAR("A"));
		W.Number(FromBits(0x4008000000000000ull));
	W.EndObject();
}

// compose.acteur
static void BuildDigestVector30(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("id"));
		W.Number(FromBits(0x401c000000000000ull));
		W.Key(UTF8_TO_TCHAR("x"));
		W.Number(FromBits(0x4044aa0000000000ull));
		W.Key(UTF8_TO_TCHAR("y"));
		W.Number(FromBits(0xc029800000000000ull));
		W.Key(UTF8_TO_TCHAR("job"));
		W.String(UTF8_TO_TCHAR("job:woodcutter"));
		W.Key(UTF8_TO_TCHAR("home"));
		W.Null();
		W.Key(UTF8_TO_TCHAR("hunger"));
		W.Number(FromBits(0x3fe3c6ef372fe950ull));
		W.Key(UTF8_TO_TCHAR("carrying"));
		W.BeginArray(2);
			W.BeginObject();
				W.Key(UTF8_TO_TCHAR("res"));
				W.String(UTF8_TO_TCHAR("res:wood"));
				W.Key(UTF8_TO_TCHAR("n"));
				W.Number(FromBits(0x4008000000000000ull));
			W.EndObject();
			W.BeginObject();
				W.Key(UTF8_TO_TCHAR("res"));
				W.String(UTF8_TO_TCHAR("res:stone"));
				W.Key(UTF8_TO_TCHAR("n"));
				W.Number(FromBits(0x3ff0000000000000ull));
			W.EndObject();
		W.EndArray();
		W.Key(UTF8_TO_TCHAR("alive"));
		W.Bool(true);
	W.EndObject();
}

// compose.imbrication
static void BuildDigestVector31(AnastasisDigest::FStateWriter& W)
{
	W.BeginObject();
		W.Key(UTF8_TO_TCHAR("z"));
		W.BeginArray(2);
			W.BeginObject();
				W.Key(UTF8_TO_TCHAR("k"));
				W.Number(FromBits(0x3ff0000000000000ull));
			W.EndObject();
			W.BeginArray(1);
				W.String(UTF8_TO_TCHAR("profond"));
			W.EndArray();
		W.EndArray();
		W.Key(UTF8_TO_TCHAR("a"));
		W.BeginObject();
			W.Key(UTF8_TO_TCHAR("b"));
			W.BeginObject();
				W.Key(UTF8_TO_TCHAR("c"));
				W.Number(FromBits(0x4022000000000000ull));
			W.EndObject();
		W.EndObject();
	W.EndObject();
}

struct FDigestVector { const TCHAR* Name; void (*Build)(AnastasisDigest::FStateWriter&); uint64 Expected; };
static const FDigestVector DigestVectors[] = {
	{ TEXT("scalaire.null"), &BuildDigestVector0, 0xaf63bc4c8601b62cull },
	{ TEXT("scalaire.vrai"), &BuildDigestVector1, 0xaf63be4c8601b992ull },
	{ TEXT("scalaire.faux"), &BuildDigestVector2, 0xaf63bf4c8601bb45ull },
	{ TEXT("nombre.zero"), &BuildDigestVector3, 0x985b2cc3d2245173ull },
	{ TEXT("nombre.zero-negatif"), &BuildDigestVector4, 0x985bacc3d2252af3ull },
	{ TEXT("nombre.un"), &BuildDigestVector5, 0x9a4469c3d3c3dd0aull },
	{ TEXT("nombre.demi"), &BuildDigestVector6, 0x9a7ae9c3d3f245faull },
	{ TEXT("nombre.tiers"), &BuildDigestVector7, 0x4fcbe8b30d864069ull },
	{ TEXT("nombre.dt-du-harnais"), &BuildDigestVector8, 0xcadc00476ee205a5ull },
	{ TEXT("nombre.negatif"), &BuildDigestVector9, 0x3830092c6977b8d6ull },
	{ TEXT("nombre.tres-grand"), &BuildDigestVector10, 0x9785e8b0c769c757ull },
	{ TEXT("nombre.tres-petit"), &BuildDigestVector11, 0x796065bac7350752ull },
	{ TEXT("nombre.infini"), &BuildDigestVector12, 0x9a4429c3d3c3704aull },
	{ TEXT("nombre.infini-negatif"), &BuildDigestVector13, 0x9a43a9c3d3c296caull },
	{ TEXT("nombre.nan"), &BuildDigestVector14, 0x9a2929c3d3aca892ull },
	{ TEXT("nombre.ulp-a"), &BuildDigestVector15, 0x97d016c3d1ae5ca2ull },
	{ TEXT("nombre.ulp-b"), &BuildDigestVector16, 0x79ebfbbac7abd5a3ull },
	{ TEXT("chaine.vide"), &BuildDigestVector17, 0xa551e004b29ed560ull },
	{ TEXT("chaine.ascii"), &BuildDigestVector18, 0x233a77d0336ad4e9ull },
	{ TEXT("chaine.accents"), &BuildDigestVector19, 0x50647caf552d7c2bull },
	{ TEXT("chaine.grec"), &BuildDigestVector20, 0xa60f7dcd23bed5e7ull },
	{ TEXT("chaine.hors-bmp"), &BuildDigestVector21, 0xb14d3585599bbd88ull },
	{ TEXT("tableau.vide"), &BuildDigestVector22, 0xcb5e89842a8d1489ull },
	{ TEXT("tableau.nombres"), &BuildDigestVector23, 0x1a08b1b84914036bull },
	{ TEXT("tableau.mixte"), &BuildDigestVector24, 0xf20a77c5e22285a4ull },
	{ TEXT("objet.vide"), &BuildDigestVector25, 0xbeafa659ad3daa26ull },
	{ TEXT("objet.une-cle"), &BuildDigestVector26, 0x6bb32263cd3f75fbull },
	{ TEXT("objet.tri-a"), &BuildDigestVector27, 0x1deaa2704475bb5eull },
	{ TEXT("objet.tri-b"), &BuildDigestVector28, 0x1deaa2704475bb5eull },
	{ TEXT("objet.cles-majuscules"), &BuildDigestVector29, 0x45ca9d87b953c988ull },
	{ TEXT("compose.acteur"), &BuildDigestVector30, 0xa637dc417804d3ccull },
	{ TEXT("compose.imbrication"), &BuildDigestVector31, 0xbdd9e9d27e60688dull },
};

// Invariants verifies par le test, en plus des empreintes elles-memes.
static const uint64 DigestTriDesordre = 0x1deaa2704475bb5eull; // == objet.tri-b
static const uint64 DigestTriOrdre = 0x1deaa2704475bb5eull;
static const uint64 DigestUlpA = 0x97d016c3d1ae5ca2ull;
static const uint64 DigestUlpB = 0x79ebfbbac7abd5a3ull; // != DigestUlpA
