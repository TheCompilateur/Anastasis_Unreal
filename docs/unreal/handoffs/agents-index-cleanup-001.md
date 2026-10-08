# HANDOFF: agents-index-cleanup-001

## MISSION

Nettoyer AGENTS.md abime par les fusions du 2026-10-07 : lignes d'index en double (player-pie,
gather-deliver-pie, lived-paths-capture, sky-clock-pie, create-ground-cover), ligne create-ground-cover
tombee dans la table « Dans la racine canonique, un agent peut », paragraphe `-TestFilter` qui coupait
la table du cycle de vie. Pour chaque doublon, la version la plus recente est gardee (player-pie avec
Goal build/drink, gather-deliver avec le sac plein, lived-paths avec anastasis.Anthropic.Draw,
create-ground-cover avec les fleurs sauvages). Regle ajoutee au skill anastasis-mission : AGENTS.md ne se
fusionne jamais par union.

## FILES_OWNED

- `AGENTS.md`
- `.claude/skills/anastasis-mission/SKILL.md`

## COMMIT

PENDING

## MEC

- `awk -F'|' '/^\| `/{print $2}' AGENTS.md | sort | uniq -d` : vide.

## PROOFS

PROOFS: (aucune)

## SCN

Sans objet.

## PLY

Sans objet.

## INTEGRATION_RISK

- Documentation seule.

## STOP

- Aucune ligne d'index ajoutee ni retiree hors doublons.
