# Fase 6: pruebas

Medido el 2026-10-17 en el equipo de desarrollo: i3-10100F (4 núcleos), 8 GB de RAM, GTX 1050 Ti,
Windows 10, 48 kHz. Detección automática de nivel: **Medio**.

## Tests automáticos

`tools/build.ps1 -Test`: **41 tests, todos pasan.** Cubren la cola lock-free (también con dos hilos),
la regla de niveles, las tablas de afinación (12 notas, cuartos de tono, maqam, períodos que no son la
octava) y cada motor de audio con señales sintéticas.

## Latencia

| Plugin | Latencia | Cómo se verificó |
|---|---|---|
| PatDeEsser | 0 muestras | Test `ZeroLatency`: un impulso sale en la muestra 0. |
| PatSidechain | 0 muestras | El ducking se aplica en la misma muestra; sin buffers de anticipación. |
| PatDoubler | 0 muestras para la señal seca | Test `CopyArrivesAfterItsDelay`. Las copias suenan a propósito a "delay + 16 ms". |
| PatTune | 1594 muestras (33,2 ms a 48 kHz) | Test `AutoOffIsAPureDelayOfTheReportedLatency`. Se informa al host con `setLatencySamples`, y FL Studio la compensa. |

## CPU del audio (un núcleo, señal estéreo, bloques de 256)

| Plugin | CPU |
|---|---|
| PatDeEsser | 0,2 % |
| PatSidechain | 0,2 % |
| PatDoubler | 0,5 % (4 copias, desafinación máxima) |
| PatTune | 3,0 % (detección + PSOLA + formantes) |

El audio no depende del nivel visual ni del modo Neón fijo: el motor hace exactamente lo mismo.

## Costo de dibujo por nivel

Tiempo para dibujar la ventana completa, en software y a tamaño 1:1 (modo diagnóstico
`PATSUITE_SNAPSHOT`, promedio de las vistas Simple y Pro). La última columna estima el peor caso:
redibujar la ventana entera en cada cuadro, a 20 fps en Bajo y 60 fps en Alto.

| Plugin | Bajo (ms) | Bajo + neón fijo (ms) | Medio (ms) | Alto (ms) | Alto + neón fijo (ms) | Peor caso Bajo / Alto * |
|---|---|---|---|---|---|---|
| PatDeEsser | 4,7 | 4,1 | 4,8 | 6,5 | 4,5 | 9 % / 39 % |
| PatSidechain | 4,9 | 4,3 | 5,5 | 8,6 | 5,8 | 10 % / 52 % |
| PatDoubler | 7,2 | 5,6 | 6,8 | 10,3 | 6,2 | 14 % / 62 % |
| PatTune | 6,9 | 4,7 | 4,7 | 5,3 | 4,5 | 14 % / 32 % |

\* Es una cota superior, no lo que se va a ver en pantalla. En la ventana real:
- JUCE dibuja con Direct2D (GPU);
- la capa estática está guardada en caché;
- solo se repintan las zonas que cambian;
- en Bajo, el espectro y los medidores no se repintan si nada cambió.

Con Neón fijo (10 fps) el peor caso queda entre el 4 % y el 6 % de un núcleo para cualquier plugin.

## Neón fijo, verificado con capturas

- Sin glow en perillas, curvas ni bandas, y sin anillos ni partículas animadas.
- Medidores sin animación de caída: saltan al valor, refrescados cada 100 ms (10 fps).
- El estado de la cabecera muestra "neon fixed | 10 fps".
- En "Auto", Neón fijo se enciende solo en el nivel Bajo.

## No verificado (requiere probar en FL Studio)

- Uso de CPU real dentro de FL con la ventana abierta y cerrada.
- Que la pausa de la visual funcione con la ventana minimizada u oculta detrás de FL.
- Ruteo del sidechain de FL hacia PatSidechain (bus de entrada auxiliar VST3).
- Calidad de PatTune con voces reales: los tests usan tonos sintéticos tipo vocal.
- Que el teclado de la PC siga tocando notas después de hacer clic en los plugins (corregido en código,
  sin probar todavía dentro de FL).
