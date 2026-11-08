#pragma once
// NINE HANDS | DSP - acces a la fenetre de l'hote (plein ecran du plugin). Windows seulement ; ailleurs les fonctions renvoient false.
#include <juce_gui_basics/juce_gui_basics.h>

namespace nh
{
    // rectangles en pixels physiques : fenetre racine de l'hote (fenetre FX de Reaper) et vue du plugin
    bool hostWindowRects (void* pluginNativeHandle, juce::Rectangle<int>& rootPhys, juce::Rectangle<int>& selfPhys);
    // deplace la fenetre racine de l'hote (coin haut gauche, pixels physiques), sans la redimensionner
    bool moveHostWindow (void* pluginNativeHandle, int xPhys, int yPhys);
}
