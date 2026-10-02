# Recherche : réalisme dans Unreal

Sources brutes sur le réalisme du rendu (PDF, notes, rapports). **Un agent ne les applique pas
directement** : elles passent d'abord par le skill `anastasis-realisme`
(`.claude/skills/anastasis-realisme/`), qui découpe chaque source en affirmations, les vérifie contre le
moteur et contre le projet, et range le résultat dans `registre.md` et dans les fiches par domaine.

Procédure d'ajout : `SKILL.md` du skill, section 5 (« Ingérer une nouvelle recherche »).

| ID | Fichier | Nature | Reçu | Ingéré |
|---|---|---|---|---|
| RU-001 | `RU-001_resume-executif.pdf` | « Résumé exécutif », rapport ChatGPT Deep Research, 10 pages : pipeline AAA de monde ouvert sous UE 5.8 | 2026-10-01 | oui, 54 affirmations (`RU-001-01` à `RU-001-54`) |
| RU-002 | `RU-002_ue58-sources-primaires.md` | Recherche sur sources primaires : notes de version Epic 5.7 / 5.8 et moteur 5.8.2 installé (plugins, CVars lus dans le code) | 2026-10-01 | oui, 20 affirmations (`RU-002-01` à `RU-002-20`) |
