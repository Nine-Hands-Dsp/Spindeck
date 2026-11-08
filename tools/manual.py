# SPINDECK (NINE HANDS | DSP) - notice PDF en francais.
# Usage : python3 tools/manual.py [fichier PDF de sortie]   (defaut : docs/Spindeck_Notice_FR_v<version>.pdf)
# Version lue dans CMakeLists.txt. Illustrations : tools/manual_assets/ (tools/manual_assets_gen.py a partir des
# captures reelles de tools/render). Moteur de mise en page : tools/manual_lib.py.
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import manual_lib as L

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = re.search(r"project\s*\(\s*\w+\s+VERSION\s+([0-9.]+)", open(os.path.join(ROOT, "CMakeLists.txt")).read()).group(1)
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "docs", "Spindeck_Notice_FR_v" + VERSION + ".pdf")

TEXTS = {"fr": dict(lang="FRANÇAIS", title="Notice d'utilisation",
                    sub=["Logiciel DJ 2 platines pour Reaper, piloté par l'ION Discover DJ",
                         "Instrument VST3 pour Windows et macOS \u00b7 AU pour macOS"],
                    ver="Notice d'utilisation \u00b7 version " + VERSION)}


def content(lang):
    b = []
    # 1 -------------------------------------------------------------------------------------------------
    b += [("h1", "1. Présentation"),
          ("p", "<b>Spindeck</b> est un logiciel DJ complet à deux platines, logé dans un plugin VST3 pour Reaper. Il se pilote avec la "
                "platine <b>ION Discover DJ</b>, reconnue d'office : jog wheels pour scratcher, crossfader, volumes, égaliseurs, "
                "boutons de lecture, de cue et de sync, et LED de la platine qui suivent l'état du logiciel. Tout se fait aussi à la souris."),
          ("p", "On y retrouve l'organisation d'un logiciel DJ classique : formes d'onde défilantes en haut, une platine de chaque "
                "côté avec son vinyle animé, la table de mixage au centre et la bibliothèque de morceaux en bas."),
          ("h2", "En bref"),
          ("table", [["2 platines", "Lecture, cue façon platine CD, sync du tempo et de la phase, marche arrière, pitch ± 4 / 8 / 16 / 50 %."],
                     ["Scratch", "Avec les jog wheels de la Discover DJ (mode SCRATCH) ou à la souris directement sur le vinyle."],
                     ["Analyse automatique", "BPM et grille de temps calculés au chargement ; le cue se place sur le premier son."],
                     ["Table de mixage", "EQ HIGH / LOW avec coupure, filtre passe-bas / passe-haut, volumes, VU-mètres, crossfader à 2 courbes, master avec limiteur de crête transparent."],
                     ["Bibliothèque", "Dossier de musique, recherche, colonne BPM, pré-écoute, glisser-déposer, encodeur BROWSE et boutons LOAD de la platine."],
                     ["Plein écran", "Bouton FULL : la fenêtre occupe tout l'écran où elle se trouve et s'adapte si on la déplace sur un autre écran."],
                     ["Platines réalistes", "Vinyle irisé qui tourne avec la lecture, bras qui suit la progression du morceau, points stroboscopiques, lampe rouge."],
                     ["4 skins", "Midnight, Neon Club, Amber Mono et Pro Red / Blue."],
                     ["MIDI Learn", "Chaque potard et fader peut aussi être appris sur n'importe quel contrôleur MIDI."]],
           [40, None], {"header": False}),
          ("pb",)]

    # 2 -------------------------------------------------------------------------------------------------
    b += [("h1", "2. Installation"),
          ("h2", "Windows"),
          ("bullets", ["Fermez Reaper.",
                       "Copiez le dossier <b>Spindeck.vst3</b> dans <b>C:\\Program Files\\Common Files\\VST3</b> (remplacez l'ancienne version s'il y en a une).",
                       "Ouvrez Reaper : Options > Preferences > Plug-ins > VST, puis <b>Re-scan</b>."]),
          ("h2", "macOS"),
          ("bullets", ["Copiez <b>Spindeck.vst3</b> dans <b>~/Bibliothèque/Audio/Plug-Ins/VST3</b> (et <b>Spindeck.component</b> dans <b>Components</b> pour la version AU).",
                       "Le binaire est universel (Apple Silicon et Intel), macOS 10.13 minimum. Relancez Reaper et refaites un scan des plugins."]),
          ("h2", "Mise en place dans Reaper"),
          ("p", "Spindeck est un <b>instrument</b> : il produit le son à partir des fichiers audio et reçoit les commandes MIDI de la platine. "
                "Il apparaît dans la liste des instruments sous <b>Spindeck (NINE HANDS | DSP)</b>."),
          ("table", [["Étape", "Réglage"],
                     ["1", "Insérez Spindeck sur une piste (FX > Instruments)."],
                     ["2", "Armez la piste en enregistrement et activez le <b>monitoring</b> (bouton haut-parleur de la piste)."],
                     ["3", "Entrée de la piste : <b>Input: MIDI > ION Discover DJ > All channels</b>."],
                     ["4", "LED de la platine : bouton Routing de la piste > <b>MIDI Hardware Output</b> > ION Discover DJ."],
                     ["5", "Pour scratcher sans retard : pilote ASIO avec un buffer de <b>128 ou 256 échantillons</b>."]],
           [16, None]),
          ("p", "La Discover DJ n'a pas de carte son : le son sort par l'interface audio de l'ordinateur, sur la sortie master stéréo. "
                "Ces étapes sont rappelées dans le plugin par <b>MENU > Reaper setup help</b>."),
          ("p", "Au premier lancement, la bibliothèque affiche le dossier <b>Musique</b> de votre session. Choisissez votre dossier de morceaux "
                "avec l'icône <b>dossier</b> à côté du titre LIBRARY : il est mémorisé pour toutes les instances du plugin."),
          ("pb",)]

    # 3 -------------------------------------------------------------------------------------------------
    b += [("h1", "3. Vue d'ensemble de l'interface"),
          ("img", "annotated.png", 174, "Interface de Spindeck (skin Midnight, mode scratch actif)."),
          ("table", [["N°", "Élément", "N°", "Élément"],
                     ["1", "Contrôleur détecté (le voyant clignote à chaque message MIDI reçu)", "14", "BEAT : position dans la mesure"],
                     ["2", "SCRATCH : mode scratch des jog wheels", "15", "CUE et PLAY"],
                     ["3", "LIBRARY : bibliothèque en plein écran", "16", "Bouton START et lampe stroboscopique"],
                     ["4", "VU-mètre master (gauche / droite)", "17", "Main : le mode scratch est actif"],
                     ["5", "MENU : réglages, skins, taille de la fenêtre", "18", "EQ HIGH / LOW et FILTER"],
                     ["6", "Formes d'onde défilantes A et B", "19", "Fader de volume de la voie"],
                     ["7", "Titre, temps restant, BPM et pitch", "20", "VU-mètres des voies A et B"],
                     ["8", "Vue d'ensemble du morceau (clic = déplacement)", "21", "Crossfader"],
                     ["9", "Vinyle : scratch à la souris", "22", "Courbe du crossfader : MIX / SCRATCH"],
                     ["10", "Fader de pitch", "23", "LOAD A / LOAD B"],
                     ["11", "Plage du pitch : 4 / 8 / 16 / 50 %", "24", "MASTER : volume général"],
                     ["12", "SYNC et REV", "25", "Bibliothèque de morceaux"],
                     ["13", "PITCH - / PITCH +", "26", "FULL : plein écran"],
                     ["28", "Résumé des platines (lecture, BPM, temps restant) et heure", "27", "Pré-écoute de la bibliothèque"]],
           [9, 78, 9, None], {"small": True}),
          ("pb",)]

    # 4 -------------------------------------------------------------------------------------------------
    b += [("h1", "4. Les platines"),
          ("img", "deck_a.png", 150, "Platine A. La platine B est son reflet, à droite de la table de mixage."),
          ("h2", "En-tête"),
          ("bullets", ["<b>Titre et artiste</b> : lus dans le nom du fichier sous la forme « Artiste - Titre » (un numéro de piste au début est ignoré), avec le format du fichier.",
                       "<b>Temps restant</b> au dixième de seconde ; il passe en <b>rouge</b> sous les 30 dernières secondes de lecture. Dessous : temps écoulé / durée totale.",
                       "<b>BPM</b> effectif (BPM du morceau multiplié par le pitch) et écart de pitch en pourcentage."]),
          ("h2", "Vue d'ensemble"),
          ("p", "La bande sous l'en-tête montre tout le morceau. La partie déjà jouée est assombrie, le trait jaune marque le point de cue et le trait blanc la position. "
                "<b>Cliquez ou faites glisser</b> pour vous déplacer dans le morceau ; le saut se fait sans clic audio."),
          ("h2", "Formes d'onde défilantes"),
          ("p", "En haut de la fenêtre, une forme d'onde par platine défile sous la tête de lecture centrale (10 secondes visibles). Chaque forme d'onde prend la couleur de sa platine "
                "(celle des boutons LOAD A / B) ; les passages aigus sont plus clairs, les basses plus foncées. La grille de temps est tracée par-dessus, avec un trait plus marqué à chaque mesure, "
                "et le point de cue est repéré par un triangle jaune. La partie déjà jouée, à gauche, est plus sombre."),
          ("cond", 60),
          ("side", "platter.png", 62, [
              ("h2", "Le vinyle"),
              ("p", "Le disque tourne exactement avec le son : à 33 tours, plus vite ou moins vite selon le pitch, à l'envers en marche arrière, et il suit la main pendant un scratch. "
                    "Le reflet irisé reste fixe, comme sous une vraie lampe ; ce sont l'étiquette, les poussières et la rangée intérieure de points stroboscopiques qui tournent. "
                    "La rangée extérieure semble tourner à l'envers, comme sur une platine de club."),
              ("p", "<b>Le bras</b> se pose au bord du disque au début du morceau et avance vers le centre au fil de la lecture. À la fin du morceau, le disque continue de tourner "
                    "pendant que le bras retourne sur son repose-bras, puis il s'arrête. Sans morceau chargé, le bras reste au repos."),
              ("p", "<b>Scratch à la souris</b> : cliquez sur le disque et tournez. Le son suit la souris tour pour tour, que le mode SCRATCH soit actif ou non. Relâchez pour reprendre la lecture.")]),
          ("cond", 40),
          ("side", "start_lamp.png", 30, [
              ("h2", "Bouton START et lampe stroboscopique"),
              ("p", "Comme sur une platine de club, le bouton START indique l'état du moteur : son repère pivote vers la droite quand le disque tourne. "
                    "La lampe stroboscopique éclaire le bord argenté du plateau d'un reflet rouge, dans l'axe du bouton et du centre du disque.")]),
          ("cond", 30),
          ("side", "scratch_hand.png", 22, [
              ("h2", "Icône du mode scratch"),
              ("p", "Quand le mode SCRATCH est actif, une main de la couleur de la platine apparaît en bas à droite de chaque vinyle.")]),
          ("pb",)]

    # 5 -------------------------------------------------------------------------------------------------
    b += [("h1", "5. Commandes de lecture"),
          ("side", "deck_buttons.png", 52, [
              ("h2", "PLAY"),
              ("p", "Lance ou arrête la lecture. Le démarrage et le freinage du moteur sont progressifs, comme sur une platine."),
              ("h2", "CUE"),
              ("bullets", ["<b>Pendant la lecture</b> : retour au point de cue et arrêt.",
                           "<b>À l'arrêt sur le point de cue</b> : la lecture dure tant que CUE est maintenu, puis revient au cue au relâchement. Appuyez sur PLAY pendant l'appui pour continuer à jouer.",
                           "<b>À l'arrêt ailleurs</b> : place le point de cue à cet endroit."]),
              ("p", "Au chargement, le cue se place automatiquement sur le premier son du morceau.")]),
          ("h2", "SYNC"),
          ("p", "Cale le tempo de la platine sur celui de l'autre platine. Un morceau à 70 BPM peut se caler sur un morceau à 140 BPM : SYNC choisit le double ou la moitié "
                "quand c'est plus proche. Si l'écart dépasse la plage du pitch, la plage s'élargit automatiquement. Quand les deux platines jouent, SYNC aligne aussi les temps (phase). "
                "Le bouton clignote sur chaque temps."),
          ("h2", "REV"),
          ("p", "Marche arrière tant que le bouton est maintenu."),
          ("h2", "PITCH - / PITCH +"),
          ("p", "Changent le tempo par pas de 0,1 %. Maintenus, ils se répètent (après 0,35 s, puis toutes les 50 ms)."),
          ("side", "pitch.png", 14, [
              ("h2", "Fader de pitch et plage"),
              ("p", "Le fader règle la vitesse de lecture (vitesse et hauteur varient ensemble, comme sur un vinyle). Sa valeur en pourcentage s'affiche au-dessus. "
                    "Le bouton sous le fader change la plage : <b>4, 8, 16 ou 50 %</b>. Double-clic sur le fader : retour à 0."),
              ("h2", "BEAT"),
              ("p", "Les quatre cases montrent la position dans la mesure ; la première case, en blanc, marque le premier temps. Pratique pour caler deux morceaux à l'œil.")]),
          ("pb",)]

    # 6 -------------------------------------------------------------------------------------------------
    b += [("h1", "6. La table de mixage"),
          ("side", "mixer.png", 72, [
              ("h2", "Égaliseur HIGH / LOW"),
              ("p", "Deux potards, comme sur la Discover DJ : <b>HIGH</b> pour les aigus (au-dessus de 2,5 kHz) et <b>LOW</b> pour les basses (sous 250 Hz) ; les médiums restent à plat. Au centre (point de repère sur le potard), le son est intact. À droite : jusqu'à <b>+6 dB</b>. "
                    "À gauche : jusqu'à <b>−26 dB</b>, puis coupure totale de la bande en butée (kill)."),
              ("h2", "FILTER"),
              ("p", "Au centre : aucun effet. Vers la gauche : <b>passe-bas</b> (le son s'assombrit jusqu'à 80 Hz). Vers la droite : <b>passe-haut</b> (les basses disparaissent jusqu'à 8 kHz)."),
              ("h2", "VOL et VU-mètres"),
              ("p", "Fader en haut = 0 dB (gain unité) : Spindeck n'amplifie jamais le morceau, il ne fait que l'atténuer. Les VU-mètres sont gradués en dB et s'éteignent quand il n'y a pas de son.")]),
          ("h2", "Crossfader et courbe"),
          ("bullets", ["<b>MIX</b> (par défaut) : les deux platines restent à plein volume jusqu'au centre, puis l'une s'efface en douceur. Idéal pour les transitions.",
                       "<b>SCRATCH</b> : coupure franche aux bords de la course. Un léger mouvement suffit pour ouvrir ou couper le son, comme sur une table de scratch."]),
          ("h2", "MASTER et LOAD"),
          ("p", "MASTER règle le volume de sortie, en haut à 0 dB, protégé par un limiteur de crête transparent : tant que le signal reste sous −0,3 dBFS il sort intact ; au-dessus (deux morceaux forts mélangés, EQ poussé), le volume baisse un instant au lieu de saturer. LOAD A et LOAD B chargent le morceau sélectionné dans la bibliothèque."),
          ("p", "Tous les potards et faders sont <b>automatisables</b> dans Reaper. Clic droit sur l'un d'eux : <b>MIDI Learn</b>, <b>Forget MIDI</b> ou <b>Reset</b>. Double-clic : valeur par défaut. "
                "La molette de la souris fonctionne aussi."),
          ("pb",)]

    # 7 -------------------------------------------------------------------------------------------------
    b += [("h1", "7. Bibliothèque et chargement"),
          ("img", "library.png", 120, None),
          ("bullets", ["<b>Icône dossier</b> (à côté du titre LIBRARY) : choisit le dossier de musique ; ses sous-dossiers sont aussi parcourus (jusqu'à 20 000 morceaux).",
                       "<b>Icône flèche circulaire</b> : relit le dossier après avoir ajouté des fichiers. Le nombre de morceaux est affiché juste après ces icônes.",
                       "<b>Recherche</b> (loupe, après le nombre de morceaux) : cliquez dessus pour ouvrir le champ, puis tapez un ou plusieurs mots (Échap efface et referme) ; ils sont cherchés dans l'artiste, le titre et le nom du dossier.",
                       "<b>Colonnes</b> : numéro, artiste, titre, BPM, format, dossier. Le BPM apparaît une fois le morceau analysé, et il est mémorisé pour la prochaine fois.",
                       "<b>LIBRARY</b> (barre du haut, ou bouton bibliothèque de la platine) : bibliothèque en plein écran, formes d'onde gardées en haut."]),
          ("h2", "Pré-écoute"),
          ("p", "La pré-écoute fait entendre un morceau de la bibliothèque sans le charger sur une platine. Le fichier est lu directement depuis le disque, sans analyse, et démarre aussitôt."),
          ("p", "Le lecteur de pré-écoute est au centre du bandeau, entre la loupe (à gauche) et les boutons A / B (à droite)."),
          ("bullets", ["<b>Bouton rond vert</b> : lance la pré-écoute du morceau sélectionné ; il affiche un carré (arrêt) pendant la lecture.",
                       "<b>Icône lecture</b> au début de chaque ligne : lance la pré-écoute de cette ligne, ou l'arrête si elle joue déjà. La ligne en cours affiche un petit égaliseur vert.",
                       "<b>Suivi de la sélection</b> : pendant une pré-écoute, choisir une autre ligne (souris ou encodeur BROWSE de la platine) passe directement à ce morceau.",
                       "<b>Barre de position</b> : cliquez ou faites glisser pour aller plus loin dans le morceau. Le temps écoulé et la durée sont affichés à droite.",
                       "<b>Petit potard vert</b> au bout du lecteur : volume de la pré-écoute (double-clic : 70 %), mémorisé."]),
          ("p", "<b>Sortie de la pré-écoute</b> (MENU > Preview output) : <b>Master (mixed in)</b>, par défaut, mélange la pré-écoute au son principal. C'est la seule possibilité avec la Discover DJ de base, qui n'a pas de sortie casque : "
                "baissez le crossfader ou les volumes avant d'écouter devant un public. <b>Outputs 3-4 (Preview bus)</b> envoie la pré-écoute sur une deuxième sortie stéréo du plugin, à router dans Reaper "
                "vers une autre sortie de votre interface audio (un casque par exemple) : activez les sorties 3-4 du plugin dans le routage de la fenêtre FX."),
          ("h2", "Charger un morceau"),
          ("bullets", ["Sélectionnez une ligne puis <b>LOAD A</b> / <b>LOAD B</b> de la table de mixage, ou les boutons <b>← A</b> / <b>B →</b> de la bibliothèque.",
                       "<b>Double-clic</b> sur une ligne : chargement sur la platine A si elle est à l'arrêt, sinon sur la B.",
                       "<b>Glisser-déposer</b> une ligne de la bibliothèque, ou un fichier depuis l'Explorateur Windows / le Finder, sur une platine.",
                       "Depuis la Discover DJ : encodeur <b>BROWSE</b> pour choisir, boutons <b>LOAD</b> pour charger."]),
          ("p", "Par sécurité, une platine <b>doit être à l'arrêt</b> pour recevoir un morceau. Au chargement, le morceau est décodé en mémoire puis analysé "
                "(BPM et grille de temps) ; l'en-tête affiche « Loading and analysing... » pendant quelques secondes. "
                "Formats : WAV, AIFF, FLAC, OGG Vorbis et MP3, jusqu'à 60 minutes par morceau. Les morceaux chargés et leurs points de cue sont enregistrés avec le projet Reaper."),
          ("pb",)]

    # 8 -------------------------------------------------------------------------------------------------
    b += [("h1", "8. La platine ION Discover DJ"),
          ("p", "La Discover DJ est reconnue dès l'ouverture du plugin, sans réglage. Correspondance des commandes :"),
          ("table", [["Commande de la platine", "Action dans Spindeck"],
                     ["Jog wheels", "Mode SCRATCH + main posée sur le jog : scratch. Sans le mode SCRATCH : pousse ou freine le disque pendant la lecture (calage à l'oreille), déplacement audible à l'arrêt."],
                     ["SCRATCH (bouton central)", "Active / coupe le mode scratch des deux jog wheels (LED de la platine allumée en mode scratch)."],
                     ["PLAY / CUE / SYNC", "Mêmes fonctions qu'à l'écran. La LED SYNC clignote sur les temps."],
                     ["REV", "Marche arrière tant que le bouton est maintenu."],
                     ["Pitch - / +", "Tempo par pas de 0,1 %, avec répétition."],
                     ["Crossfader", "Crossfader de Spindeck (inversé par défaut, voir ci-dessous)."],
                     ["Volume / Bass / Treble", "Volume de la voie, LOW et HIGH de l'égaliseur."],
                     ["Master", "Volume master."],
                     ["BROWSE", "Fait défiler la bibliothèque."],
                     ["LOAD A / LOAD B", "Charge le morceau sélectionné sur la platine."],
                     ["Bouton bibliothèque", "Bibliothèque en plein écran (comme LIBRARY)."]],
           [44, None]),
          ("h2", "Réglages de la platine (MENU)"),
          ("bullets", ["<b>Invert crossfader (controller)</b> : la Discover DJ envoie son crossfader à l'envers ; il est donc <b>inversé par défaut</b>. Décochez si votre crossfader réagit à l'envers.",
                       "<b>Scratch sensitivity (controller)</b> : sensibilité des jog wheels, <b>25, 50 (par défaut), 75 ou 100 %</b>. Le scratch à la souris n'est pas concerné.",
                       "<b>Controller > None (MIDI Learn only)</b> : désactive la reconnaissance de la Discover DJ pour piloter Spindeck avec un autre contrôleur, uniquement par MIDI Learn.",
                       "<b>Refresh controller LEDs</b> : renvoie l'état de toutes les LED à la platine (utile si elle a été rebranchée)."]),
          ("p", "Les LED PLAY, CUE, SYNC, REV et SCRATCH de la platine suivent le logiciel, à condition d'avoir activé la sortie MIDI de la piste vers la platine (chapitre 2)."),
          ("h2", "Autres contrôleurs MIDI"),
          ("p", "Clic droit sur un potard ou un fader > <b>MIDI Learn</b>, puis bougez la commande voulue sur votre contrôleur : elle est assignée. "
                "Aucun CC n'est assigné d'usine. Les assignations sont enregistrées avec le projet."),
          ("h2", "MIDI Learn par défaut"),
          ("p", "Pour retrouver vos assignations dans tous vos projets sans les refaire, enregistrez-les comme assignation par défaut : "
                "<b>MENU > MIDI Learn > Save as default</b>. Chaque nouvelle ouverture de Spindeck, dans n'importe quel projet, rappelle alors ces assignations. "
                "Pour changer l'assignation par défaut, refaites vos MIDI Learn puis enregistrez à nouveau."),
          ("table", [["Entrée du menu MIDI Learn", "Rôle"],
                     ["Save as default", "Enregistre les assignations actuelles comme assignation par défaut."],
                     ["Load default", "Remplace les assignations du projet par l'assignation par défaut."],
                     ["Clear (this project)", "Efface les assignations de ce projet (l'assignation par défaut est conservée)."],
                     ["Delete default", "Supprime l'assignation par défaut : les nouvelles ouvertures repartent sans assignation."]],
           [52, None]),
          ("p", "Un projet déjà enregistré garde ses propres assignations : il n'est pas modifié par un nouveau réglage par défaut. "
                "L'en-tête du menu indique le nombre d'assignations du projet et de l'assignation par défaut."),
          ("pb",)]

    # 9 -------------------------------------------------------------------------------------------------
    b += [("h1", "9. Fenêtre, skins et menu"),
          ("h2", "Plein écran"),
          ("p", "Le bouton <b>FULL</b> de la barre du haut (ou <b>MENU > Full screen</b>) agrandit la fenêtre à tout l'écran sur lequel elle se trouve, barre des tâches exclue, en gardant les proportions de l'interface. "
                "Si vous déplacez la fenêtre sur un autre écran, ou si la résolution change, elle se réadapte automatiquement, même sur un écran plus petit que la taille par défaut. "
                "Appuyez à nouveau sur FULL pour revenir à la taille et à la position d'avant. Le plein écran est mémorisé avec le projet."),
          ("p", "Sous Windows, la fenêtre FX de Reaper est aussi replacée sur l'écran. Si Spindeck est ancré dans le docker de Reaper, seule la taille du plugin change."),
          ("h2", "Taille de la fenêtre"),
          ("p", "La fenêtre s'ouvre à <b>100 %</b> (1600 × 900 pixels), sa taille minimale. Clic droit sur le fond ou <b>MENU > Plugin size</b> : <b>100 %, 125 % ou 150 %</b>. "
                "Le sous-menu <b>Default size (new windows)</b> choisit la taille d'ouverture des nouvelles fenêtres. On peut aussi tirer le coin inférieur droit ; les proportions sont conservées. "
                "Chaque projet mémorise la taille de sa fenêtre."),
          ("h2", "Skins"),
          ("p", "<b>MENU > Skin</b> change l'habillage. Le choix est mémorisé pour toutes les instances."),
          ("img", "skins.png", 168, None),
          ("h2", "Couleurs et halo"),
          ("p", "<b>MENU > Colours & glow</b> personnalise l'interface, comme dans les autres plugins NINE HANDS | DSP. Les réglages sont mémorisés pour toutes les ouvertures du plugin."),
          ("bullets", ["<b>Deck A colour... / Deck B colour...</b> : ouvrent le nuancier (couleurs rapides, rangée MY COLOURS pour garder vos couleurs, teinte, saturation, luminosité, code hexadécimal). "
                       "La couleur s'applique à tout ce qui appartient à la platine : forme d'onde, bande du morceau, étiquette du vinyle, boutons LOAD / A / B / SYNC, fader de pitch, VU-mètre et cases BEAT. <b>DEFAULT</b> rend la couleur du skin.",
                       "<b>Strobe light colour...</b> : couleur du reflet de la lampe stroboscopique sur le plateau.",
                       "<b>Glow</b> : intensité du halo des boutons allumés, de la main du mode scratch et de la lampe : Off, Low, Medium (design d'origine) ou High.",
                       "<b>Reset colours to the skin</b> : revient aux couleurs du skin choisi."]),
          ("h2", "Contenu du MENU"),
          ("table", [["Entrée", "Rôle"],
                     ["Controller", "ION Discover DJ ou None (MIDI Learn seul)."],
                     ["Invert crossfader (controller)", "Inverse le crossfader de la platine (coché par défaut)."],
                     ["Scratch sensitivity (controller)", "Sensibilité des jog wheels : 25 / 50 / 75 / 100 %."],
                     ["Refresh controller LEDs", "Renvoie l'état des LED à la platine."],
                     ["MIDI Learn", "Assignation par défaut : enregistrer, charger, effacer (voir chapitre 8)."],
                     ["Skin", "Midnight, Neon Club, Amber Mono, Pro Red / Blue."],
                     ["Colours & glow", "Couleurs des platines et de la lampe, intensité du halo."],
                     ["Choose music folder...", "Même rôle que l'icône dossier de la bibliothèque."],
                     ["Preview output", "Pré-écoute mélangée au master, ou sur les sorties 3-4 du plugin."],
                     ["Full screen", "Plein écran (comme le bouton FULL)."],
                     ["Reaper setup help", "Rappel des réglages de la piste Reaper."],
                     ["Plugin size", "100 / 125 / 150 % et taille par défaut des nouvelles fenêtres."]],
           [56, None]),
          ("pb",)]

    # 10 ------------------------------------------------------------------------------------------------
    b += [("h1", "10. Conseils d'utilisation"),
          ("bullets", ["<b>Transition simple</b> : chargez le morceau suivant sur la platine arrêtée, appuyez sur SYNC, lancez-le sur le premier temps (cases BEAT), puis montez le crossfader en coupant progressivement les basses (LOW) du morceau sortant.",
                       "<b>Scratch</b> : passez la courbe du crossfader sur <b>SCRATCH</b>, activez le mode SCRATCH, posez la main sur le jog et ouvrez / coupez le son avec le crossfader.",
                       "<b>Scratch trop vif ou trop mou</b> : réglez <b>Scratch sensitivity</b> dans le MENU. Un buffer audio de 128 ou 256 échantillons rend le scratch plus réactif.",
                       "<b>Préparer un set</b> : chargez une fois vos morceaux pour que leur BPM soit analysé ; il reste ensuite affiché dans la bibliothèque.",
                       "<b>Enregistrer un mix</b> : le son de Spindeck est celui de la piste Reaper ; armez-la en enregistrement audio (sortie stéréo de la piste) ou enregistrez le master.",
                       "<b>Pas de pré-écoute casque</b> : la Discover DJ de base n'ayant pas de carte son, Spindeck sort uniquement le mix master."]),
          ("pb",)]

    # 11 ------------------------------------------------------------------------------------------------
    b += [("h1", "11. Caractéristiques techniques"),
          ("table", [["Format", "Instrument VST3 (Windows, macOS) et AU (macOS), binaire universel Apple Silicon / Intel"],
                     ["Sorties", "Master stéréo (1-2), pré-écoute stéréo optionnelle (3-4) ; entrée MIDI (commandes) et sortie MIDI (LED de la platine)"],
                     ["Contrôleur reconnu", "ION Discover DJ (mapping d'après celui de Mixxx) ; tout contrôleur MIDI par MIDI Learn"],
                     ["Fichiers lus", "WAV, AIFF, FLAC, OGG Vorbis, MP3 ; 60 minutes maximum par morceau ; tout taux d'échantillonnage"],
                     ["Lecture", "Interpolation Hermite 4 points, démarrage ~60 ms, freinage ~200 ms, saut de position sans clic"],
                     ["Pitch", "± 4 / 8 / 16 / 50 %, pas de 0,1 % aux boutons"],
                     ["Analyse", "BPM 75 à 180 (précision 0,01 BPM) et grille de temps, en tâche de fond"],
                     ["Égaliseur", "Aigus et basses, filtres Linkwitz-Riley 4e ordre (250 Hz / 2,5 kHz), +6 dB / −26 dB puis coupure"],
                     ["Filtre", "Passe-bas 20 kHz → 80 Hz, passe-haut 20 Hz → 8 kHz, résonance douce"],
                     ["Crossfader", "Courbes MIX (puissance constante) et SCRATCH (coupure franche)"],
                     ["Sortie master", "Faders à 0 dB en butée haute, limiteur de crête transparent (−0,3 dBFS), coupe-continu, silence quand les platines sont à l'arrêt"],
                     ["Fenêtre", "1600 × 900 à 100 % ; 125 % et 150 % ; plein écran adaptatif"],
                     ["Pré-écoute", "Lecture en continu depuis le disque (tampon de 8 s), sortie master ou sorties 3-4"],
                     ["Réglages communs", "Windows : %APPDATA%\\Nine Hands DSP\\Spindeck ; macOS : ~/Library/Nine Hands DSP/Spindeck"],
                     ["Version", VERSION]],
           [40, None], {"header": False})]
    return b


L.build(OUT, "Spindeck", VERSION, "#36C5F0", "SPINDECK", TEXTS, content, langs=("fr",))
