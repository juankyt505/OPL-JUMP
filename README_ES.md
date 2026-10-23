# OPL JUMP — FILTRO INTELIGENTE

Una modificación comunitaria de Open PS2 Loader enfocada en mejorar la organización del apartado **APPS de OPL**, mostrando de forma inteligente las aplicaciones habituales y los accesos directos de juegos de PlayStation 1 mediante el emulador **POPStarter**, conservando la interfaz original de OPL.

## Guía rápida

### JUMP — Accesos directos de PlayStation 1
- Pulsa **X** para ejecutar el acceso directo seleccionado.
- Pulsa **Triángulo** para actualizar la lista.
- Pulsa **START** para regresar.

### Filtro inteligente de APPS
- Dentro de APPS, pulsa **L3** para alternar entre las aplicaciones habituales y los accesos directos reconocidos de PS1.
- **L3 desactivado:** muestra las aplicaciones habituales y oculta los accesos directos de PS1 reconocidos.
- **L3 activado:** muestra exclusivamente los accesos directos de PS1 reconocidos.

### Reconocimiento de accesos directos
Se reconocen mediante el prefijo `XX.`, el prefijo `SB.` o un identificador de ejecutable de PS1 en el nombre, por ejemplo `SLUS_123.45.NOMBRE DEL JUEGO.ELF`. Los archivos `.ELF` genéricos no se reconocen automáticamente como accesos directos de PS1.

## OPL JUMP en acción

La siguiente demostración animada muestra el filtro inteligente de APPS con L3 activado.

![Demostración del filtro inteligente de OPL JUMP](docs/images/opl-jump-demo.gif)

## Base del proyecto

OPL JUMP — FILTRO INTELIGENTE está basado en Open PS2 Loader (OPL), revisión 2267, commit [`4cc74ba`](https://github.com/ps2homebrew/Open-PS2-Loader/commit/4cc74baabb73be9ca43f817474b321cd70fbcc2d).

El autor seleccionó esta revisión como punto de partida por la estabilidad observada durante sus pruebas habituales de OPL.

El proyecto incorpora modificaciones propias, incluida la página JUMP y el filtro inteligente de APPS. OPL JUMP es una modificación comunitaria, no una versión oficial de OPL.

## Características

- **Página JUMP:** organiza los accesos directos de juegos de PlayStation 1 por origen:
  - **BDM:** dispositivos gestionados por BDM, como USB y MX4SIO.
  - **HDD:** disco duro interno de PS2.
  - **RED:** accesos directos disponibles mediante SMB desde una carpeta compartida del PC.
- **Filtro inteligente en APPS:** utiliza el botón **L3** para alternar entre las aplicaciones habituales y los accesos directos reconocidos de PS1.
  - **L3 desactivado:** muestra las aplicaciones habituales y oculta los accesos directos de PS1 reconocidos.
  - **L3 activado:** muestra exclusivamente los accesos directos de PS1 reconocidos.
- **Tres criterios de reconocimiento:** JUMP y el filtro inteligente identifican los accesos directos de PS1 mediante:
  1. El prefijo `XX.` en el nombre del archivo.
  2. El prefijo `SB.` en el nombre del archivo.
  3. Un identificador de ejecutable de PS1 en el nombre, por ejemplo `SLUS_123.45.NOMBRE DEL JUEGO.ELF`.
- **Importante:** un archivo `.ELF` genérico no se considera automáticamente un acceso directo de PS1; su clasificación depende de los criterios anteriores.
- **Interfaz familiar:** conserva el estilo habitual de Open PS2 Loader.
- **Múltiples dispositivos:** permite organizar accesos directos procedentes de distintas fuentes de almacenamiento y red.

## Compatibilidad probada

Durante las pruebas de OPL JUMP se comprobó su funcionamiento con:

- **USB:** incluyendo el uso de dos dispositivos USB simultáneamente.
- **HDD interno:** con el formato tradicional utilizado por PS2.
- **SMB1:** incluyendo pruebas con PS2 Server activo, compartiendo una carpeta del PC a través de la red.
- **MX4SIO:** probado para el manejo de los accesos directos.

Estas pruebas corresponden a los dispositivos y configuraciones disponibles durante el desarrollo. No se pretende afirmar que se hayan probado todas las configuraciones posibles.

## Instalación

1. Descarga el archivo `OPNPS2LD.ELF` desde la sección **Releases** de este repositorio.
2. Conserva una copia de seguridad de tu ejecutable de OPL actual.
3. Coloca el archivo en el dispositivo o ubicación desde donde normalmente ejecutas OPL, según tu configuración.
4. Inicia OPL y prueba las funciones JUMP y el filtro del apartado APPS.

**Importante:** utiliza una copia de seguridad de tu configuración actual antes de sustituir cualquier ejecutable.

## Créditos y licencia

OPL JUMP es una modificación comunitaria basada en [Open PS2 Loader](https://github.com/ps2homebrew/Open-PS2-Loader).

El proyecto original acredita a **Ifcaro y jimmikaelkael** y utiliza la licencia **Academic Free License 3.0 (AFL-3.0)**. Se conserva el archivo `LICENSE` incluido en el código base.

Esta modificación se centra en la organización y el manejo de accesos directos de PS1, manteniendo la interfaz habitual de OPL.

Todo el reconocimiento correspondiente al proyecto original y a sus desarrolladores debe mantenerse. **OPL JUMP no es una versión oficial de Open PS2 Loader.**

## Aviso

Este proyecto se comparte con fines comunitarios. El funcionamiento puede variar según la consola, los dispositivos utilizados y la configuración de cada usuario.


## Opinión de la comunidad: posible integración en OPL oficial

¿Te gustaría que la **página JUMP y el filtro inteligente de APPS** se integraran en el proyecto oficial de Open PS2 Loader?

OPL JUMP nació como una modificación comunitaria para explorar estas funciones, conservando la interfaz habitual de OPL. Antes de considerar una propuesta formal al equipo de desarrollo de OPL, me gustaría conocer la opinión de la comunidad.

**¿Apoyarías que se presente una propuesta para integrar estas funciones en el OPL oficial?**

Son bienvenidos tus comentarios, sugerencias e ideas. El interés de la comunidad ayudará a decidir si vale la pena preparar una propuesta formal para que los desarrolladores del proyecto original la evalúen.

*Esta consulta es únicamente comunitaria. Por el momento, no se ha presentado, aprobado ni anunciado ninguna integración oficial.*

## Reconocimientos del desarrollo

**OPL JUMP fue desarrollado mediante un proceso colaborativo que combinó la dirección humana con el apoyo de inteligencia artificial.**

La IA se utilizó como herramienta de apoyo para analizar código, investigar problemas y ayudar en la implementación de cambios. Sin embargo, convertir la idea en una modificación funcional requirió semanas de trabajo práctico, pruebas repetidas, depuración y comprobaciones en una PlayStation 2 real, incluso hasta altas horas de la madrugada.

El proyecto refleja el tiempo, la perseverancia y las pruebas dedicadas a perfeccionar la página JUMP y el filtro inteligente de APPS, procurando conservar la interfaz habitual de OPL y sus funciones existentes.

**La dirección del proyecto, las decisiones de integración, las pruebas en hardware real y la validación final estuvieron a cargo del autor.** La inteligencia artificial contribuyó al proceso de desarrollo, pero el resultado funcional se consiguió mediante supervisión humana continua y pruebas reales.

Este reconocimiento destaca el papel de la IA como herramienta de desarrollo y, al mismo tiempo, el trabajo práctico y la dedicación que hicieron posible OPL JUMP.
