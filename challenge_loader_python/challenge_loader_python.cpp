// challenge_loader_python.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"


/////  FILE INCLUDES  /////
#include "pch.h"
#include "context_challenge.h"


/////  DEFINITIONS  /////


/////  GLOBAL VARIABLES  /////
char* file_modulepy = NULL;
char* param1 = NULL;
int param2 = 0;


/////  FUNCTION DEFINITIONS  /////
void getChallengeParameters();


/////  FUNCTION IMPLEMENTATIONS  /////
int init(struct ChallengeEquivalenceGroup* group_param, struct Challenge* challenge_param) {
	int result = 0;

	// It is mandatory to fill these global variables
	group = group_param;
	challenge = challenge_param;
	if (group == NULL || challenge == NULL)
		return -1;

	// Process challenge parameters
	getChallengeParameters();

	printf("Initializing (%ws)\n", challenge->file_name);

	// It is optional to execute the challenge here
	result = executeChallenge();

	// It is optional to launch a thread to refresh the key here, but it is recommended
	if (result == 0) {
		periodicExecution(periodic_execution);
	}

	return result;

	

}

int executeChallenge() {

	//TODO leer solo los parameros especificos

	printf("Execute (%ws)\n", challenge->file_name);
	if (group == NULL || challenge == NULL)	return -1;


	//Los pyobjects necesrios para cargar la funcion
	PyObject* pName, *pModule, *pFunc;
	PyObject* pArgs, *pValue;
	PyObject* pDictArgs;
	byte* result;

	//init python and import modules
	Py_Initialize();
	// Preparar el nombre del modulo python y importarlo
	pName = PyUnicode_DecodeFSDefault(file);
	pModule = PyImport_Import(pName);
	Py_DECREF(pName);
	if (pModule == NULL) {
		PyErr_Print();
		fprintf(stderr, "Dll message: Failed to load python module\n");
		Py_DECREF(pModule);
		return 1;
	}
	printf("Initialized module\n");

	//check the python function
	pFunc = PyObject_GetAttrString(pModule, "execute"); // pFunc is a new reference, the attribute(function) execute from module pModule
	if (!(pFunc && PyCallable_Check(pFunc))) { //PyCallable_Check check if its a function (its callable)
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "Dll message: Cannot find function execute\n");
		Py_DECREF(pFunc);
		return 1;
	}
	printf("Function is callable\n");

	//get the arguments and call the function
	num_main_fields = num_main_fields - 3; //Los campos del json menos el modulo, y los tiempos
	pArgs = PyTuple_New(1); // El argumento sera una tupla de un elemento
	pDictArgs = PyDict_New(); // Ese elemento de la tupla sera un diccionario
	for (int i = 0; i < num_main_fields; i++) { //Rellenamos el diccionario con la estructura parameters que obtuvimos en el init
		//TODO: check type of parameter by name
		PyDict_SetItemString(pDictArgs, parameters[i]->name, PyUnicode_FromString((char*)parameters[i]->value));
	}
	PyTuple_SetItem(pArgs, 0, pDictArgs); //Asignamos el diccionario como parametro 0 de la tupla que se llama pArgs
	pValue = PyObject_CallObject(pFunc, pArgs); // pValue es lo que devuelve la invocacion de la funcion
	Py_DECREF(pArgs);
	if (pValue != NULL) {
		result = (byte*)PyBytes_AsString(pValue);
		/*TODO: Ahora mismo la funcion solo devuelve una cadena de bytes, esto debe cambiar y devolver una tupla con los bytes y el tamaño de los
		mismos. Entonces hara que tratar pvalue como una tupla y hacer getitem en dos variables que pueden ser result_bytes y result_size,
		con ese size se hace malloc de group->subkey->data, en lugar de haberlo hecho "desde fuera" (en el securemirror)*/

		memcpy(group->subkey->data, result, 4); // Este 4 lo he puesto a fuego, debería ser "result_size", es decir la variable que devuelve python
		printf("Despues del memcpy    ");
		PRINT_HEX(group->subkey->data, 4);
		Py_DECREF(pValue);


		EnterCriticalSection(&(group->subkey->critical_section));
		if ((group->subkey)->data != NULL) {
			free((group->subkey)->data);
		}
		group->subkey->data = key;
		group->subkey->expires = time(NULL) + validity_time;
		group->subkey->size = size_of_key;
		LeaveCriticalSection(&(group->subkey->critical_section));

	}
	else {
		Py_DECREF(pFunc);
		Py_DECREF(pModule);
		PyErr_Print();
		fprintf(stderr, "Call failed\n");
		return 1;
	}

	Py_XDECREF(pFunc);
	Py_DECREF(pModule);

	if (Py_FinalizeEx() < 0) {
		return 120;
	}

	//group->subkey->expires = time(NULL) + VALID_TIME;
	//group->subkey->data = VALID_KEY;

	return 0;   // Always 0 means OK.

}

void getChallengeParameters() {
	printf("Getting challenge parameters\n");
	json_value* value = challenge->properties;
	for (int i = 0; i < value->u.object.length; i++) {
		if (strcmp(value->u.object.values[i].name, "PyFile") == 0) file_modulepy = value->u.object.values[i].value->u.string.ptr;

		if (strcmp(value->u.object.values[i].name, "validity_time") == 0) {
			validity_time = (int)(value->u.object.values[i].value->u.integer);
		}
		else if (strcmp(value->u.object.values[i].name, "refresh_time") == 0) {
			refresh_time = (int)(value->u.object.values[i].value->u.integer);
		}
		/*
		else if (strcmp(value->u.object.values[i].name, "param1") == 0) {
			param1 = (char*)malloc(value->u.object.values[i].value->u.string.length * sizeof(char) + 1);
			printf("Tamaño = %d\n", value->u.object.values[i].value->u.string.length * sizeof(char) + 1);
			strcpy_s(param1, value->u.object.values[i].value->u.string.length * sizeof(char) + 1, value->u.object.values[i].value->u.string.ptr);
		}
		else if (strcmp(value->u.object.values[i].name, "param2") == 0) {
			param2 = (int)(value->u.object.values[i].value->u.integer);
		}
		*/
		else fprintf(stderr, "WARNING: the field '%s' included in the json configuration file is not registered and will not be processed.\n", value->u.object.values[i].name);

		//TODO: meter los parametros especificos (param1 y param2) en un diccionario python
		//la forma de hacerlo esta puesta en el execute. hay que quitarlo de alli y traerlo aqui
		// NO PORQUE SIN INICIALIZAR PYTHON NO SE PUEDE CREAR EL DICCIONARIO
		//SE INICIALIZA EN EL EXECUTE

	}
}

