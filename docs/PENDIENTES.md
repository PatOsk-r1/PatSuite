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
- ~~Ambigüedades de la fase 5~~ resueltas por el usuario: "In Sen" e "Insen" suelen ser la misma
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

## Segunda tanda (Patan, PatPatPat, PatQpro, presets)

- Nada de esto se probó todavía dentro de FL Studio (queda instalado).
- PatQpro: en modo Linear phase, si hay bandas L/R y Mid/Side a la vez, se aplican primero las L/R y
  después las Mid/Side (en los otros modos se respeta el orden de las bandas).
- PatQpro: la detección de choques funciona entre instancias que corren en el mismo proceso. Si FL
  ejecuta un plugin en un proceso aparte ("bridged"), esa instancia no aparece en la lista.
- PatQpro: el nombre de cada instancia es el de la pista del mixer si FL lo informa; si no,
  "PatQpro 1", "PatQpro 2"...
- PatQpro: el EQ Match ajusta campanas y estantes; no copia diferencias muy finas (a propósito, para
  que suene natural).
- PatPatPat: el tiempo máximo es 4 segundos (notas largas a tempos lentos se recortan a 4 s).
- Patan: no hay copiar/pegar curvas entre instancias (se puede guardar la curva como preset).
