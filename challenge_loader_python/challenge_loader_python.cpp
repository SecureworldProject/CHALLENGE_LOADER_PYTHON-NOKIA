// challenge_loader_python.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"


/////  FILE INCLUDES  /////
#include "pch.h"
#include "context_challenge.h"
#include <Python.h>
#include "json.h"

/////  DEFINITIONS  /////


/////  GLOBAL VARIABLES  /////
char* module_python = NULL;
PyObject* pDictArgs;
PyObject* pName, *pModule, *pFuncInit, *pFuncExec;

/////  FUNCTION DEFINITIONS  /////
void getChallengeParameters();
int importModulePython();

/////  FUNCTION IMPLEMENTATIONS  /////
int init(struct ChallengeEquivalenceGroup* group_param, struct Challenge* challenge_param) {
	int result = 0;

	
	// It is mandatory to fill these global variables
	group = group_param;
	challenge = challenge_param;
	if (group == NULL || challenge == NULL) {
		printf("\033[33mGroup or challenge are NULL \n \033[0m");
		return -1;
	}
	printf("\033[33mInitializing (%ws) \n \033[0m", challenge->file_name);

	//init python shell
	Py_Initialize();

	// Process challenge parameters
	getChallengeParameters();
	printf("\033[33m get parameters ok\n \033[0m");

	//importamos el modulo
	result= importModulePython();
	if (result != 0) {
		if (Py_FinalizeEx() < 0) {
			return 120;
		}
		return result;//DECREF se hace dentro de la funcion import
	}
	//llamamos al init de python
	PyObject *pValue = PyObject_CallOneArg(pFuncInit, pDictArgs);

	//comprobamos el resultado
	result = PyLong_Check( pValue);
	if (result == 0) {
		Py_XDECREF(pFuncInit);
		Py_DECREF(pModule);
		if (Py_FinalizeEx() < 0) {
			return 120;
		}
		return -1;
	}
	result = PyLong_AsLong(pValue);
	
	if (result != 0) {
		printf("\033[33m error: result no es cero en init de python! \n\033[0m");
		Py_XDECREF(pFuncInit);
		Py_DECREF(pModule);
		if (Py_FinalizeEx() < 0) {
			return 120;
		}
		return result;
	}
	printf("\033[33m result  es cero en init python: ok \n\033[0m");
	// It is optional to execute the challenge here
	// result = executeChallenge(); do it from python init, if you want

	// It is optional to launch a thread to refresh the key here, but it is recommended
	if (result == 0) {
		//this function is located at context_challenge.h
		launchPeriodicExecution();
	}

	return result;

	

}

int executeChallenge() {

	//TODO leer solo los parameros especificos

	printf("--- Execute (%ws)\n", challenge->file_name);
	if (group == NULL || challenge == NULL)	return -1;


	
	byte* result;

	PyObject *pValue = PyObject_CallNoArgs(pFuncExec);
	int res = PyTuple_Check(pValue);
	if (res == 0) {
		printf("--- Result is not a tuple! \n");
		periodic_execution = false;//con esto ya basta para que no se lance mas.
		return -1;
	}
	PyObject *pValuekey=PyTuple_GetItem(pValue, 0);
	PyObject *pValuekeysize = PyTuple_GetItem(pValue, 1);

	//TO DO: robustecer esto para que la tupla sepamos que mide 2 y que sus tipos estan bien

	byte* key_data = (byte*)PyBytes_AsString(pValuekey);
	int size_of_key = (int) PyLong_AsLong(pValuekeysize);

	if (size_of_key == 0) {
		// esto indica que el challenge no se ha podido ejecutar
		printf(" key len zero: NOT possible to execute challenge: %s\n", module_python);
		printf(" Stop thread %s and securemirror go for next equivalent challenge\n", module_python);
		periodic_execution = false;//con esto ya basta para que no se lance mas.
		return - 1; //este -1 no se procesa normalmente a menos que lo invoque directamente securemirror (en caso de clave expirada)
	}

	// BEGIN CRITICAL
	// --------------
	EnterCriticalSection(&(group->subkey->critical_section));
	printf(" --- enter critical section \n");
	/* esto no sabemos si debemos descomentarlo o no. pendiente experimentar
	if ((group->subkey)->data != NULL) {
		printf(" --- free memory \n");
		free((group->subkey)->data);
	}*/
		
	group->subkey->data = key_data;
	group->subkey->expires = time(NULL) + validity_time;
	group->subkey->size = size_of_key;
	LeaveCriticalSection(&(group->subkey->critical_section));
	// LEAVE CRITICAL
	// --------------
	printf(" --- exited from critical section \n");

	periodic_execution = true; //como el hilo sigue en sleep, basta con este true para que siga vivo

	return 0;   // Always 0 means OK.

}

void getChallengeParameters() {
	printf("--- Getting challenge parameters\n");

	
	pDictArgs = PyDict_New(); // Ese elemento de la tupla sera un diccionario

	
	json_value* value = challenge->properties;
	for (int i = 0; i < value->u.object.length; i++) {
		if (strcmp(value->u.object.values[i].name, "module_python") == 0) module_python = value->u.object.values[i].value->u.string.ptr;

		if (strcmp(value->u.object.values[i].name, "validity_time") == 0) {
			validity_time = (int)(value->u.object.values[i].value->u.integer);
		}
		else if (strcmp(value->u.object.values[i].name, "refresh_time") == 0) {
			refresh_time = (int)(value->u.object.values[i].value->u.integer);
		}
		else {
			//parametros especificos del challenge a priori desconocidos
			switch (value->u.object.values[i].value->type) {
			  case  json_integer:
				PyDict_SetItemString(pDictArgs, value->u.object.values[i].name, PyLong_FromLong((long)value->u.object.values[i].value->u.integer));
				break;

			  case json_string:
				char *param = (char*)malloc(value->u.object.values[i].value->u.string.length * sizeof(char) + 1);
				strcpy_s(param, value->u.object.values[i].value->u.string.length * sizeof(char) + 1, value->u.object.values[i].value->u.string.ptr);
				PyDict_SetItemString(pDictArgs, value->u.object.values[i].name, PyUnicode_FromString(param));

				break;
					

			}

		}
		

	}
}

int importModulePython() {
	

	pName = PyUnicode_DecodeFSDefault(module_python);
	pModule = PyImport_Import(pName);
	Py_DECREF(pName);
	if (pModule == NULL) {
		PyErr_Print();
		fprintf(stderr, "--- Dll message: Failed to load python module\n");
		//Py_DECREF(pModule);
		return 1;
	}
	printf("--- Initialized module\n");


	pFuncInit = PyObject_GetAttrString(pModule, "init"); // pFunc is a new reference, the attribute(function) execute from module pModule
	if (!(pFuncInit && PyCallable_Check(pFuncInit))) { //PyCallable_Check check if its a function (its callable)
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "--- Dll message: Cannot find function execute\n");
		//Py_DECREF(pFuncInit);
		return 1;
	}
	printf("--- Function init is callable\n");

	pFuncExec = PyObject_GetAttrString(pModule, "executeChallenge"); // pFunc is a new reference, the attribute(function) execute from module pModule
	if (!(pFuncExec && PyCallable_Check(pFuncExec))) { //PyCallable_Check check if its a function (its callable)
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "--- Dll message: Cannot find function executeChallenge\n");
		//Py_DECREF(pFuncExec);
		return 1;
	}
	printf("--- Function executeChallenge is callable\n");


	return 0;
}
