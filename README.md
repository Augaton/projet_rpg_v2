# Augaton — un FTL-like en C++ / Raylib

Traversez 4 secteurs hostiles, gérez l'énergie de votre vaisseau, semez la flotte rebelle
et abattez le vaisseau amiral qui vous attend au bout du voyage.

Style rétro « brut » (police pixel, blocs à angles vifs, écran cathodique) sur une chaîne
de rendu moderne (shaders, bloom, post-traitement).

## Compiler et lancer

1. Installez Raylib 5.5 (voir https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux).
2. Compilez :

| Commande | Résultat |
|---|---|
| `make` | build optimisé et durci → `bin/ftl_game` |
| `make run` | compile puis lance le jeu |
| `make debug` | UBSan (mode trap) + conteneurs vérifiés → `bin/ftl_game_debug` |
| `make asan` | AddressSanitizer + UBSan (paquets `libasan` et `libubsan` requis) → `bin/ftl_game_asan` |
| `make clean` | nettoie |

Le binaire retrouve seul le dossier `asset/` : il peut être lancé depuis n'importe où.

## Commandes

| Action | Commande |
|---|---|
| Ajouter / retirer de l'énergie | clic gauche / clic droit / molette sur un système |
| Allumer une arme, puis la sélectionner | clic gauche sur l'arme, ou 1-4 |
| Viser avec l'arme sélectionnée | clic sur une salle ennemie |
| Toutes les armes sur une salle | clic sur une salle sans arme sélectionnée |
| Ciblage automatique | T ou bouton AUTO |
| Invisibilité / piratage (modules) | C / H puis clic sur une salle ennemie |
| Menu VAISSEAU (améliorations) | U ou bouton, à l'arrêt sur une balise |
| Éteindre une arme | clic droit sur l'arme |
| Réorganiser les armes (emplacements 1-4) | glisser-déposer une carte d'arme sur un autre emplacement |
| Allumer + viser en un geste | glisser une carte d'arme sur une salle ennemie |
| Système au maximum / coupé | Maj + clic gauche / droit sur le système |
| Allumer / éteindre toutes les armes | Maj + clic gauche / droit sur une arme (molette : une arme) |
| Pause automatique (début, fuite, coque critique, boss) | Paramètres → PAUSE AUTO EN COMBAT |
| Pause tactique (combat) / continuer | ESPACE |
| Vitesse du combat ×1 / ×2 / ×4 | TAB ou boutons en haut |
| Carte du secteur / saut FTL d'urgence | J (aussi depuis le résultat d'un événement) |
| Passer l'animation de saut | clic / ESPACE |
| Sauter vers une balise | clic, ou 1-9 sur la carte |
| Acheter par 5 | Maj + clic au magasin |
| Menus | flèches + Entrée, ou souris |
| Annuler, fermer, menu pause | ÉCHAP |
| Aide / plein écran / capture | F1 / F11 / F12 |

## Mécaniques

- **Réacteur & systèmes** : BOUCLIERS (2 barres = 1 couche), MOTEURS (esquive + charge FTL),
  ARMES, RÉPARATION (répare les systèmes en combat).
- **Armes** : elles se chargent quand elles sont alimentées et tirent sur leur cible.
  Les lasers sont bloqués par les boucliers, les missiles les traversent, l'ion les désactive.
- **Temps** : chronomètre de combat et vitesse ×1/×2/×4 (simulation à pas fixe, identique à toutes les vitesses).
- **FTL** : en présence d'un ennemi, le propulseur se recharge lentement (~31 s avec 2 barres de moteurs).
- **Fuite ennemie** : un ennemi en mauvaise posture annonce son intention de fuir 6 s à l'avance,
  puis charge son FTL. Détruire ses moteurs bloque la charge. Drones, flotte et amiral ne fuient jamais.
- **Réparation des modules en combat** : l'équipage répare toujours (~20 s par barre), le module RÉPARATION accélère.
  Aucun module ne reste hors service indéfiniment.
- **Modules** : BOUCLIERS, MOTEURS, ARMES, RÉPARATION, INVISIBILITÉ (+60 % d'esquive, armes ennemies gelées),
  PIRATAGE (coupe un système ennemi), ANTI-MISSILE (intercepte les missiles), CAPTEURS (révèle la carte).
  Les nouveaux modules s'achètent en magasin ; ils s'améliorent dans le menu VAISSEAU.
- **Carte** : les balises hostiles dominent, les zones calmes sont rares et peuvent cacher une embuscade ;
  revenir sur une balise visitée expose à une patrouille. Chaque saut coûte 1 carburant et fait avancer la flotte rebelle.
  L'info-bulle indique la distance jusqu'à la sortie et l'arrivée estimée de la flotte.
- **Ciblage** : les armes retiennent leur cible d'un combat à l'autre ; le ciblage auto envoie les lasers
  sur les boucliers puis les armes, les missiles sur les armes, et vise les moteurs d'un ennemi en fuite.
- **Lisibilité** : la charge et la cible des armes ennemies sont affichées, les salles visées sont signalées ;
  les capteurs révèlent le type des balises voisines.
- **Réparation en combat** : l'équipage répare toujours lentement, le système de réparation accélère.
- **Magasin** : ressources, armes et nouveaux modules ; l'énergie libre est assignée automatiquement.
- **Vaisseaux jouables** : AUGATON (polyvalent), VIPÈRE (intercepteur furtif, débloqué en atteignant le secteur 3),
  BASTION (croiseur lourd, débloqué après 20 destructions ou une victoire). Choix du vaisseau et de la difficulté
  (Facile / Normal / Difficile) en début de partie.
- **Augmentations** : 3 emplacements pour des bonus passifs (blindage réactif, préchargeur, recycleur de missiles…),
  trouvées dans les événements, sur les élites ou achetées en magasin.
- **Dangers de balise** (visibles sur la carte) : champ d'astéroïdes, éruptions solaires qui traversent les boucliers,
  tempête ionique qui ralentit les boucliers.
- **Ennemis d'élite** : un trait (blindé, surchargé, bouclier renforcé, évasif) et de meilleures récompenses.
- **Archives** : statistiques cumulées, dernières parties et conditions de déblocage.
- **Vaisseau amiral** en 3 phases : à 66 % il passe en « barrage » (missiles, tourelle renforcée),
  à 33 % en « surcharge » (armes et moteurs poussés, boucliers affaiblis). Les seuils sont marqués sur sa barre de coque.
- **Tutoriel** guidé au premier combat (réactivable dans les paramètres).
- **Sons d'interface synthétisés** : clics, arme prête, alarme de coque, impacts de bouclier, invisibilité, piratage.
- **Événements** à choix, **magasins**, boss final.

## Données utilisateur

Paramètres, progression (profil), sauvegarde automatique et captures sont stockés dans
`$XDG_DATA_HOME/augaton` (par défaut `~/.local/share/augaton`).

La partie est sauvegardée à chaque balise, à chaque arrivée et au début de chaque combat,
puis supprimée à la mort ou à la victoire. Au chargement, le fichier est validé :
taille maximale, somme de contrôle, bornes de chaque valeur, cohérence de la carte.
Tout fichier invalide est ignoré. La somme de contrôle détecte une corruption,
mais ce n'est pas une signature : un joueur déterminé peut toujours modifier sa partie.

## Architecture

```
src/
  main.cpp              point d'entrée
  game/Game             machine à états, menus, paramètres, reprise
  game/SaveGame         sauvegarde texte validée (v3, compatible v1/v2)
  game/Profile          progression : records, historique, déblocages
  combat/Combat         tirs, boucliers, esquive, IA, destruction, fuite
  ship/                 modèle du vaisseau, armes, augmentations, vaisseaux jouables et ennemis, rendu
  map/SectorMap         génération de la carte, flotte, export/import
  event/Events          base d'événements
  shop/Store            magasin (ressources, armes, modules)
  shop/ShipMenu         menu VAISSEAU (améliorations)
  hud/HUD               interface de jeu
  effect/PostFX         fond basse résolution tramé, bloom, écran cathodique
  effect/               nébuleuse, étoiles, particules
  core/                 UI (style + navigation clavier), paramètres, chemins, aléatoire
  ressources/shader/    nebula.fs, bloom.fs, crt.fs
```

## Crédits

- Sprites : pack *Kla'ed* (Foozle).
- Police : Source Code Pro (Adobe), sous licence SIL Open Font License (`asset/fonts/SourceCodePro-LICENSE.md`).
- Police pixel : police par défaut de raylib.
