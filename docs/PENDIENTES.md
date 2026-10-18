# Pendientes y cosas no verificadas

## Fase 1

- **No probado dentro de FL Studio.** PatTemplate solo se probó como versión suelta, con capturas
  automáticas. Es un banco de pruebas: la carga en FL se verifica con los plugins reales.
- **Pausa al minimizar.** La lógica usa `isShowing()` de JUCE, que da falso con la ventana cerrada o
  minimizada. Falta comprobarlo con la ventana del plugin dentro de FL, minimizada u oculta detrás de otra.
- **Costo de dibujo.** Se midió dibujando en software sobre una imagen: entre 3 y 6 ms por cuadro a
  900×560. En pantalla JUCE dibuja con Direct2D (GPU), que suele ser más barato. La medición real de CPU
  queda para la fase 6.
- **Detección de GPU solo en Windows** (DXGI). En otros sistemas cuenta como "sin GPU dedicada", así que
  arranca en nivel Bajo (se puede cambiar a mano). Por ahora no hay versión para Mac ni Linux.
- **Instalador PatSuite:** ya reserva lugar para los 4 plugins nuevos, pero no se ejecutó en otra PC.
- **Ambigüedades a resolver antes de la fase 5 (PatTune):** "In Sen" e "Insen" suelen ser la misma
  escala; falta definir la afinación de los maqamat (24-EDO exacto o afinaciones tradicionales, que
  varían por región).

## Fases 2 a 6

- Nada se probó todavía dentro de FL Studio (quedan instalados para que los pruebes). Ver `docs/PRUEBAS.md`.
- PatSidechain: falta comprobar en FL el ruteo del sidechain (en el mixer: "Sidechain to this track")
  y que el bus auxiliar quede activo.
- PatTune: probado con tonos sintéticos. Con voces reales pueden hacer falta ajustes finos del gate y del
  humanize. La calidad de PSOLA baja en pasajes muy rápidos o con mucha respiración.
- PatTune: no tiene corrección por MIDI (notas objetivo desde un teclado): no estaba en el pedido.
- PatDoubler: las copias suenan a "delay + 16 ms" por el desplazador de tono.
- Sin presets en los plugins de la suite (no estaban en el pedido). Los ajustes se guardan con el proyecto de FL.
