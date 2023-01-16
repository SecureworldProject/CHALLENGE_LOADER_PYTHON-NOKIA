# Challenge loader Python

## Introducción
Librería que usaremos para cargar los challenges programados en Python en la herramienta DLL_validator.

## Instalación
Descargue o clone el repositorio.
```
git clone https://github.com/SecureworldProject/CHALLENGE_LOADER_PYTHON-NOKIA
```
La carpeta del proyecto debería quedar así:

![folder](https://user-images.githubusercontent.com/9071577/212644466-3a9ef15c-9ff3-4f4a-b52e-c186dc9bb9d2.png)

Ahora ya puede compilar el proyecto. Recordamos, importante hacerlo en modo Realease x64.

## Requisitos
Para hacer funcionar el Challenge_loader de manera correcta, el proyecto debe estar compilado con la misma versión de Python que el DLL_validator. Para ello, descargar los repositorios y compilar ambos en el ordenador desde el que se esté trabajando.

Añadir en las propiedades del proyecto directorios de inclusión adicionales. Abrir el proyecto Challenge_loader_python en Visual Studio, click derecho sobre el proyecto y en **Properties→C/C++→General→Additional include directories**: añadir el directorio donde se encuentra *Python310/include*, según donde se encuentre:

	*- C:\Program Files\Python310\include*
	*- C:\Users\username\AppData\Local\Programs\Python\Python310\include)*

Debe tener instalado Python 3.10 y la librería python310.dll en el directorio x64/Release.
Las posibles rutas de instalación de Python (donde podrá encontrar el archivo python3.dll son):

	*- C:\Program Files\Python310*
	*- C:\Users\username\AppData\Local\Programs\Python\Python310*
	
## Uso
Una vez compilado y ejecutado el proyecto, debe mover la librería challenge_loader_python.dll al directorio *DLL_VALIDATOR-NOKIA\x64\Release* o donde se encuentre el ejecutable DLL_validator.exe).

Para ver un ejemplo de cómo usar la herramienta DLL_validator con la librería Challenge_loader para cargar challenges en Python, lea este [README](https://github.com/SecureworldProject/DLL_VALIDATOR-NOKIA/blob/master/README.md).
