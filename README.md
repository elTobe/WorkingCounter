# WorkingCounter

WorkingCounter es una aplicación desarrollada en Qt 5.15.2 y MSVC 2019 que permite a los trabajadores registrar sus horas de entrada y salida utilizando un lector de huellas digitales U.are.U 4500. El programa cuenta las horas trabajadas, guarda horas extra y las acumula hasta completar nuevos días laborables. También indica cuántos días se han trabajado para el pago y puede ejecutarse en la bandeja del sistema para una mayor comodidad, mostrando las entradas y salidas en pantalla completa cuando se detectan en segundo plano. 

![WorkingCounter](doc/img/screenshot1.png)

## Requisitos

- Qt 5.15.2
- MSVC 2019
- Lector de huellas digitales U.are.U 4500
- DigitalPersona SDK

## Instalación

1. Clona este repositorio en tu máquina local.
2. Instala Qt 5.15.2 con el kit MSVC 2019.
3. Compila el proyecto abriendo el archivo `.pro` y ejecutándolo. Si no deseas compilar desde el código fuente, ejecuta `windeployqt` en el archivo `AsistenciaSUPER.exe` ubicado en la ruta `doc/bin/` del repositorio.
4. Instala el setup `VC_redist`, el RTE de Digital Persona ubicado en la carpeta `doc/install_req/`.
5. Coloca las carpetas con archivos .fpt de los trabajadores dentro de la carpeta de trabajadores, usando la utilidad proveída por el SDK.
6. Ejecuta el archivo `AsistenciaSUPER.exe` para iniciar la aplicación, se recomienda que esta inicie con Windows, para estar de fondo lista para los registros en cualquier momento.

## Uso

Para utilizar WorkingCounter:

- Al abrir `Asistencia.exe` este se ejecutará en la bandeja del sistema y estará listo para registrar las entradas y salidas de los trabajadores.
- Cuando un trabajador coloque su dedo en el lector de huellas digitales y sea reconocido, aparecerá un mensaje en pantalla completa indicando la entrada. Para salir, el trabajador puede pasar su dedo nuevamente.
- El administrador puede ver el resumen semanal seleccionando la fecha en los widgets de calendario y al trabajador en la lista desplegable de trabajadores.
- Protegido por la contraseña del archivo "pass", el administrador puede realizar acciones adicionales como añadir entradas o salidas a horas arbitrarias, quitar registros, abrir la carpeta raíz del programa, etc.

El programa por defecto considera jornadas laborales de 8.5 horas, y Domingo como primer dia de la semana, para cambiar esto debe hacerse desde el código fuente.

## Contribuciones

Las contribuciones son bienvenidas. Si deseas mejorar o agregar características a este proyecto, siéntete libre de hacer un fork del repositorio y enviar un pull request con tus cambios.

## Contacto y Dudas

Si tienes alguna pregunta o duda sobre este proyecto, no dudes en ponerte en contacto enviando un correo electrónico a perrusquia832@gmail.com.

## Licencia

Este proyecto está bajo la Licencia MIT.
