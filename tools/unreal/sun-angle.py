"""SUN_ANGLE_001 : pose LightSourceAngle sur toutes les lumieres directionnelles du monde de l'editeur.

    py <ce fichier> <angle en degres>

Pour un A/B de capture-sky : un etat "a1=...;py C:/.../tools/unreal/sun-angle.py 1.0". La commande
console `set DirectionalLightComponent LightSourceAngle` ne touche PAS les lumieres posees (SUN_ANGLE_001 :
10 degres demandes, ombres inchangees) ; le setter, lui, marque le rendu sale.

Le setter marque l'etat de rendu sale ; la valeur relue est ecrite au log (SUN_ANGLE_SET).
"""
import sys
import unreal

angle = float(sys.argv[1])
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for light in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DirectionalLight):
    comp = light.light_component
    comp.set_light_source_angle(angle)
    unreal.log('SUN_ANGLE_SET %s angle=%.4f moon=%d' % (light.get_name(), comp.get_editor_property('light_source_angle'),
                                                       1 if light.actor_has_tag('AnastasisMoon') else 0))
