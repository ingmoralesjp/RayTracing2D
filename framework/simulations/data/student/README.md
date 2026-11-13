# Mapa OSM del proyecto del tesista

Coloque aqui el archivo `.osm` utilizado por el proyecto original de cobertura
geografica. El nombre recomendado es:

`zacatecas_student.osm`

Despues ejecute:

```bash
python3 scripts/osm_inspect.py simulations/data/student/zacatecas_student.osm
```

El script reporta limites geograficos, numero de nodos, edificios `way`
cerrados y materiales. Use el centro sugerido como `osmOriginLatDeg` y
`osmOriginLonDeg` en la configuracion `UrbanStudentMapTemplate` de
`simulations/omnetpp.ini`.

## Importante

v1.1 importa edificios representados como `way` cerrados. Las relaciones OSM
`multipolygon`, huecos y geometrias complejas no se procesan todavia. Si el
archivo original contiene esas estructuras, el importador necesitara
extenderse en una version posterior.

No confundir el modelo anterior de la tesis (`FSPL + Nobs*2.5 dB`) con el motor
RayTracing2D. El archivo OSM se reutiliza como **geometria del entorno**, no
como modelo de propagacion.
