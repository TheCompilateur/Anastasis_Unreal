#include "Core/AnastasisJson.h"

#include "Core/AnastasisStateDigest.h"

namespace AnastasisJson
{
	FValue FValue::MakeBool(bool bValue)
	{
		FValue V;
		V.Kind = EKind::Bool;
		V.bBool = bValue;
		return V;
	}

	FValue FValue::MakeNumber(double Value)
	{
		FValue V;
		V.Kind = EKind::Number;
		V.Number = Value;
		return V;
	}

	FValue FValue::MakeString(const FString& Value)
	{
		FValue V;
		V.Kind = EKind::String;
		V.String = Value;
		return V;
	}

	FValue FValue::MakeArray()
	{
		FValue V;
		V.Kind = EKind::Array;
		return V;
	}

	FValue FValue::MakeObject()
	{
		FValue V;
		V.Kind = EKind::Object;
		return V;
	}

	const FValue* FValue::Find(const FString& Key) const
	{
		if (Kind != EKind::Object)
		{
			return nullptr;
		}
		// Derniere occurrence: c'est celle que garde `JSON.parse`.
		for (int32 Index = Keys.Num() - 1; Index >= 0; --Index)
		{
			if (Keys[Index].Equals(Key, ESearchCase::CaseSensitive))
			{
				return &Items[Index];
			}
		}
		return nullptr;
	}

	FValue* FValue::Find(const FString& Key)
	{
		return const_cast<FValue*>(static_cast<const FValue*>(this)->Find(Key));
	}

	void FValue::Set(const FString& Key, const FValue& Value)
	{
		check(Kind == EKind::Object);
		if (FValue* Existing = Find(Key))
		{
			*Existing = Value;
			return;
		}
		Keys.Add(Key);
		Items.Add(Value);
	}

	namespace
	{
		/** Analyseur a descente recursive sur le texte UTF-16. */
		struct FParser
		{
			const FString& Text;
			int32 Pos = 0;
			FString Error;

			explicit FParser(const FString& InText) : Text(InText) {}

			bool Fail(const TCHAR* What)
			{
				if (Error.IsEmpty())
				{
					Error = FString::Printf(TEXT("JSON invalide a la position %d : %s"), Pos, What);
				}
				return false;
			}

			TCHAR Peek() const { return Pos < Text.Len() ? Text[Pos] : TEXT('\0'); }

			void SkipSpace()
			{
				while (Pos < Text.Len())
				{
					const TCHAR C = Text[Pos];
					if (C != TEXT(' ') && C != TEXT('\t') && C != TEXT('\n') && C != TEXT('\r'))
					{
						break;
					}
					++Pos;
				}
			}

			bool Literal(const TCHAR* Word)
			{
				const int32 Len = FCString::Strlen(Word);
				if (Pos + Len > Text.Len() || FCString::Strncmp(*Text + Pos, Word, Len) != 0)
				{
					return Fail(TEXT("litteral inattendu"));
				}
				Pos += Len;
				return true;
			}

			static int32 HexDigit(TCHAR C)
			{
				if (C >= TEXT('0') && C <= TEXT('9')) return C - TEXT('0');
				if (C >= TEXT('a') && C <= TEXT('f')) return 10 + C - TEXT('a');
				if (C >= TEXT('A') && C <= TEXT('F')) return 10 + C - TEXT('A');
				return -1;
			}

			bool ParseString(FString& Out)
			{
				if (Peek() != TEXT('"'))
				{
					return Fail(TEXT("chaine attendue"));
				}
				++Pos;
				Out.Reset();
				while (Pos < Text.Len())
				{
					const TCHAR C = Text[Pos++];
					if (C == TEXT('"'))
					{
						return true;
					}
					if (C < 0x20)
					{
						return Fail(TEXT("caractere de controle dans une chaine"));
					}
					if (C != TEXT('\\'))
					{
						Out.AppendChar(C);
						continue;
					}
					if (Pos >= Text.Len())
					{
						break;
					}
					const TCHAR E = Text[Pos++];
					switch (E)
					{
					case TEXT('"'): Out.AppendChar(TEXT('"')); break;
					case TEXT('\\'): Out.AppendChar(TEXT('\\')); break;
					case TEXT('/'): Out.AppendChar(TEXT('/')); break;
					case TEXT('b'): Out.AppendChar(TEXT('\b')); break;
					case TEXT('f'): Out.AppendChar(TEXT('\f')); break;
					case TEXT('n'): Out.AppendChar(TEXT('\n')); break;
					case TEXT('r'): Out.AppendChar(TEXT('\r')); break;
					case TEXT('t'): Out.AppendChar(TEXT('\t')); break;
					case TEXT('u'):
					{
						// Une unite de code UTF-16 par echappement: une paire de
						// substitution arrive en deux `\u`, et FString (UTF-16)
						// la garde telle quelle — comme une chaine JS.
						if (Pos + 4 > Text.Len())
						{
							return Fail(TEXT("echappement \\u tronque"));
						}
						int32 Unit = 0;
						for (int32 K = 0; K < 4; ++K)
						{
							const int32 D = HexDigit(Text[Pos + K]);
							if (D < 0)
							{
								return Fail(TEXT("echappement \\u invalide"));
							}
							Unit = Unit * 16 + D;
						}
						Pos += 4;
						Out.AppendChar(static_cast<TCHAR>(Unit));
						break;
					}
					default:
						return Fail(TEXT("echappement inconnu"));
					}
				}
				return Fail(TEXT("chaine non terminee"));
			}

			bool ParseNumber(double& Out)
			{
				// Grammaire JSON stricte, puis UNE conversion du jeton entier par
				// la bibliotheque C: c'est elle qui arrondit correctement.
				const int32 Start = Pos;
				if (Peek() == TEXT('-')) ++Pos;
				if (Peek() == TEXT('0'))
				{
					++Pos;
				}
				else if (Peek() >= TEXT('1') && Peek() <= TEXT('9'))
				{
					while (FChar::IsDigit(Peek())) ++Pos;
				}
				else
				{
					return Fail(TEXT("nombre invalide"));
				}
				if (Peek() == TEXT('.'))
				{
					++Pos;
					if (!FChar::IsDigit(Peek())) return Fail(TEXT("fraction vide"));
					while (FChar::IsDigit(Peek())) ++Pos;
				}
				if (Peek() == TEXT('e') || Peek() == TEXT('E'))
				{
					++Pos;
					if (Peek() == TEXT('+') || Peek() == TEXT('-')) ++Pos;
					if (!FChar::IsDigit(Peek())) return Fail(TEXT("exposant vide"));
					while (FChar::IsDigit(Peek())) ++Pos;
				}
				const FString Token = Text.Mid(Start, Pos - Start);
				Out = FCString::Atod(*Token);
				return true;
			}

			bool ParseValue(FValue& Out, int32 Depth)
			{
				if (Depth > 512)
				{
					return Fail(TEXT("imbrication trop profonde"));
				}
				SkipSpace();
				const TCHAR C = Peek();
				if (C == TEXT('{'))
				{
					++Pos;
					Out = FValue::MakeObject();
					SkipSpace();
					if (Peek() == TEXT('}'))
					{
						++Pos;
						return true;
					}
					for (;;)
					{
						SkipSpace();
						FString Key;
						if (!ParseString(Key)) return false;
						SkipSpace();
						if (Peek() != TEXT(':')) return Fail(TEXT("':' attendu"));
						++Pos;
						FValue Child;
						if (!ParseValue(Child, Depth + 1)) return false;
						// Cle repetee: `JSON.parse` garde la place de la premiere et la
						// valeur de la derniere. `serialize` n'en produit pas; le lire juste
						// coute une ligne.
						if (FValue* Existing = Out.Find(Key))
						{
							*Existing = MoveTemp(Child);
						}
						else
						{
							Out.Keys.Add(MoveTemp(Key));
							Out.Items.Add(MoveTemp(Child));
						}
						SkipSpace();
						if (Peek() == TEXT(','))
						{
							++Pos;
							continue;
						}
						if (Peek() == TEXT('}'))
						{
							++Pos;
							return true;
						}
						return Fail(TEXT("',' ou '}' attendu"));
					}
				}
				if (C == TEXT('['))
				{
					++Pos;
					Out = FValue::MakeArray();
					SkipSpace();
					if (Peek() == TEXT(']'))
					{
						++Pos;
						return true;
					}
					for (;;)
					{
						FValue Child;
						if (!ParseValue(Child, Depth + 1)) return false;
						Out.Items.Add(MoveTemp(Child));
						SkipSpace();
						if (Peek() == TEXT(','))
						{
							++Pos;
							continue;
						}
						if (Peek() == TEXT(']'))
						{
							++Pos;
							return true;
						}
						return Fail(TEXT("',' ou ']' attendu"));
					}
				}
				if (C == TEXT('"'))
				{
					FString S;
					if (!ParseString(S)) return false;
					Out = FValue::MakeString(S);
					return true;
				}
				if (C == TEXT('t'))
				{
					if (!Literal(TEXT("true"))) return false;
					Out = FValue::MakeBool(true);
					return true;
				}
				if (C == TEXT('f'))
				{
					if (!Literal(TEXT("false"))) return false;
					Out = FValue::MakeBool(false);
					return true;
				}
				if (C == TEXT('n'))
				{
					if (!Literal(TEXT("null"))) return false;
					Out = FValue();
					return true;
				}
				double N = 0.0;
				if (!ParseNumber(N)) return false;
				Out = FValue::MakeNumber(N);
				return true;
			}
		};
	}

	bool Parse(const FString& Text, FValue& Out, FString& OutError)
	{
		// Un separateur decimal autre que le point (culture de la bibliotheque C)
		// lirait chaque fraction de travers sans erreur. Mieux vaut refuser.
		if (FCString::Atod(TEXT("0.5")) != 0.5)
		{
			OutError = TEXT("la bibliotheque C ne lit pas le point decimal (culture numerique) : lecture refusee");
			return false;
		}
		FParser Parser(Text);
		FValue Root;
		if (!Parser.ParseValue(Root, 0))
		{
			OutError = Parser.Error;
			return false;
		}
		Parser.SkipSpace();
		if (Parser.Pos != Text.Len())
		{
			Parser.Fail(TEXT("texte apres la valeur"));
			OutError = Parser.Error;
			return false;
		}
		Out = MoveTemp(Root);
		return true;
	}

	namespace
	{
		void AppendQuoted(FString& Out, const FString& S)
		{
			Out.AppendChar(TEXT('"'));
			for (const TCHAR C : S)
			{
				switch (C)
				{
				case TEXT('"'): Out.Append(TEXT("\\\"")); break;
				case TEXT('\\'): Out.Append(TEXT("\\\\")); break;
				case TEXT('\n'): Out.Append(TEXT("\\n")); break;
				case TEXT('\r'): Out.Append(TEXT("\\r")); break;
				case TEXT('\t'): Out.Append(TEXT("\\t")); break;
				case TEXT('\b'): Out.Append(TEXT("\\b")); break;
				case TEXT('\f'): Out.Append(TEXT("\\f")); break;
				default:
					if (C < 0x20)
					{
						Out.Appendf(TEXT("\\u%04x"), static_cast<uint32>(C));
					}
					else
					{
						Out.AppendChar(C);
					}
				}
			}
			Out.AppendChar(TEXT('"'));
		}

		void AppendValue(FString& Out, const FValue& V)
		{
			switch (V.Kind)
			{
			case EKind::Null: Out.Append(TEXT("null")); return;
			case EKind::Bool: Out.Append(V.bBool ? TEXT("true") : TEXT("false")); return;
			case EKind::Number:
				if (!FMath::IsFinite(V.Number))
				{
					Out.Append(TEXT("null"));
				}
				else
				{
					Out.Appendf(TEXT("%.17g"), V.Number);
				}
				return;
			case EKind::String: AppendQuoted(Out, V.String); return;
			case EKind::Array:
				Out.AppendChar(TEXT('['));
				for (int32 I = 0; I < V.Items.Num(); ++I)
				{
					if (I > 0) Out.AppendChar(TEXT(','));
					AppendValue(Out, V.Items[I]);
				}
				Out.AppendChar(TEXT(']'));
				return;
			case EKind::Object:
				Out.AppendChar(TEXT('{'));
				for (int32 I = 0; I < V.Keys.Num(); ++I)
				{
					if (I > 0) Out.AppendChar(TEXT(','));
					AppendQuoted(Out, V.Keys[I]);
					Out.AppendChar(TEXT(':'));
					AppendValue(Out, V.Items[I]);
				}
				Out.AppendChar(TEXT('}'));
				return;
			}
		}
	}

	FString Stringify(const FValue& Value)
	{
		FString Out;
		AppendValue(Out, Value);
		return Out;
	}

	void Write(AnastasisDigest::FStateWriter& Writer, const FValue& Value)
	{
		switch (Value.Kind)
		{
		case EKind::Null:
			Writer.Null();
			return;
		case EKind::Bool:
			Writer.Bool(Value.bBool);
			return;
		case EKind::Number:
			Writer.Number(Value.Number);
			return;
		case EKind::String:
			Writer.String(Value.String);
			return;
		case EKind::Array:
			Writer.BeginArray(Value.Items.Num());
			for (const FValue& Item : Value.Items)
			{
				Write(Writer, Item);
			}
			Writer.EndArray();
			return;
		case EKind::Object:
			Writer.BeginObject();
			for (int32 Index = 0; Index < Value.Keys.Num(); ++Index)
			{
				Writer.Key(Value.Keys[Index]);
				Write(Writer, Value.Items[Index]);
			}
			Writer.EndObject();
			return;
		}
	}

	uint64 DigestOf(const FValue& Value)
	{
		AnastasisDigest::FStateWriter Writer;
		Write(Writer, Value);
		return Writer.Digest();
	}
}
