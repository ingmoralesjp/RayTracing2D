# Contrato de coordenadas - RayTracing2D v1.5.1

Este archivo es normativo para futuras versiones. No mezclar el sistema físico
del trazador con el sistema visual de Qtenv.

## 1. OSM geográfico

El archivo `.osm` almacena latitud/longitud en grados. `OsmImporter2D` aplica
una proyección equirectangular local:

```
x = R*cos(lat0)*(lon-lon0)
y = R*(lat-lat0)
```

con diferencias angulares expresadas en radianes.

Contrato del marco local:

- `x` en metros, positivo hacia el este;
- `y` en metros, positivo hacia el norte;
- `(lat0,lon0)` sólo fija el origen local; no es el Canvas de Qtenv.

Para `zacatecas_student.osm` se usa el extremo suroeste del `<bounds>`:

```
osmOriginLatDeg = 22.7569500
osmOriginLonDeg = -102.6002500
```

El edificio soportado ocupa aproximadamente:

```
X = -12.60 .. 3881.60 m
Y = -38.28 .. 2497.72 m
```

El GW de la tesis (`22.761356667994857,-102.58256162532065`) se proyecta
aproximadamente a `(1813.748,490.000) m`.

Importante: el XML contiene nodos auxiliares muy alejados del `<bounds>`; no se
debe tomar el mínimo/máximo de **todos** los nodos como extensión del escenario
de edificios. `scripts/osm_inspect.py` diferencia `<bounds>`, todos los nodos y
los nodos realmente usados por `building`.

## 2. Coordenadas físicas de RayTracing2D / OMNeT++

Los parámetros NED/INI `txX`, `txY`, `rxX`, `rxY`, paredes y malla son números
con unidad `m`. OMNeT++ no interpreta esos valores como latitud/longitud ni los
convierte automáticamente al Canvas. Para el núcleo son simplemente el marco
cartesiano local de propagación.

Toda la física usa exclusivamente este marco:

- distancias;
- visibilidad;
- método de imágenes R1/R2;
- ángulos de incidencia;
- retardos;
- FSPL;
- Fresnel;
- difracción;
- CSV y scalars.

## 3. cCanvas / Qtenv

`cCanvas` usa **canvas units** y Qtenv permite zoom/pan. Esas unidades son de
presentación, no deben asumirse iguales a metros del mundo físico. El fondo
`@display("bgb=1200,800")` define una dimensión visual de referencia, no un área
de 1200 m x 800 m.

v1.5.1 aplica sólo al dibujar:

```
Cx = left + (x - xmin)*scale
Cy = top  + (ymax - y)*scale     # viewNorthUp=true
```

La inversión de Y hace que `+Y=norte` aparezca hacia arriba en pantalla.
`scale` es uniforme en X/Y para no deformar ángulos ni edificios.

El transformado WORLD->CANVAS no modifica ningún cálculo físico.

## 4. Modos de vista

### Auto-fit

```
viewAutoFit = true
```

Los límites se obtienen de paredes + TX/RX + malla activa y se ajustan al
viewport conservando proporción.

### Ventana local

```
viewAutoFit = false
viewWorldMinX = ...m
viewWorldMaxX = ...m
viewWorldMinY = ...m
viewWorldMaxY = ...m
```

Sólo cambia lo que se muestra. No recorta paredes del trazador ni altera los
caminos. La geometría fuera de la ventana se omite únicamente del dibujo.

## 5. Huellas OSM

El importador físico sigue entregando `Wall2D`. Para Qtenv, v1.5.1 reconstruye
las huellas cerradas a partir de `osm_<wayId>_eN` y las dibuja como polígonos.
Esto es una mejora visual; la física continúa usando exactamente los segmentos
`Wall2D` originales.

## 6. Regla de trabajo

**Nunca** corregir una discrepancia visual cambiando las coordenadas físicas del
ray tracer sin comprobar primero el origen OSM y el transformado de Canvas.

Orden de diagnóstico:

1. ejecutar `scripts/osm_inspect.py`;
2. comprobar el origen local declarado en el INI;
3. comprobar `TX/RX` en metros;
4. comprobar los límites `viewWorld*` / `viewAutoFit`;
5. sólo después revisar geometría/propagación.
