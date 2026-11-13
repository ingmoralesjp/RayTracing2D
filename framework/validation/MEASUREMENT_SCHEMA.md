# Esquema de mediciones v1.4

CSV obligatorio:

- `id`: identificador único y estable;
- `x_m`, `y_m`: coordenadas locales en el mismo CRS/origen del experimento;
- `measured_pr_dbm`: potencia medida, ya corregida conforme al protocolo;
- `uncertainty_db`: incertidumbre expandida o estándar, cuya definición debe
  indicarse en los metadatos de campaña;
- `set`: `calibration` o `test` (otros nombres son aceptados, pero deben
  documentarse).

El convenio de residual es `predicción - medición`: sesgo positivo significa
que el modelo sobreestima potencia. `synthetic_measurements.csv` no es evidencia
de campo; contiene residuos conocidos para verificar el software.

Para una campaña real deben archivarse además fecha/hora, frecuencia, potencia
TX conducida/EIRP, antenas y alturas, orientación, instrumento, ancho de banda,
promediado, coordenadas originales, clima/ocupación si aplica, versión del OSM,
transformación de coordenadas y protocolo de exclusión de muestras.
