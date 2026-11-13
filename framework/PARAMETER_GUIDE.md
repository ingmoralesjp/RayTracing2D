# Guía de parámetros experimentales - RayTracing2D v1.5.1

Esta guía complementa los comentarios de `simulations/omnetpp.ini`.


## 0. Coordenadas y visualización

- física: coordenadas cartesianas locales en metros, `+X=este`, `+Y=norte`;
- `cCanvas`: unidades visuales, no metros físicos;
- `viewAutoFit=true`: ajusta la geometría al Canvas;
- `viewNorthUp=true`: invierte visualmente Y para mostrar norte arriba;
- `viewAutoFit=false` + `viewWorldMin/Max*`: recorte de visualización local;
- `drawBuildingFootprints`: dibuja polígonos OSM sin cambiar la física;
- `viewCanvasWidth`, `viewCanvasHeight`, `viewMargin`: sólo presentación.

Véase `COORDINATE_SYSTEM.md`.

## 1. Frecuencia y potencia

- `frequency`: frecuencia de portadora.
  - referencia v1.1: `2.4GHz`;
  - exploración de continuidad con tesis: `916MHz`.
- `txPowerDbm`: potencia transmitida usada para convertir path loss a potencia
  recibida. No modela por sí sola una antena real ni EIRP completa.

Al cambiar frecuencia cambian longitud de onda, FSPL, Fresnel, fase y
knife-edge.

## 2. Polarización

`polarization = "TE" | "TM"`

Solo afecta el coeficiente Fresnel. Los ángulos de incidencia NO son parámetros
de barrido: el trazador los calcula a partir de TX, RX y superficie.

## 3. Orden y mecanismos

- `enableLos`
- `enableFirstOrderReflections`
- `enableSecondOrderReflections`
- `enableDiffraction`

v1.1 llega hasta R2. No existe un `maxReflectionOrder=3` oculto.

## 4. ¿Número de rayos?

No hay `rayCount` de lanzamiento porque se usa método de imágenes.

Controles disponibles:

- `maxPathsPerReceiver=0`: todos los caminos válidos retenidos;
- `maxPathsPerReceiver=N`: conserva los N más fuertes DESPUÉS del trazado;
- `minPathPowerDbm`: elimina contribuciones débiles después de evaluarlas;
- `maxPathDistance`: elimina detours largos después de evaluarlos;
- `maxIncidenceAngleDeg`: elimina reflexiones demasiado rasantes.

Estos parámetros son aproximaciones de escalabilidad. Deben reportarse.

## 5. Materiales

Catálogo actual:

| Material | Modelo v1.1 |
|---|---|
| generic | epsilon_r=4, sigma=0.01 S/m constantes |
| concrete | epsilon_r=5.24; sigma=0.0462*f_GHz^0.7822 |
| brick | epsilon_r=3.91; sigma=0.0238*f_GHz^0.1600 |
| plasterboard | epsilon_r=2.73; sigma=0.0085*f_GHz^0.9395 |
| wood | epsilon_r=1.99; sigma=0.0047*f_GHz^1.0718 |
| glass | epsilon_r=6.31; sigma=0.0036*f_GHz^1.3394 |
| metal | aproximación conductora fuerte, sigma=1e7 S/m |

`f_GHz` es la frecuencia en GHz.

La **densidad de masa** no se usa. Para reflexión electromagnética se usan
permitividad y conductividad. Los rangos de frecuencia declarados se verifican
y el CSV marca extrapolación.

OSM:

- `osmDefaultMaterial`: fallback cuando no hay material reconocido;
- `osmForceMaterial=""`: respeta materiales OSM normalizados;
- `osmForceMaterial="concrete"` (u otro): fuerza todas las superficies OSM a
  ese material para una prueba controlada.

## 6. Combinación multipath

- `coherentCombination=true`: suma amplitudes complejas y conserva fase;
- `false`: suma potencias, útil como sensibilidad/diagnóstico.

No deben presentarse como equivalentes.

## 7. Resolución de malla

`gridStepX`, `gridStepY` controlan densidad de receptores virtuales, no densidad
de rayos.

Para el fixture v1.1:

- 20 m -> ~121 receptores;
- 10 m -> 484 receptores;
- 5 m -> ~1849 receptores.

R2 puede ser costoso porque prueba pares ordenados de paredes por receptor.

## 8. Heatmap

`heatmapMetric`:

- `receivedPowerDbm`;
- `pathCount`;
- `rmsDelayNs`.

Los límites de color son solo visualización y no umbrales físicos universales.

## 9. Tolerancia numérica

`numericalTolerance=1e-8` debe mantenerse durante experimentos normales. Solo
modificarlo en estudios explícitos de robustez numérica.

## 10. Experimentos recomendados

A. Incremental: Urban01..Urban05.

B. Polarización: Urban05 vs Urban06.

C. Fase: Urban05 vs Urban07.

D. Frecuencia: Urban05 vs Urban08, reportando extrapolaciones.

E. Malla: Urban11 vs Urban05 vs Urban12.

F. Material: Urban13 vs Urban14.

G. Escalabilidad: Urban05 vs Urban10/Urban15.

En todos los casos cambiar una sola familia de parámetros a la vez.

## v1.6 Thesis Release notes

- D1 now uses **unique geometric vertices**, not independent wall endpoints.
  Shared OSM polygon corners cannot contribute duplicate TX-edge-RX paths.
- `maxPathsPerReceiver` remains a post-trace selection cap; it is not a launched-ray count.
- `maxIncidenceAngleDeg` is measured from wall normal and is a model guard, not a material constant.
- The final thesis map configuration is `Thesis_M10_FULL_10m`.
- The canonical D1 config is `Thesis_D04_NLoS_FULL` with RX=(1800,225) m.
- For 916 MHz, inspect `material_model_in_range`; the frequency experiment is not material validation.
