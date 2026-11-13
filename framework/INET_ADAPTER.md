# Adaptador INET opcional

Requisitos verificados en este workspace:

- OMNeT++ 6.0pre11;
- INET 4.3 compilado en `samples/inet`;
- `libINET.so` disponible.

Ejecución:

```bash
make inet-adapter OMNETPP_ROOT=/home/juan/omnetpp-6.0pre11
```

Puede definirse `INET_ROOT=/ruta/a/inet`. El script genera la biblioteca fuera
del núcleo, ejecuta `InetAdapterScenario` en Cmdenv y luego comprueba los
escalares con `scripts/validate_inet_adapter.py`.

La API distancia/frecuencia devuelve una ganancia lineal, no dB. La prueba
convierte a pérdida mediante `-10 log10(factor)`. Un valor sin camino en el
overload de posiciones se representa como factor cero.

El escenario actual es una prueba de integración, no una simulación de red:
no crea transmisiones, por lo que los contadores de recepción/interferencia de
INET permanecen en cero. No usar esos contadores como resultado científico.
