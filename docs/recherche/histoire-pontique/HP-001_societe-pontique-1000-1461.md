# HP-001 — La société grecque du Pont avant et après 1204, pour ANÁSTASIS

Recherche du 2026-10-07, faite pour Alexandre. Premier rapport d'une série : il doit pouvoir être relu
par n'importe quel agent futur sans la conversation qui l'a produit.

## 0. À quoi sert ce document

ANÁSTASIS commence vers 1204 : un village de réfugiés grecs s'installe dans une **zone abandonnée du
Pont, hors de toute faction, exposée aux Turcs** (décision d'Alexandre). Ce rapport rassemble ce que
les sources disent de la vie de ces gens : habitat, économie, pouvoir local, frontière, captifs,
relations avec les Turcs, les Latins et les autres Grecs. Il sert de **réserve de faits sourcés** pour
le design et pour les données de scénario (le C++ porte la physique ; les faits historiques vont dans
les données, avec leur source).

Il ne tranche pas de question de design. Il dit ce qui est attesté, ce qui est plausible, ce qui est
inconnu.

## 1. Statut des affirmations

| Marque | Sens |
|---|---|
| **[V]** | vérifié : lu dans une source listée en §2, page donnée ; les citations clés ont été relues dans le texte |
| **[I]** | inférence de l'auteur de ce rapport à partir de faits [V] |
| **[A]** | abstraction de jeu proposée, sans prétention historique |
| **[?]** | inconnu, ou affirmation qui circule et qu'on n'a pas pu sourcer |

Les sources primaires (Panarétos, actes de Vazelon, Clavijo, Lazaropoulos, Nicétas Choniatès, Michel le
Syrien…) ne sont connues ici **qu'à travers** les études qui les citent. Personne n'a encore lu les
actes de Vazelon eux-mêmes.

## 2. Sources lues

Toutes librement téléchargeables sur archive.org (texte OCR `*_djvu.txt`). Notes de lecture détaillées,
page par page, dans `notes/`.

| Id | Référence | Lu | Notes |
|---|---|---|---|
| BRY75 | A. Bryer, « Greeks and Türkmens: the Pontic Exception », *Dumbarton Oaks Papers* 29 (1975), p. 113-148. https://archive.org/details/DOP29_05_Bryer | en entier | ce document, §4-6 |
| BW85 | A. Bryer & D. Winfield, *The Byzantine Monuments and Topography of the Pontos*, DOS 20 (1985). https://archive.org/details/bryer-winfield-1985-byz-pontus-01-02 | géographie, Matzouka (p. 251-266), Chaldia (p. 299-305), Cheriana, Trikomia, Limnia, Philabonitès | `notes/HP-001b_notes-bryer-winfield-1985.md` |
| VRY71 | S. Vryonis, *The Decline of Medieval Hellenism in Asia Minor* (1971). https://archive.org/details/vryonis-1971-dmh | Pont, conquête, nomades, Église, survie du grec | `notes/HP-001a_notes-vryonis-1971.md` |
| BRY83 | A. Bryer, « The Crypto-Christians of the Pontos… », *Deltio KMS* 4 (1983). https://archive.org/details/bryer-1983-crypto-christians | en entier | `notes/HP-001c_notes-bryer-1983-2002.md` |
| BRY02 | A. Bryer et al., *The Post-Byzantine Monuments of the Pontos* (2002). https://archive.org/details/bryer-2002-post-byzantine-monuments-pontos | introduction, essai de 1991, mots-clés | `notes/HP-001c_notes-bryer-1983-2002.md` |

Lu aussi, pour la littérature de cour : Papadopoulos-Kérameus, *Analekta* I (1891), p. 421-437
(oraison de Loukitès pour Alexis II, poèmes de Sgouropoulos), et K. Kubina, *DOP* 76 (2022). Peu utile
pour la société rurale ; voir §9.

## 3. Le cadre en une page

- **Avant 1204, le Pont est un thème, la Chaldia** (depuis les années 820), dont les **sept vallées**
  autour de Trébizonde sont des *banda*, circonscriptions militaires de vallée [V BRY75]. Elles gardent
  cette structure jusqu'en 1461.
- **Le XIe-XIIe siècle est dur, au nord comme ailleurs.** Paipert saccagée (1054), Coloneia razziée
  (1057), Néocésarée saccagée (1068) ; sous Michel VII (1071-1078) la côte jusqu'à Trébizonde et
  l'arrière-pays de l'Iris et de l'Halys sont « partially abandoned » ; Trébizonde même est prise un
  temps (1071-1075) [V VRY71 p. 87, 160-162]. En 1139, un émir danichmendide emmène la population des
  terres pontiques d'un certain Cassianus et la vend comme esclaves ; ce Cassianus avait livré ses forts
  aux Danichmendides contre un poste [V VRY71 p. 161 n., 230].
- **Les Gabrades** gouvernent la Chaldia de fait (années 1070-1140), contre les Turcs **et** contre
  Constantinople. Théodore Gabras meurt martyr des Turcs vers 1098, mais pour Constantinople c'est un
  rebelle. Un siècle plus tard, des Gabras sont émirs et dignitaires à Konya [V BRY75 ; VRY71 p. 230-231, 360].
- **En 1204**, Alexis et David Comnène prennent Trébizonde. L'empire des Grands Comnènes est une
  affaire **locale** : séparatisme pontique plus que restauration de Rome [V BRY75]. Sinope est perdue
  en 1214 ; les Seldjoukides assiègent Trébizonde en 1222-1223 et sont repoussés par les vallées
  [V BW85 p. 252-257].
- **Après 1204, ailleurs** : le partage latin (*Partitio Romaniae*) donne l'Asie Mineure à l'empereur
  latin ; des aristocrates grecs se taillent des principautés, dont Manuel Maurozomès, **beau-père du
  sultan**, installé dans la vallée du Méandre « with the aid of Turkish troops » [V VRY71 p. 130-131].
  Le sultan Kaykhusraw, chassé de Konya, s'était réfugié à Constantinople [V VRY71 p. 130 n. 255].
- **Vers 1205-1255, Konya et Nicée vivent en paix relative** : « the borders remained comparatively
  stable and Anatolia once more began to experience the blessings of peace » [V VRY71 p. 131-132].
- **La poussée turkmène vient plus tard** : après Köse Dağ (1243) et surtout après 1277, quand le
  pouvoir seldjoukide puis mongol se défait. Les Çepni et les futurs Akkoyunlu pressent le Pont, surtout
  au XIVe siècle [V BRY75 ; BW85].

**[I] Conséquence pour 1204.** Un village de réfugiés en 1204 vit d'abord sous la menace **seldjoukide
et des marches** (raids, sièges, captifs), pas encore sous la pression systématique des Turkmènes
transhumants, qui est surtout attestée après 1277-1340. Les deux auteurs de notes BW85 le signalent :
les razzias pastorales turkmènes sont un léger anachronisme en 1204, plausibles dès les années 1240.

## 4. Le village pontique

### Habitat
- **Dispersé** : « houses scattered widely over a valley with only a church or mosque to mark its
  centre » ; des hameaux et des fermes rattachés à un *chorion* et à ses *staseis*, sur le « modèle
  caucasien » [V BW85 p. 1, 251-252]. Pas de village groupé, pas d'enceinte. Les termes *chorion* et
  *stasis* sont souples ; un hameau peut être promu *chorion* [V BW85 p. 162, 257-263].
- **Maisons** : tours fortifiées ou chalets de bois, étable en dessous ; on chauffe au bois ; pas de
  charrette, des bêtes de bât (« ou des femmes ») [V BRY75 ; BW85 p. 1-2, 18]. Clavijo (1404) : des
  hameaux appelés « tours » [V BRY75].
- **Trois étages** : village permanent à 300-1 000 m, village d'été vers 1 500 m, pâturages au-dessus
  de 2 000 m, à quelques heures de marche ; « one could conveniently have acted as a shepherd one day
  and as a farmer the next » [V BW85 p. 260 ; BRY75 p. 140].
- **Densité** : environ 500 familles sur 24 km² du haut Prytanis (« perhaps ») [V BW85 p. 260] ; hameaux
  modernes de 3 à 20 maisons [V BW85 p. 260-264].

### Terre, famille, pouvoir local
- **Paysans libres** : la terre est un patrimoine familial (*gonikeion*) librement hérité, vendu,
  légué ; peu de grands domaines laïcs ; le mot *paroikos* n'apparaît pas dans les actes de Vazelon
  [V BW85 p. 252 ; BRY75]. Une tenure garde le nom d'une famille de 1270 à 1922 [V BW85 p. 252].
- Bryer parle de « peacemaking elders » et d'un *allelengyon* (impôt d'entraide) encore levé au
  XVe siècle [V BRY75]. BW85 ne trouve l'*allelengyon* qu'une fois (Kampana, XIIIe s.) et ne documente
  pas les anciens [V BW85 p. 257]. **Les anciens restent à sourcer dans les actes eux-mêmes.**
- **Officiers de vallée** : *doux* et *kephalē* de bandon, un juge (*kritēs*) de Matzouka attesté en 1367,
  un gardien de château [V BW85 p. 160, 253, 262].
- **Seigneurs des marches** : en Chaldia, des dynastes à demi indépendants (Kabazitai, Tzanichitai),
  titrés à la cour. Leon Kabazites à Clavijo (1404) : il vit « de ce que lui donnent les passants et du
  pillage des terres de ses voisins », avec 30 archers à cheval [V BRY75 ; BW85 p. 301-303].
- **Cheriana**, le seul district rural que l'empereur ne protège pas : « a sort of Christian
  no-man's-land ». Hypothèse des auteurs : des communautés autonomes qui se réfugient dans des tours
  quand les Çepni descendent, menées par leur évêque [V BW85 p. 165, 172-173 — hypothèse explicite].

**[I]** Cheriana est le meilleur modèle historique du village d'ANÁSTASIS : zone sans protecteur
régulier, communautés qui se gouvernent seules, protectorat lointain qu'on appelle quand le danger est
réel.

### Économie
- Noisettes (grande exportation), noix, cerises, poires ; vigne non palissée grimpant aux arbres, vin
  noir exporté ; céréales en terrasses irriguées ; chanvre, miel, foin ; fromage et présure en altitude ;
  moulins à eau nombreux [V BW85 p. 3-6, 251, 260-263 ; BRY75].
- Sel, poisson salé, céréales importés de Crimée ; le sel manque [V BW85 p. 5-6].
- Pas de maïs, de pomme de terre ni de tabac avant l'époque moderne [V BW85 p. 6, 252].
- **Pas de mines d'argent prouvées sous les Comnènes** ; Gümüşhane est ottomane (XVIIe-XIXe s.)
  [V BW85 p. 3 ; BRY02].
- Prix (actes de Vazelon via Bryer) : agneau 4-6 aspres, chaudron d'occasion 8, selle 11-15, cheval de
  selle 150-400, esclave circassienne de 11 ans 600, de 18 ans 900 ; rachat d'une captive aux Turkmènes
  fixé à 850 [V BRY75 p. 138-139]. **Ce qui manque, ce sont les hommes, pas la terre** [V BRY75].

### Calendrier
| Saison | Ce qui se passe | Réf. |
|---|---|---|
| mai | montée aux pâturages ; pâturages « annually disputed in May » ; des bergers disparaissent | [V BW85 p. 251 ; BRY75 p. 139] |
| début d'été | raids sur les hameaux hauts (23 juillet 1361 ; bataille du 21 mai 1370) | [V BW85 p. 256-259] |
| septembre | récolte et séchage des noisettes ; passage des cailles et des *hamsi* | [V BRY75 ; BW85 p. 5] |
| automne | le crocus « vargit » annonce la descente des villages d'été | [V BW85 p. 5] |
| hiver | les Turkmènes descendent vers les basses terres et les deltas ; expéditions impériales contre leurs camps | [V BW85 p. 98-99, 141, 172 ; BRY75] |

## 5. La frontière : ce qui arrive au village

- **Le raid enlève des gens, il ne rase pas.** « Manpower, not land » ; « they raided to kidnap » ;
  disparitions de bergers en mai ; un village perd la moitié de sa population [V BRY75 p. 138-139].
  Les raiders visent surtout les enfants et le bétail [V VRY71 p. 175, 269].
- **Actes de Vazelon, cas datés** :
  - XIIIe s. : à Trigoliktos, « the entire Romanopoulos family was carried off by Türkmens » [V BW85 p. 257] ;
  - 1261 : Maria Tzarchalina lègue son bien au monastère : « j'ai cinq fils prisonniers ; s'ils reviennent,
    qu'on leur donne leur part ; sinon… » [V BRY75 p. 139] ;
  - 1302 : Anna Elaphinaba donne la moitié de son *gonikeion* car « during the raid of the Hagarenes my
    relatives were carried off in captivity » [V BW85 p. 257] ;
  - décembre 1344 : la nonne Anysia, sans bras pour la ferme familiale capturée, la donne à Vazelon
    [V BW85 p. 257].
- **Les terres des disparus passent au monastère** [V BRY75 p. 139 ; BW85 p. 257].
- **Réfugiés** : le surnom *Aichmalōtos* (captif évadé) ; un évêque de Satala réfugié à Vazelon en 1256 ;
  des réfugiés de Limnia réinstallés en bloc à Magera en 1432 [V BRY75 ; BW85 p. 161, 257].
- **Fuir en forêt, pas derrière les murs.** Dans les années 1430, assiégée, Trébizonde tombe à 50
  habitants : 4 000 ont « simplement fondu » dans les vallées [V BRY75]. Ailleurs en Anatolie au XIe s.,
  on fuit au contraire vers les villes murées (Trébizonde comprise) [V VRY71 p. 167-169].
- **Défense** : tours de guet monastiques qui alertent la vallée (Doubera pour Soumela, Gantopedin
  pour Vazelon), châteaux sur rocher, grottes-refuges qui peuvent devenir des pièges (Golacha, 1369)
  [V BW85 p. 254, 263, 283-286, 302].
- **Sous la menace nomade** : on abandonne d'abord les champs et pâtures loin du refuge ; on rentre le
  bétail la nuit ; on ne laboure ou ne coupe le bois qu'avec escorte [V VRY71 p. 270, 282].
- **Peste** : 1341, 1348, 1362, 1382 [V VRY71 p. 257 n. ; BW85 p. 161].

## 6. Les Turcs : une relation nuancée, sourcée

Alexandre a demandé de nuancer l'image du Turc conquérant. Les sources le permettent largement. Elles
distinguent sans cesse **le souverain** qui veut des sujets, **la tribu** qui veut des pâturages et des
captifs, et **les familles** qui passent d'un monde à l'autre.

### Ce qui est attesté
- **Un sultan veut des paysans, pas des ruines.** En 1197, Kaykhusraw installe 5 000 chrétiens de Carie
  près de Philomélion, leur donne terre et semence et **cinq ans d'exemption d'impôt** ; « not one of them
  considered escaping, and indeed many who heard of the tax exemption migrated to the sultan's domains
  because of the great disorder » des terres byzantines [V VRY71 p. 184 et 216-218, relu]. ʿAlāʾ al-Dīn donne
  semence et bétail pour faire revenir des paysans [V VRY71 p. 221 n.].
- **Des souverains protègent l'Église** : Malik Shah exempte églises, monastères et prêtres arméniens
  [V VRY71 p. 199, relu]. Vryonis distingue dirigeants parfois cléments et tribus [V VRY71 p. 171, 213-214].
- **Les élites se mêlent** : Gabras émirs à Konya ; Maurozomès beau-père du sultan ; chancellerie grecque
  du sultan, qui rédige en 1214 le traité avec Trébizonde ; émirs grecs (Constantin, Kir Farid)
  [V VRY71 p. 130-131, 231-233].
- **Trébizonde marie ses princesses aux émirs** : onze princesses épousent des musulmans, dont huit
  Turkmènes ; l'émir reçoit un titre et un « château de famille » sur la côte ; un descendant d'émir
  devient Grand Mésazon (premier ministre) de Trébizonde en 1461. L'empereur Alexis III est à la fois
  empereur et « melik » de petits émirats qu'il a aidé à créer [V BRY75].
- **La mémoire turque admire** : dans le *Dede Korkut*, la princesse de Trébizonde est une héroïne qu'un
  héros akkoyunlu conquiert par trois épreuves [V BRY75].
- **La vie se mêle au quotidien** : surnoms turcs chez des chrétiens de Matzouka (pas de prénoms) ;
  soldats trapézontins enturbannés, à l'étrier court ; *hatun* devient un titre grec ; la plus ancienne
  formule d'abjuration de l'islam pour entrer dans l'orthodoxie vient du Pont ; la femme d'un émir de
  Sivas vient se faire exorciser à Trébizonde ; des musulmans prient à Soumela ; les Turkmènes de la
  région sont alévis, à la religion souple [V BRY75 ; VRY71 p. 441 n., 460-461, 486 ; BRY83 p. 23].
- **Les mêmes Turkmènes tuent et enlèvent** : 1361, 1369, 1370 ; une famille entière enlevée ; les Çepni
  « mènent le djihad » contre Trébizonde [V BW85 ; VRY71 p. 193]. Les deux faces coexistent.

### Ce qui n'est pas attesté
- **[?]** « Les Turcs compatissaient parce que l'Empire avait été trahi par ses frères d'Occident en
  1204. » Aucune des sources lues ne dit cela. Ce qui est attesté est plus concret : un sultan qui a
  vécu en exil à Constantinople, des liens de mariage entre la cour seldjoukide et l'aristocratie grecque,
  un demi-siècle de paix relative avec Nicée, une politique d'installation de paysans grecs. **[I]** Une
  sympathie de certains Turcs pour les Grecs dépouillés par les Latins est **plausible** comme attitude
  individuelle dans un jeu ; elle ne doit pas être présentée comme un fait.

### Comment dire « Turc » dans le jeu [I]
Ne jamais modéliser « les Turcs » comme un bloc. Au moins quatre acteurs distincts, aux intérêts
différents :
1. **le sultanat de Konya** (État, impôt, colonisation agricole, diplomatie, chancellerie grecque) ;
2. **les émirs de marche** (Danichmendides avant 1178, puis émirs locaux) : razzias, forts, alliances ;
3. **les tribus turkmènes transhumantes** (Çepni, futurs Akkoyunlu) : pâturages d'été, raids, captifs,
   commerce de laine et de laitages une fois la paix faite [V VRY71 p. 270] ;
4. **les Grecs passés au service turc** (Gabras, Maurozomès, scribes, convertis qui parlent grec).

## 7. Latins et autres Grecs

- **Latins** : le partage de 1204 donne l'Asie Mineure à l'empereur latin, qui doit la conquérir par les
  armes [V VRY71 p. 130-131]. Sur la côte pontique, les **Génois** sont marchands et voisins : Kapanion,
  limite de sécurité au traité de Gênes de 1314 ; faubourg génois de Samsun ; cargaisons de noisettes ;
  indemnité payée à Gênes en noisettes (1418) [V BRY75 ; BW85 p. 257]. **[?]** Rien de lu sur la présence
  latine dans l'intérieur du Pont.
- **Trébizonde** : protège son Église ; c'est selon Vryonis la cause de la survie du grec au Pont
  [V VRY71 p. 291, 449-451]. Mais elle ne protège pas tout : Cheriana, haute Matzouka déclarée « enemy
  country » par une escorte en 1404 [V BW85 p. 263].
- **Nicée** : regarde vers Constantinople ; paix avec Konya ; met fin aux ambitions des Comnènes sur
  Constantinople dès 1214 [V BRY75 ; VRY71 p. 131-132].
- **Les monastères** sont des puissances à part entière : Soumela reçoit en 1364 l'immunité totale, 40
  dépendants nommés, l'exemption des colons nouveaux, et l'obligation de tenir une tour contre les
  Turkmènes [V BW85 p. 254]. « When your landlord is an abbot, you think twice about turning Turk »
  [V BRY02 p. xxviii].
- **Seigneurs des marches** : voir §4 ; ils taxent, protègent, pillent.

## 8. Protecteurs possibles pour un village sans faction [I]

Ce tableau est une lecture des faits ci-dessus ; ce n'est pas une règle historique.

| Protecteur | Modèle attesté | Ce que le village gagne | Ce qu'il paie |
|---|---|---|---|
| bandon impérial (Trébizonde) | *doux*, *kephalē*, juge de vallée | justice, milice de vallée, expéditions punitives | impôt, service |
| monastère | Soumela 1364, Vazelon | immunité, tour de guet, recueil des terres des disparus | la terre devient monastique |
| seigneur de marche | Kabazites 1404 | quelques archers | péage, pillage possible |
| émir ou tribu turkmène | Limnia, Chalybie ; Kaykhusraw 1197 | paix des pâturages, commerce, parfois exemption | tribut, mariages, glissement vers l'assimilation |
| personne | Cheriana | liberté | exposition totale ; on fuit en forêt |

Ces choix rejoignent presque un à un les factions identitaires de la référence JS
(`src/life/identityFactions.js` : `imperial`, `rooted`, `independents`, `assimilated`, `restorers`).
Il y manque la voie du **monastère**, historiquement centrale.

## 9. Pistes non retenues

- Littérature de cour (Loukitès, Sgouropoulos, Eugénikos, Bessarion) : intéressante pour l'idéologie
  dynastique, presque muette sur les villages. Inventaire et statut de traduction établis le
  2026-10-07 : l'oraison funèbre de Loukitès (Analekta I, p. 421-430) est peut-être traduite en allemand
  par Stefec (*Νέα Ῥώμη* 15, 2018) ; le poème 1 de Sgouropoulos est traduit en anglais (Kubina & Riehle
  2021) ; les poèmes 7-8 (Analekta I, p. 431-437) n'ont pas de traduction intégrale connue.
- Le crypto-christianisme pontique n'a **pas** d'origine byzantine attestée selon Bryer (premier
  témoignage vers 1825-1833, lié aux mines) ; thèse contestée par Photiadès (1985) [V BRY83 p. 20-33].
  Pour 1204-1461, penser **porosité** (mariages, sanctuaires partagés, conversion selon l'impôt) plutôt
  que culte caché [I].
- Fuite vers les hauteurs (Santa, Kromni, Stavri) : fin XVIIe siècle, sous les derebeys, pas byzantine
  [V BW85 p. 260, 304 ; BRY02].

## 10. Ce qui manque

- **Les actes de Vazelon eux-mêmes** (Ouspenski & Beneševič, Leningrad 1927) : non trouvés en ligne.
  Ce sont eux qui donneraient les anciens de village, les prix, les dots, les noms.
- **Panarétos** (traduit : DOML 52) et **Lazaropoulos** (Rosenqvist 1996) : non lus directement.
- **Bryer, « Rural society in Matzouka »** (1986) : cité partout, non lu.
- **[?]** La situation précise de 1204-1222 dans l'arrière-pays : quasi muette dans les sources lues.
- **[?]** La date de la prise de Trébizonde par les Turcs (1071-1075) : note de Vryonis non retrouvée dans l'OCR.

## 11. Pour reprendre ce travail

- Textes OCR : télécharger `https://archive.org/download/<id>/<fichier>_djvu.txt` (ids en §2). Ne pas
  les commiter (droits d'auteur) ; ils se retéléchargent.
- Le grec est illisible dans ces OCR ; les citations grecques doivent se vérifier sur le PDF.
- Les pages citées dans `notes/` viennent des en-têtes courants de l'OCR ; marge ±1 page là où le folio
  manque (signalé dans chaque fichier).
