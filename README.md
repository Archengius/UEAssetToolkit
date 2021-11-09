# Unreal Engine Asset Toolkit
A set of 2 plugins, one for generating asset dumps from the game and second for making assets from them in editor

## Creating Asset Dumps
For dumping assets from the game, install AssetDumper plugin in the Shipping game and type 'dumper.OpenAssetDumper' in console. 
Tweak settings and asset whitelist as needed, then use Dump button to generate a list of dumped asset files and source art.

## Importing Assets from Dumps into Editor
To import assets into the editor, navigate to Window -> Developer Tools -> Asset Generator.
Specify directory with the asset dump in the settings, tweak them as necessary, and press Generate Assets.
It will also automatically refresh existing assets and create dependencies for the assets you have selected, even if you haven't checked them for generation.
