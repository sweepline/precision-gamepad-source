# Precision Gamepad Source
OBS plugin for simple gamepad visualization, made to replace https://github.com/piatf/padviz using XInput directly.

## Installation
1. Download the release and close OBS
2. Overwrite the folders in your obs-studio install with the ones from the zip (eg. data and obs-plugins)

## Building
The way I've been building this is putting the content in sources of obs and building OBS with the plugin. Then extracting the plugin, this requires knowledge of CMake..

## TODO
* Outlines (This is annoying to do as I have to manually draw the lines... I should make a file to load for the geometries.)
* DirectInput (This is even worse...)
* Github automatic builds?
