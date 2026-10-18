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

## Fases 2 a 5: plugins

| Decisión | Por qué |
|---|---|
| **Foco del teclado:** ningún control toma el foco del teclado (`releaseKeyboard` + `EDITOR_WANTS_KEYBOARD_FOCUS FALSE` + `keyPressed` que devuelve `false`) | FL Studio usa el teclado de la PC como teclado MIDI. Si un control del plugin se queda con el foco, FL deja de recibir esas teclas. |
| **PatDeEsser:** detector pasabanda TPT con ganancia unitaria en el centro; compresor con curva suave de 6 dB | La división por filtro de variables de estado reconstruye la señal exacta (`x = resto + banda`), así que en modo Split la voz no se colorea cuando no hay reducción (test `SplitIsTransparentWhenNotReducing`). |
| PatDeEsser: **"Smooth"** es una macro de la interfaz que mueve threshold y ratio | Pedido del usuario. Los valores reales se ven y se automatizan en Pro. |
| PatDeEsser: se agregaron **Release** y **Output** | Aprobado por el usuario. |
| **PatSidechain:** modos Tempo / Sidechain / Auto. "Pump" es la profundidad en los dos | La vista simple pedida (Pump + 1/4, 1/8, 1/16) necesita un modo a tempo que no depende de rutear el bombo. La vista pro es un compresor sidechain con filtro en la cadena lateral. Auto elige Sidechain mientras entra señal por ese bus. |
| PatSidechain: el ducking a tempo sigue la posición de la canción (ppq del host) | Así queda en fase con el bombo aunque arranques la reproducción en cualquier lugar. |
| **PatDoubler:** desafinación con desplazador de tono de dos lecturas cruzadas (ventana de 24 ms); variación de tiempo con deriva aleatoria suave | Es liviano y suena limpio para desafinaciones chicas (±50 cents). Contra: agrega 16 ms fijos a cada copia (documentado). |
| PatDoubler: Width y Thickness son parámetros globales que escalan los valores de cada copia | No hay macros que sincronizar: la vista simple y la pro conviven sin conflicto. |
| **PatTune, detección:** método McLeod (NSDF por FFT), ventana de unos 43 ms, salto de unos 5 ms, rango de 70 a 1000 Hz | Es preciso con voces (±0,3 % en los tests) y da una "claridad" que se usa directamente como medidor de confianza. |
| **PatTune, corrección:** TD-PSOLA con marcas fraccionarias exactamente a un período de distancia | PSOLA conserva los formantes (la voz no suena "ardilla"). Las marcas fraccionarias evitan el desfasaje con períodos no enteros (se detectó y corrigió con tests). El formant shift se hace leyendo cada grano más rápido o más lento. |
| PatTune: latencia de 33 ms informada al host | PSOLA necesita ver un poco más de dos períodos de la nota más grave. FL compensa la latencia. |
| PatTune: **escalas.** Maqamat en 24-EDO; "In Sen / Insen" como una sola escala; escala dibujada con 24 pasos de cuarto de tono | Decidido por el usuario. El motor acepta cualquier tabla de cents (`TuningTable`), así que agregar más escalas es solo agregar datos. |
| `TuningTable` de capacidad fija (128 grados, sin memoria dinámica) | El hilo de audio la reconstruye cuando cambia la escala o la tonalidad sin reservar memoria. |
| Retune speed como constante de tiempo; humanize la estira en desvíos chicos de notas sostenidas | Con 0 ms da el efecto robótico; con humanize se conserva el vibrato natural. |

## Segunda tanda: presets, Patan, PatPatPat y PatQpro

| Decisión | Por qué |
|---|---|
| **Presets comunes** (`common/ui/Presets`): de fábrica en código, del usuario en `Documentos\PatSuite\<Plugin>\Presets` (`.patpreset`, XML), compartidos dentro del plugin con `tools/collect-presets.ps1` | Pedido del usuario: los presets propios viajan dentro del plugin al compartirlo. La subcarpeta "Privado" nunca se comparte. Cargar un preset vuelve todo a los valores por defecto y aplica solo lo que el preset define. |
| Barra de presets en la cabecera de todos los plugins (anterior / menú por carpetas / siguiente / SAVE) | Mismo lugar en todos. El nombre del preset queda guardado en el proyecto de FL. |
| Estado que no es parámetro (curva de Patan, escala dibujada de PatTune) viaja en los presets como "extras" | Así un preset reproduce el sonido completo. |
| **Patan:** curva dibujada (puntos con curvatura, tramos suaves o escalones), modos Pan / Volumen / Ancho, ciclo en notas o en Hz, leyes de paneo Balance / -3 / -4,5 / -6 dB | Auto-panner de curva dibujada con estética propia. La curva pasa al audio con un triple buffer sin bloqueos. Mientras FL reproduce, la posición sigue la canción. |
| **PatPatPat:** modos Mono / Estéreo / Ping-Pong / Cross, carácter Clean / Tape / Analog / Lo-Fi, ducking, freeze, difusión | Delay completo con ping-pong, pedido por el usuario. La saturación dentro del lazo mantiene acotado el 100 % de feedback (test `FullFeedbackStaysBounded`). Los cambios de tiempo se deslizan como una cinta. |
| PatPatPat: visual de pelotas que rebotan entre L y R con los tiempos y niveles reales de los ecos | Muestra lo que se escucha (usa la misma función `taps()` que se prueba con tests). En Neón fijo queda solo el mapa de ecos. |
| **PatQpro:** 24 bandas; campana, estantes, cortes de 6 a 96 dB/oct y brickwall, notch, pasabanda, tilt y flat tilt; cada banda en estéreo, L, R, Mid o Side | Todo lo que hace un EQ profesional de referencia, con nombres y diseño propios. |
| PatQpro: estantes tipo Butterworth de cualquier pendiente, con la frecuencia en el punto de media ganancia; cortes Butterworth con Q como resonancia | Pendientes reales (no estantes repetidos) y respuesta predecible. Verificado con tests de forma. |
| PatQpro: modos **Zero latency** (biquads), **Natural** (las bandas corren al doble de la frecuencia de muestreo con filtros medio-banda IIR, 3 muestras de latencia) y **Linear phase** (FIR por FFT particionada, latencia 512 + largo/2) | Natural conserva la forma analógica arriba de 10 kHz. Linear no corre la fase (test de respuesta al impulso simétrica). Las bandas dinámicas siguen como filtros naturales después del FIR. |
| PatQpro: el FIR se arma en un hilo aparte y entra al audio sin bloqueos, con un fundido de un bloque | Mover una banda en modo lineal no traba el audio ni hace clicks. |
| PatQpro: bandas dinámicas con umbral y rango propios, detector en la zona de la banda, opción de escuchar el sidechain | EQ dinámico pedido por el usuario. Ataque y release son globales. |
| PatQpro: **detección de choques** entre instancias: cada PatQpro publica su espectro de salida en un "hub" compartido (mismo proceso) | Se elige otra pista (o el sidechain) y se marcan en rojo las frecuencias donde las dos suenan fuerte y parecido. Usa el nombre de la pista de FL cuando el host lo informa. |
| PatQpro: **EQ Match** aprende unos segundos de la referencia (sidechain u otra pista) y de la pista propia, y ajusta campanas y estantes a la diferencia suavizada (1/3 de octava) | Reemplaza las bandas (se puede deshacer). Cantidad de bandas y porcentaje regulables. |
| PatQpro: deshacer/rehacer, auto gain por el promedio de la curva, gain scale, pan y polaridad de salida, solo de banda, nota musical en la lectura, teclado de piano opcional | Funciones habituales de un EQ profesional ("que funcione igual"). |
