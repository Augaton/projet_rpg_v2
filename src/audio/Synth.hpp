#pragma once

class AudioManager;

// Effets sonores synthétisés au démarrage (aucun fichier requis) :
//   sx_click   clic d'interface          sx_error   action refusée
//   sx_ready   arme chargée              sx_alarm   coque critique / phase du boss
//   sx_shield  impact sur bouclier       sx_cloak   invisibilité
//   sx_hack    piratage                  sx_unlock  déblocage / récompense rare
void RegisterSynthSounds(AudioManager& audio);
