// FONCTIONS D'UN MODULE JS — ou commence et ou finit chaque definition.
//
// L'inventaire doit dire, pour un module dont une partie seulement est portee,
// combien de lignes de code restent. Il faut pour cela savoir ou s'etend chaque
// fonction portee. Un parseur JS complet serait une dependance pour une question
// simple; ce scanner masque d'abord ce qui n'est pas du code (chaines, gabarits,
// commentaires, expressions regulieres), puis apparie accolades et parentheses
// sur le texte masque.
//
// Formes reconnues, a toute profondeur:
//   function nom(...) {          export function / async function / function*
//   const|let|var nom = (...) => ...    const nom = function ... / async ...
//   nom(...) {                   methode de classe ou d'objet litteral
//
// Limite connue: la detection d'une expression reguliere repose sur le caractere
// significatif qui precede le `/` (un operateur, une ouverture, un mot-cle). Une
// division apres `)` est bien lue comme une division; un `/` regex apres `)`
// (rare: `if (x) /re/.test(y)`) serait mal lu. Le rapport le dit.

const KEYWORDS_BEFORE_REGEX = new Set([
  "return", "typeof", "instanceof", "in", "of", "new", "delete", "void", "throw",
  "case", "do", "else", "yield", "await",
]);
const NOT_METHOD = new Set([
  "if", "for", "while", "switch", "catch", "function", "return", "with", "else",
  "do", "try", "finally", "typeof", "new", "await", "yield",
]);

/** Remplace chaines, gabarits, commentaires et regex par des espaces (sauts de ligne gardes). */
export function maskCode(text) {
  const out = text.split("");
  const n = text.length;
  const blank = (i) => { if (out[i] !== "\n" && out[i] !== "\r") out[i] = " "; };
  // Pile de contextes: "code" ou "tpl" (texte de gabarit). Un `${` empile du
  // code avec sa profondeur d'accolades.
  const stack = [{ kind: "code", depth: 0 }];
  let lastSig = ""; // dernier caractere significatif du code
  let lastWord = "";
  let i = 0;
  while (i < n) {
    const top = stack[stack.length - 1];
    const c = text[i];
    if (top.kind === "tpl") {
      if (c === "\\") { blank(i); blank(i + 1); i += 2; continue; }
      if (c === "`") { stack.pop(); i += 1; lastSig = "`"; lastWord = ""; continue; }
      if (c === "$" && text[i + 1] === "{") { stack.push({ kind: "code", depth: 0 }); i += 2; lastSig = "{"; lastWord = ""; continue; }
      blank(i); i += 1; continue;
    }
    // code
    if (c === "/" && text[i + 1] === "/") {
      while (i < n && text[i] !== "\n") { blank(i); i += 1; }
      continue;
    }
    if (c === "/" && text[i + 1] === "*") {
      blank(i); blank(i + 1); i += 2;
      while (i < n && !(text[i] === "*" && text[i + 1] === "/")) { blank(i); i += 1; }
      blank(i); blank(i + 1); i += 2;
      continue;
    }
    if (c === "'" || c === '"') {
      i += 1;
      while (i < n && text[i] !== c && text[i] !== "\n") {
        if (text[i] === "\\") { blank(i); i += 1; }
        blank(i); i += 1;
      }
      i += 1; lastSig = c; lastWord = "";
      continue;
    }
    if (c === "`") { stack.push({ kind: "tpl" }); i += 1; continue; }
    if (c === "/") {
      const regexCtx = lastSig === "" || "(,=:[!&|?{};+-*%<>~^".includes(lastSig) || KEYWORDS_BEFORE_REGEX.has(lastWord);
      if (regexCtx) {
        i += 1;
        let inClass = false;
        while (i < n && text[i] !== "\n") {
          const d = text[i];
          if (d === "\\") { blank(i); blank(i + 1); i += 2; continue; }
          if (d === "[") inClass = true;
          else if (d === "]") inClass = false;
          else if (d === "/" && !inClass) break;
          blank(i); i += 1;
        }
        i += 1;
        while (i < n && /[a-z]/i.test(text[i])) i += 1; // drapeaux
        lastSig = "/"; lastWord = "";
        continue;
      }
    }
    if (c === "{") top.depth += 1;
    if (c === "}") {
      if (top.depth === 0 && stack.length > 1) { stack.pop(); i += 1; lastSig = "}"; lastWord = ""; continue; }
      top.depth -= 1;
    }
    if (/[A-Za-z0-9_$]/.test(c)) {
      let j = i;
      while (j < n && /[A-Za-z0-9_$]/.test(text[j])) j += 1;
      lastWord = text.slice(i, j);
      lastSig = text[j - 1];
      // un mot-cle laisse le contexte "regex possible"
      if (KEYWORDS_BEFORE_REGEX.has(lastWord)) lastSig = "(";
      i = j;
      continue;
    }
    if (!/\s/.test(c)) { lastSig = c; lastWord = ""; }
    i += 1;
  }
  return out.join("");
}

function matchClose(masked, openIdx) {
  const open = masked[openIdx];
  const close = open === "{" ? "}" : open === "(" ? ")" : "]";
  let depth = 0;
  for (let i = openIdx; i < masked.length; i += 1) {
    const c = masked[i];
    if (c === open) depth += 1;
    else if (c === close) { depth -= 1; if (depth === 0) return i; }
  }
  return -1;
}

function skipWs(masked, i) {
  while (i < masked.length && /\s/.test(masked[i])) i += 1;
  return i;
}

/** Fin d'une expression (corps d'arrow sans accolades): `;` ou `,`/`)` au niveau 0. */
function expressionEnd(masked, i) {
  let depth = 0;
  for (; i < masked.length; i += 1) {
    const c = masked[i];
    if ("([{".includes(c)) depth += 1;
    else if (")]}".includes(c)) { if (depth === 0) return i - 1; depth -= 1; }
    else if ((c === ";" || c === ",") && depth === 0) return i;
    else if (c === "\n" && depth === 0) {
      // fin de ligne au niveau 0: l'expression continue si la ligne suivante
      // commence par un operateur ou un point.
      const k = skipWs(masked, i + 1);
      if (!/[.?:+\-*/&|]/.test(masked[k] ?? "")) return i;
    }
  }
  return masked.length - 1;
}

/**
 * Definitions trouvees dans `text`: { name, kind, start, end } en offsets, et
 * startLine / endLine (0-based, inclusives).
 */
export function findDefinitions(text) {
  const masked = maskCode(text);
  const defs = [];
  const lineOf = (() => {
    const starts = [0];
    for (let i = 0; i < text.length; i += 1) if (text[i] === "\n") starts.push(i + 1);
    return (off) => {
      let lo = 0, hi = starts.length - 1;
      while (lo < hi) { const mid = (lo + hi + 1) >> 1; if (starts[mid] <= off) lo = mid; else hi = mid - 1; }
      return lo;
    };
  })();
  const push = (name, kind, start, end) => {
    if (end < start) return;
    defs.push({ name, kind, start, end, startLine: lineOf(start), endLine: lineOf(end) });
  };

  const bodyFromParams = (parenIdx) => {
    const close = matchClose(masked, parenIdx);
    if (close < 0) return -1;
    let k = skipWs(masked, close + 1);
    if (masked[k] === "=" && masked[k + 1] === ">") {
      k = skipWs(masked, k + 2);
      return masked[k] === "{" ? matchClose(masked, k) : expressionEnd(masked, k);
    }
    return masked[k] === "{" ? matchClose(masked, k) : -1;
  };

  let m;
  const reFn = /\bfunction\s*\*?\s*([A-Za-z_$][\w$]*)\s*\(/g;
  while ((m = reFn.exec(masked))) {
    const end = bodyFromParams(m.index + m[0].length - 1);
    if (end > 0) push(m[1], "function", m.index, end);
  }
  const reConst = /\b(?:const|let|var)\s+([A-Za-z_$][\w$]*)\s*=\s*(async\s+)?(function\b\s*\*?\s*[\w$]*\s*\(|\(|[A-Za-z_$][\w$]*\s*=>)/g;
  while ((m = reConst.exec(masked))) {
    const tail = m[3];
    let end = -1;
    if (tail.endsWith("(")) end = bodyFromParams(m.index + m[0].length - 1);
    else {
      let k = skipWs(masked, masked.indexOf("=>", m.index + m[0].length - 3) + 2);
      end = masked[k] === "{" ? matchClose(masked, k) : expressionEnd(masked, k);
    }
    if (end > 0) push(m[1], "const", m.index, end);
  }
  // Methode: en debut de ligne, `nom(...) {`, eventuellement precede de
  // static / async / get / set.
  const reMethod = /^[ \t]*(?:static\s+|async\s+|get\s+|set\s+)*\*?([A-Za-z_$][\w$]*)\s*\(/gm;
  while ((m = reMethod.exec(masked))) {
    if (NOT_METHOD.has(m[1])) continue;
    const parenIdx = m.index + m[0].length - 1;
    const close = matchClose(masked, parenIdx);
    if (close < 0) continue;
    const k = skipWs(masked, close + 1);
    if (masked[k] !== "{") continue; // un appel, pas une definition
    const end = matchClose(masked, k);
    if (end > 0) push(m[1], "method", m.index + (m[0].length - m[0].trimStart().length), end);
  }
  defs.sort((a, b) => a.start - b.start);
  return { defs, masked };
}
