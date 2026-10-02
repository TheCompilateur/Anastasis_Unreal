// CATALOGUE DU PLANIFICATEUR — mission planner-module-001.
//
//   node tools/migration/gen-planner-catalog.mjs -ref <clone anastasis-ref-p3>
//
// Les donnees que le planificateur collectif lit dans la reference, recopiees telles quelles :
// BUILDINGS (cout, logement, securite, stockage, postes), DEPOT_PROFILES, SITE_STOCK_PROFILE,
// CONSUME_ORDER (pas exporte : relu du source), MARKET.cap, MARKET_RESOURCES, CHARTER_THEMES,
// JOBS[*].traitBias.gather, WORKSHOP, HOUSE_PHASES (relu du source), BUILD_COST_GROWTH,
// PLANK_BUILD, CONSTRUCTION_PIECE_TOTAL, et les constantes COLLECTIVE, COLONY_REPORT, CHARTER,
// FOREST_STOCK, COLONIZATION. Ne pas editer le .inl a la main : relancer.

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");
const argv = process.argv.slice(2);
const argOf = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : d; };
const REF = argOf("-ref", null);
if (!REF) {
  console.error("-ref <clone de anastasis-ref-p3> requis");
  process.exit(2);
}
const REF_DIR = REF.split(String.fromCharCode(92)).join("/");
const OUT = argOf("-out", join(RACINE, "Source", "AnastasisSim", "Private", "Village", "AnastasisPlannerCatalog.inl"));
const url = (rel) => pathToFileURL(join(REF_DIR, rel)).href;

const content = await import(url("src/sim/content.js"));
const ledger = await import(url("src/sim/transport/stockLedger.js"));
const charter = await import(url("src/sim/founderCharter.js"));
const cp = await import(url("src/sim/collectivePriorities.js"));
const report = await import(url("src/sim/colonyStockReport.js"));
const forest = await import(url("src/sim/forestSustain.js"));
const doctrine = await import(url("src/sim/colonizationDoctrine.js"));
const pieces = await import(url("src/sim/constructionPieces.js"));

// Constantes non exportees : relues du source, verifiees par une evaluation stricte.
function sourceConst(rel, name) {
  const src = readFileSync(join(REF_DIR, rel), "utf8");
  const start = src.indexOf(`const ${name} =`);
  if (start < 0) throw new Error(`${name} introuvable dans ${rel}`);
  let i = src.indexOf("=", start) + 1;
  let depth = 0;
  let j = i;
  for (; j < src.length; j += 1) {
    const c = src[j];
    if (c === "(" || c === "[" || c === "{") depth += 1;
    else if (c === ")" || c === "]" || c === "}") depth -= 1;
    else if (c === ";" && depth === 0) break;
  }
  const expr = src.slice(i, j).trim().replace(/Object\.freeze/g, "");
  // eslint-disable-next-line no-new-func
  return Function(`"use strict"; return (${expr});`)();
}
const CONSUME_ORDER = sourceConst("src/sim/transport/stockLedger.js", "CONSUME_ORDER");
const HOUSE_PHASES = sourceConst("src/sim/simulation.js", "HOUSE_PHASES");

const L = [];
const num = (x) => {
  if (!Number.isFinite(x)) throw new Error(`nombre attendu : ${x}`);
  const s = String(x);
  return /[.eE]/.test(s) ? s : `${s}.0`;
};
const str = (s) => `TEXT("${s}")`;

L.push("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
L.push("// Source: tools/migration/gen-planner-catalog.mjs (reference anastasis-ref-p3)");
L.push("// Inclus par AnastasisPlanner.cpp, dans namespace AnastasisPlanner::Catalog.");
L.push("");
L.push("// clang-format off");
L.push("");

// --- Ressources --------------------------------------------------------------------------
L.push(`static const TCHAR* const MarketResources[] = { ${content.MARKET_RESOURCES.map(str).join(", ")} };`);
L.push(`static const FNamedValue MarketCap[] = { ${Object.entries(content.MARKET.cap).map(([k, v]) => `{ ${str(k)}, ${num(v)} }`).join(", ")} };`);
L.push("");

// --- Batiments ---------------------------------------------------------------------------
const costs = [];
const storage = [];
const specs = [];
for (const [type, data] of Object.entries(content.BUILDINGS)) {
  const c0 = costs.length;
  for (const [res, amt] of Object.entries(data.cost || {})) costs.push(`{ ${str(res)}, ${num(amt)} }`);
  const s0 = storage.length;
  for (const [res, amt] of Object.entries(data.storage || {})) storage.push(`{ ${str(res)}, ${num(amt)} }`);
  specs.push(`{ ${str(type)}, ${num(data.housing || 0)}, ${num(data.security || 0)}, ${(data.jobs || []).length}, `
    + `${c0}, ${costs.length - c0}, ${s0}, ${storage.length - s0}, ${data.housing ? "true" : "false"} }`);
}
L.push(`static const FNamedValue BuildingCosts[] = {\n\t${costs.join(",\n\t")}\n};`);
L.push(`static const FNamedValue BuildingStorage[] = {\n\t${storage.join(",\n\t")}\n};`);
L.push("/** Type, housing, security, postes (jobs.length), cout [debut, n], stockage [debut, n], logement (data.housing vrai). */");
L.push(`static const FBuildingSpec Buildings[] = {\n\t${specs.join(",\n\t")}\n};`);
L.push("");

// --- Profils de depot ----------------------------------------------------------------------
const allow = [];
const profiles = [];
for (const [type, prof] of Object.entries(ledger.DEPOT_PROFILES)) {
  const a0 = allow.length;
  for (const res of prof.allow) allow.push(`{ ${str(res)}, ${num(prof.cap[res] ?? content.MARKET.cap[res] ?? 0)} }`);
  profiles.push(`{ ${str(type)}, ${a0}, ${allow.length - a0} }`);
}
const site0 = allow.length;
for (const res of ledger.SITE_STOCK_PROFILE.allow) allow.push(`{ ${str(res)}, ${num(ledger.SITE_STOCK_PROFILE.cap[res] ?? content.MARKET.cap[res] ?? 0)} }`);
L.push("/** Ressource autorisee et plafond (`profile.cap[res] ?? MARKET.cap[res] ?? 0`). */");
L.push(`static const FNamedValue ProfileAllow[] = {\n\t${allow.join(",\n\t")}\n};`);
L.push(`static const FDepotProfile DepotProfiles[] = {\n\t${profiles.join(",\n\t")}\n};`);
L.push(`static const FDepotProfile SiteProfile = { TEXT(""), ${site0}, ${allow.length - site0} };`);
L.push("");
L.push(`static const FConsumeOrder ConsumeOrder[] = {\n\t${Object.entries(CONSUME_ORDER).map(([res, types]) => `{ ${str(res)}, TEXT("${types.join(",")}") }`).join(",\n\t")}\n};`);
L.push("");

// --- Charte ----------------------------------------------------------------------------------
const cvals = [];
const themes = [];
for (const [id, t] of Object.entries(charter.CHARTER_THEMES)) {
  const g0 = cvals.length;
  for (const [k, v] of Object.entries(t.goalBias || {})) cvals.push(`{ ${str(k)}, ${num(v)} }`);
  const b0 = cvals.length;
  for (const [k, v] of Object.entries(t.buildingBoosts || {})) cvals.push(`{ ${str(k)}, ${num(v)} }`);
  const j0 = cvals.length;
  for (const [k, v] of Object.entries(t.jobBoosts || {})) cvals.push(`{ ${str(k)}, ${num(v)} }`);
  themes.push(`{ ${str(id)}, ${g0}, ${b0 - g0}, ${b0}, ${j0 - b0}, ${j0}, ${cvals.length - j0} }`);
}
L.push(`static const FNamedValue CharterValues[] = {\n\t${cvals.join(",\n\t")}\n};`);
L.push("/** Theme : goalBias [debut, n], buildingBoosts [debut, n], jobBoosts [debut, n] dans CharterValues. */");
L.push(`static const FCharterTheme CharterThemes[] = {\n\t${themes.join(",\n\t")}\n};`);
L.push("");

// --- Metiers -------------------------------------------------------------------------------
L.push("/** `jobForId(id).traitBias.gather` (`Number(...) || 0`). */");
L.push(`static const FNamedValue JobTraitBiasGather[] = { ${Object.entries(content.JOBS).map(([id, j]) => `{ ${str(id)}, ${num(Number(j.traitBias?.gather) || 0)} }`).join(", ")} };`);
L.push("");

// --- Constantes -------------------------------------------------------------------------------
const consts = [];
const pushConsts = (prefix, obj) => {
  for (const [k, v] of Object.entries(obj)) {
    if (typeof v === "number") consts.push(`inline constexpr double ${prefix}${k[0].toUpperCase()}${k.slice(1)} = ${num(v)};`);
  }
};
pushConsts("Collective", cp.COLLECTIVE);
pushConsts("Report", report.COLONY_REPORT);
pushConsts("Charter", charter.CHARTER);
const forestNums = {};
for (const k of Object.keys(forest.FOREST_STOCK)) {
  const v = forest.FOREST_STOCK[k];
  if (typeof v === "number") forestNums[k] = v;
}
pushConsts("Forest", forestNums);
pushConsts("Colonization", doctrine.COLONIZATION);
pushConsts("Workshop", content.WORKSHOP);
pushConsts("CostGrowth", content.BUILD_COST_GROWTH);
pushConsts("PlankBuild", content.PLANK_BUILD);
consts.push(`inline constexpr int32 ConstructionPieceTotal = ${pieces.CONSTRUCTION_PIECE_TOTAL};`);
L.push(...consts);
L.push(`static const double HousePhaseCapacity[] = { ${HOUSE_PHASES.map((p) => num(p.capacity)).join(", ")} };`);
L.push(`static const FNamedValue CollectiveReserveCap[] = { ${Object.entries(cp.COLLECTIVE.reserveCap).map(([k, v]) => `{ ${str(k)}, ${num(v)} }`).join(", ")} };`);
L.push(`static const TCHAR* const CollectiveSecondaryTypes[] = { ${cp.COLLECTIVE.secondaryTypes.map(str).join(", ")} };`);
L.push(`static const FNamedValue ReportPresumed[] = { ${Object.entries(report.COLONY_REPORT.presumed).map(([k, v]) => `{ ${str(k)}, ${num(v)} }`).join(", ")} };`);
L.push(`static const TCHAR* const ReportResources[] = { ${report.COLONY_REPORT.resources.map(str).join(", ")} };`);
const floors = [];
for (const [focus, m] of Object.entries(cp.DAILY_FOCUS_FLOOR)) {
  for (const [goal, pts] of Object.entries(m)) floors.push(`{ ${str(focus)}, ${str(goal)}, ${num(pts)} }`);
}
L.push(`static const FFocusFloor DailyFocusFloor[] = {\n\t${floors.join(",\n\t")}\n};`);
L.push("");

mkdirSync(dirname(OUT), { recursive: true });
writeFileSync(OUT, L.join("\n") + "\n", "utf8");
console.error(`Ecrit: ${OUT} — ${specs.length} batiments, ${profiles.length} profils, ${themes.length} themes, ${consts.length} constantes`);
