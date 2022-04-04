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

	// Process challenge parameters
	getChallengeParameters();


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
		Py_XDECREF(pFuncInit);
		Py_DECREF(pModule);
		if (Py_FinalizeEx() < 0) {
			return 120;
		}
		return result;
	}

	// It is optional to execute the challenge here
	// result = executeChallenge(); do it from python init, if you want

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


	
	byte* result;

	PyObject *pValue = PyObject_CallNoArgs(pFuncExec);
	int res = PyTuple_Check(pValue);
	if (res != 0) return -1;
	PyObject *pValuekey=PyTuple_GetItem(pValue, 0);
	PyObject *pValuekeysize = PyTuple_GetItem(pValue, 1);

	//TO DO: robustecer esto para que la tupla sepamos que mide 2 y que sus tipos estan bien

	byte* key = (byte*)PyBytes_AsString(pValuekey);
	int size_of_key = (int) PyLong_AsLong(pValuekeysize);

	   
	EnterCriticalSection(&(group->subkey->critical_section));
	if ((group->subkey)->data != NULL) {
		free((group->subkey)->data);
	}
		
	group->subkey->data = key;
	group->subkey->expires = time(NULL) + validity_time;
	group->subkey->size = size_of_key;
	LeaveCriticalSection(&(group->subkey->critical_section));

	

	return 0;   // Always 0 means OK.

}

void getChallengeParameters() {
	printf("Getting challenge parameters\n");

	//init python
	Py_Initialize();
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
		fprintf(stderr, "Dll message: Failed to load python module\n");
		Py_DECREF(pModule);
		return 1;
	}
	printf("Initialized module\n");


	pFuncInit = PyObject_GetAttrString(pModule, "init"); // pFunc is a new reference, the attribute(function) execute from module pModule
	if (!(pFuncInit && PyCallable_Check(pFuncInit))) { //PyCallable_Check check if its a function (its callable)
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "Dll message: Cannot find function execute\n");
		Py_DECREF(pFuncInit);
		return 1;
	}
	printf("Function is callable\n");

	pFuncExec = PyObject_GetAttrString(pModule, "executeChallenge"); // pFunc is a new reference, the attribute(function) execute from module pModule
	if (!(pFuncExec && PyCallable_Check(pFuncExec))) { //PyCallable_Check check if its a function (its callable)
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "Dll message: Cannot find function executeChallenge\n");
		Py_DECREF(pFuncExec);
		return 1;
	}
	printf("Function is callable\n");


	return 0;
}
