Presets shared inside the PatSuite plugins.

tools/collect-presets.ps1 copies your own presets from Documents\PatSuite\<Plugin>\Presets into
presets\<Plugin>\ and the next build ships them inside each plugin, so whoever installs it gets
them too. Sub-folders become categories; a sub-folder named "Privado" is never shared.
