# VOIX_CONSEIL_001 — « Une voix au conseil » : le joueur entre dans les décisions de Valmire

## Décision d'Alexandre (2026-10-09)

Après « Les arrivants », Alexandre choisit son deuxième rêve, **simple habitant**, et ajoute : **« les PNJ doivent être
critiques envers moi »**. C'est aussi le mandat de la parole du joueur (jusqu'ici `PLAYER` minimal, AGENTS.md).

| Question | Réponse |
|---|---|
| Au départ, comment le village te voit | **un étranger à éprouver** : arrivé après les fondateurs, on se méfie ; ta confiance se gagne acte par acte |
| Jusqu'où va la critique | **en face** (quand tu demandes ou réponds, on te rappelle tes actes) et **dans ton dos** (tes actes deviennent des histoires qui courent, que ton carnet entend parfois) |

> **Scène** : le joueur est un habitant de Valmire, pas son seigneur. Il entre dans les mêmes décisions que les autres, et le village le juge sur ses actes.
> **Je dois voir** ma voix au conseil quand un groupe arrive ; des habitants qui viennent me demander de l'aide, et à qui je dis oui ou non ; moi qui demande de l'aide pour mon toit ; et des habitants critiques envers moi, qui me rappellent mes refus, parlent de moi dans mon dos, et me répondent selon ce que j'ai fait.
> **Le joueur** décide avec les mêmes règles que les autres ; son carnet garde ce qu'il a fait et ce qu'on dit de lui.
> **Hors sujet** : le rendu, une interface graphique (des commandes pour commencer), le combat.
> **Fini quand** : la chronique de 60 jours raconte un vote du joueur au conseil, une aide qu'il a refusée et qu'on lui reproche plus tard, une rumeur à son sujet entendue dans son dos, et une demande d'aide du joueur refusée à cause de ce qu'il a fait.

## Ce que le joueur peut faire (écart n°54)

| Commande | Effet |
|---|---|
| `Anastasis.Player.Vote oui` / `non` | sa voix au prochain conseil, sur le plus ancien groupe qui attend à la porte |
| `Anastasis.Player.Help oui` / `non` | sa réponse à la plus ancienne demande d'aide qu'on lui a faite ; sans réponse jusqu'au soir suivant, c'est un refus (« il n'a même pas répondu ») |
| `Anastasis.Player.Build` | il décide de bâtir : il devient chef de son foyer, et sa parcelle est tracée près de lui comme celle d'une famille |
| `Anastasis.Player.Ask <prénom>` | il va demander de l'aide à un habitant pour sa maison ; la réponse tombe tout de suite, avec sa raison |

Ses besoins restent les siens : quand le corps parle (épuisé, assoiffé, affamé), seul le remède passe, et c'est à lui de le
choisir (écart n°21) ; l'écran le dit (« le corps passe devant : il faut dormir »).

## Comment le village le juge

- **L'étranger à éprouver** : tant qu'il n'a pas posé une pièce sur la maison achevée d'une autre famille, sa voix au
  conseil compte pour moitié, et qui il sollicite hésite (`nouveau`, −10). La même règle vaut pour les groupes d'arrivants.
- **En face** : quand il demande, la réponse se pèse comme celle de tout habitant (Bible §29), avec trois raisons qui
  parlent de lui : `refus_rendu` (« tu m'as dit non »), `on_dit` (−12 par refus qu'on lui prête, entendu d'un autre : « On dit
  que tu as laissé Konstantinos sans bras ») et `porte_fermee` (« Tu as fermé la porte à la maison d'Hovhannes. La mienne
  aussi est fermée. »).
- **Dans son dos** : sa voix au conseil est retenue par chaque chef présent (`votedNo` / `votedYes`), ses refus par ceux
  qu'il a laissés (`refusedHelp`) ; ces souvenirs courent de bouche en bouche et se déforment ; quand il est à portée de
  voix, son carnet les entend (« CE QU'ON DIT DE MOI »).

## Le carnet

Deux pages de plus, en tête : **ce que j'ai fait** (ma voix à chaque conseil, mes réponses, mes demandes et ce qu'on m'a
répondu) et **ce qu'on dit de moi** (les histoires sur moi entendues dans mon dos).

## Comment le lire

| Pour | Comment |
|---|---|
| la preuve rejouable | `tools\unreal\editor-batch.ps1 -Proofs voix-pie` → `voix-pie-60-jours.txt`, `carnet-voix-60-jours.txt`, `Saved/VoiceEvidence/pie/voice.json` |
| jouer | PIE, `Anastasis.Player.Arrive`, puis les commandes ci-dessus ; `Anastasis.Chronicle.Write`, `Anastasis.Carnet.Write` |

## Ce qui n'est pas fait

- Pas d'interface : des commandes. Le joueur ne parle pas encore de sa voix (ses réponses n'ont pas de phrase).
- On ne le chasse pas du village (Alexandre n'a pas retenu cette option).
- Une même faute ne pèse pas plus lourd venant de lui (option non retenue) : seule la demi-voix et `nouveau` le distinguent, et ils tombent quand il a aidé.
