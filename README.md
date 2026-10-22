# Holyverb — Studio Wiga

Pédale de reverb VST3 au format pédale simple, inspirée d'une reverb compacte à 8 modes.
Le signal sec passe intact : seule la reverb est ajoutée.

Interface d'après le visuel Holyverb : potards et switches rendus en haute définition
avec une lumière fixe en haut à gauche (seuls l'index, le point blanc et le moletage tournent),
LED bleue dans sa bague chromée, animée par la reverb ; embases jack Studio Wiga IN (droite) / OUT (gauche).

Fenêtre : 391 × 493 à l'ouverture (taille minimale, identique à Echoflex), agrandissable jusqu'à 150 %.

## Commandes

| Commande | Rôle |
|---|---|
| **DECAY** | Durée de la queue de reverb (plage propre à chaque mode) |
| **TONE** | Couleur de la reverb, du sombre au brillant (agit aussi sur l'amortissement interne) |
| **LEVEL** | Quantité de reverb : 0 = muet, midi ≈ −6 dB, max ≈ +6 dB |
| **MODE** | sens horaire : Church, Plate, Hall, Room, Shimmer, Lofi, Mod, Spring |
| **PRE-DELAY** | Mini-switch SHORT / LONG (temps adapté à chaque mode) |
| **TAILS** | haut (TAILS) : la queue continue de sonner quand on coupe la pédale ; bas (CUT) : coupure nette |
| **Footswitch** | ON / BYPASS (la LED s'allume et respire avec la reverb) |
| **Expression** | Paramètre automatisable (pas de bouton) à assigner à une pédale d'expression MIDI |

La ligne d'état sous HOLYVERB affiche le mode en cours (« MODE HALL ») ; quand un réglage bouge
(souris, automation, MIDI), elle affiche sa valeur 2,5 s puis revient au mode.
Double-clic sur un potard : retour à la valeur par défaut. Clic droit : menu (taille de fenêtre, presets, mode…).

## Presets

14 presets usine, visibles dans le **menu déroulant de presets du DAW** (Reaper : liste en haut de la fenêtre FX)
et dans le menu clic droit : Concert Hall, Studio Room, Tight Ambience, Vintage Plate, Dark Plate, Surf Spring,
Drippy Spring, Cathedral, Endless Church, Chorus Hall, Lofi Tape Room, Heaven Shimmer, Ambient Wash, Huge Long Hall.

9 emplacements de presets utilisateur (clic droit → Save / Load user preset), enregistrés dans
`%APPDATA%\Studio Wiga\Holyverb\`.

## Logo

Le logo Studio Wiga officiel (blanc, horizontal) est lu depuis `Assets/logo_studio_wiga.png`.
Les embases jack IN/OUT sont intégrées au fond (`Assets/background.jpg`).
Le dossier `StudioWiga_Kit/` contient le logo et l'embase détourée, prêts pour les prochaines pédales.

## Les modes

| Mode | Decay (min → max) | Pré-délai S / L | Expression (0 → 100 %) |
|---|---|---|---|
| Room | 0,25 → 2,2 s | 2 / 40 ms | monte le LEVEL jusqu'au max |
| Hall | 0,8 → 6 s | 10 / 80 ms | monte le LEVEL jusqu'au max |
| Spring | 0,6 → 4,5 s | 0 / 50 ms | monte le LEVEL jusqu'au max |
| Plate | 0,6 → 7 s | 5 / 70 ms | monte le LEVEL jusqu'au max |
| Church | 2,5 → 12 s | 25 / 120 ms | allonge le DECAY jusqu'à 20 s |
| Lofi | 0,6 → 6 s | 8 / 60 ms | allonge le DECAY jusqu'à 20 s |
| Mod | 0,8 → 8 s | 12 / 80 ms | modulation plus profonde + queue plus longue |
| Shimmer | 1,5 → 15 s | 15 / 90 ms | plus d'octave + queue jusqu'à 20 s |

Astuce (comme sur la pédale d'origine) : LEVEL à zéro en Room/Hall/Spring/Plate,
la pédale d'expression devient un « volume de reverb » au pied.

## Moteur audio

- **Room / Hall / Church / Lofi / Mod / Shimmer** : réseau de 8 lignes à retard réinjectées (FDN, matrice de Hadamard), diffusion d'entrée, amortissement dépendant de la fréquence, modulation lente.
- **Room** ajoute des réflexions précoces.
- **Plate** : topologie de Dattorro.
- **Spring** : deux ressorts simulés (chaîne d'allpass dispersifs = « boing » caractéristique).
- **Lofi** : bande passante réduite, sous-échantillonnage ~11 kHz et réduction de résolution.
- **Shimmer** : transposition à l'octave supérieure dans la boucle de réinjection.
- Entrée mono ou stéréo, sortie stéréo. Pas de latence. Changement de mode sans clic (fondu).

Le moteur est dans `Source/HolyverbDSP.h` (sans dépendance JUCE) et un banc de test
hors DAW est fourni dans `tests/test_dsp.cpp` (mesure des T60, niveaux, stabilité, rendus WAV).

## Obtenir le VST3 via GitHub

Pousser le dossier sur GitHub : le workflow `.github/workflows/build.yml` compile automatiquement
le VST3 sous Windows. Onglet **Actions** → dernier build → **Artifacts** → `Holyverb-VST3-Windows`.
Dézipper et copier `Holyverb.vst3` dans `C:\Program Files\Common Files\VST3\`.

## Compilation locale (Windows 11)

Prérequis : Visual Studio 2022 (C++), CMake ≥ 3.22, Git.

```bat
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

JUCE est téléchargé automatiquement. Pour utiliser une copie locale :
`cmake -B build -DJUCE_DIR=C:/chemin/vers/JUCE`

Le plugin se trouve dans `build/Holyverb_artefacts/Release/VST3/Holyverb.vst3`
→ à copier dans `C:\Program Files\Common Files\VST3\`, puis relancer le scan dans Reaper.

## Banc de test (optionnel)

```bat
g++ -O2 -std=c++17 tests/test_dsp.cpp -o test_dsp && test_dsp
```
