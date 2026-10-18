# Decisiones técnicas

## Fase 1: base común (2026-10-17)

| Decisión | Por qué |
|---|---|
| **Solo VST3** (+ versión suelta para probar sin DAW) | Steinberg dejó de distribuir el kit de VST2 en 2018 y usarlo requiere un acuerdo de licencia. FL Studio maneja VST3 sin problemas. Lo decidió el usuario. |
| **Gratis y de código abierto (AGPL-3.0)** | Pedido del usuario. Además JUCE es AGPL, así que encaja sin pagar licencia comercial. |
| **Nombres:** PatDeEsser, PatSidechain, PatDoubler, PatTune | "Pat" + qué hace cada uno. Se evitó "AutoTune", que es marca registrada de Antares. |
| **Un solo proyecto CMake** para los 4 plugins | Comparten `common/`. Cada plugin es un `juce_add_plugin` en `plugins/<Nombre>`. PatWave y PatVerb siguen como proyectos aparte y el instalador los junta. |
| `common/core` **sin JUCE** | Se prueba con tests automáticos y se reutiliza en el DSP. |
| `common/ui` como **lista de archivos fuente compartida** (`INTERFACE`) que se compila dentro de cada plugin | Cada plugin compila su propia copia con sus macros `JucePlugin_*`. Así se evita una librería JUCE compartida con configuraciones distintas. |
| **Del audio a la UI:** atómicos para los medidores + `SpscFifo<float>` (cola circular sin bloqueos, un productor y un consumidor) para las muestras | El hilo de audio nunca espera ni reserva memoria. Si la UI no lee (editor cerrado), los datos nuevos se descartan. La UI siempre salta a lo más reciente (`skipToNewest`). |
| **Canvas por capas** (`LayeredCanvas`) | La capa estática (grilla, escalas, textos) se dibuja una sola vez en una imagen, a la densidad real de píxeles. En cada cuadro solo se dibuja encima la capa dinámica. |
| **Reloj de cuadros** (`FrameClock`) por editor | Repinta a los fps del nivel, o a 10 Hz en Neón fijo. Si la ventana no se ve (cerrada, minimizada u oculta), deja de dibujar y solo revisa 2 veces por segundo si volvió. |
| **Niveles:** Bajo = 20 fps, FFT 1024 reducida a 48 bandas, repinta solo si algo cambió · Medio = 30 fps, FFT 2048, curvas suavizadas · Alto = 60 fps, FFT 4096, glow | Cumple el pedido. Para Bajo se eligió 20 fps (el pedido decía 15–20) para que los medidores no se vean entrecortados. |
| **Detección de hardware:** núcleos físicos (`getNumPhysicalCpus`), RAM (`getMemorySizeInMegabytes`) y GPU por DXGI (memoria de video dedicada; menos de 512 MB cuenta como integrada) | JUCE no informa la GPU. Regla: **Bajo** con 2 núcleos o menos, menos de 6 GB o sin GPU dedicada; **Alto** con 8 núcleos o más, 16 GB y 4 GB de video (con un margen, porque Windows informa un poco menos); **Medio** en el resto. Este equipo (i3-10100F de 4 núcleos, 8 GB, GTX 1050 Ti de 4 GB) da **Medio**, verificado. |
| **Ajustes compartidos** en `%APPDATA%\PatSuite\settings.xml` | El nivel y el Neón fijo que elijas en un plugin valen para todos y quedan guardados. |
| **Neón fijo:** sin glow, sin animaciones de caída, medidores cada 100 ms. En "Auto" se enciende en el nivel Bajo | Pedido del usuario. Los trazos mantienen el mismo color y contraste. |
| **Vista Simple/Pro** guardada en el estado de cada plugin (`proView` dentro del árbol del APVTS) | Cada instancia recuerda su vista dentro del proyecto de FL. |
| **Lienzo de tamaño fijo, escalado** | El editor se diseña a un tamaño fijo y se escala, así la ventana puede cambiar de tamaño sin rehacer la disposición. |
| **Tablas de afinación arbitrarias** (`TuningTable`): grados en cents dentro de un período (1200 = octava, aunque puede ser otro) | Base del motor de PatTune. Soporta 12 semitonos, cuartos de tono (24-EDO), intervalos neutros de maqam con cualquier valor en cents, escalas dibujadas y períodos que no son la octava. Ya probado con tests. |
| **Modo diagnóstico** `PATSUITE_SNAPSHOT` | Permite verificar cada combinación de nivel × Neón fijo × vista sin manejar la pantalla, y mide el costo de dibujo (`render-cost.csv`). |
