// challenge_loader_python.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"


/////  FILE INCLUDES  /////
#include "pch.h"
#include "context_challenge.h"
#include <Python.h>
#include "json.h"
//#include <stdio.h>





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
		printf("--- ERROR: group or challenge are NULL\n");
		return -1;
	}
	printf("--- Proceding to initialize challenge '%ws'\n", challenge->file_name);

	// Init python shell
	Py_Initialize();

	// Process challenge parameters
	getChallengeParameters();

	// Import the specified python module
	result = importModulePython();
	if (result != 0) {
		if (Py_FinalizeEx() < 0) {
			return 120;
		}
		return result;//DECREF se hace dentro de la funcion import
	}

	// Call python challenge init()
	PyObject *pValue = PyObject_CallOneArg(pFuncInit, pDictArgs);

	// Check the result
	result = PyLong_Check(pValue);
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
		printf("--- ERROR: result is NOT zero in python challenge init()!\n");
		Py_XDECREF(pFuncInit);
		Py_DECREF(pModule);
		if (Py_FinalizeEx() < 0) {
			return 120;
		}
		return result;
	}
	printf("--- Result IS zero in python challenge init(): OK\n");
	// It is optional to execute the challenge here. As long as this is a wrapper for many python challenges, better not to call it here.
	// result = executeChallenge(); // You can do it from python init if you want

	// It is optional to launch a thread to refresh the key here, but it is recommended
	if (result == 0) {
		launchPeriodicExecution();  // This function is located at context_challenge.h
	}

	return result;
}

int executeChallenge() {
	//TODO leer solo los parameros especificos

	if (group == NULL || challenge == NULL) {
		printf("--- ERROR: group or challenge are NULL\n");
		return -1;
	}
	printf("--- Execute (%ws with module %s)\n", challenge->file_name, module_python);

	byte* result;

	PyObject *pValue = PyObject_CallNoArgs(pFuncExec);
	int res = PyTuple_Check(pValue);
	if (res == 0) {
		printf("--- ERROR: result is not a tuple! \n");
		periodic_execution = false; // This is enough to ensure that the thread dies and does not execute any other time.
		return -1;
	}
	PyObject *pValuekey = PyTuple_GetItem(pValue, 0);
	PyObject *pValuekeysize = PyTuple_GetItem(pValue, 1);

	//TO DO: robustecer esto para que la tupla sepamos que mide 2 y que sus tipos estan bien

	int size_of_key = (int) PyLong_AsLong(pValuekeysize);
	if (size_of_key == 0) {
		// This indicates that the challenge could not execute correctly
		printf("--- ERROR: size_of_key is 0. It was NOT possible to execute the challenge '%s'\n", module_python);
		printf("--- Stopping thread %s. Securemirror will automatically try to launch next equivalent challenge\n", module_python);
		periodic_execution = false; // This is enough to ensure that the thread dies and does not execute any other time.
		return - 1; // This -1 is not processed unless it is directly invocated from securemirror (which happens only when the key expired)
	}
	byte* key_data = (byte*)PyBytes_AsString(pValuekey);
	printf("--- After execute: key_data[0] = %d\n", key_data[0]);


	// BEGIN CRITICAL
	// --------------
	EnterCriticalSection(&(group->subkey->critical_section));
	printf(" --- Entered in critical section\n");
	/* TO DO: esto no sabemos si debemos descomentarlo o no. pendiente experimentar
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
	printf(" --- Exited from critical section\n");

	periodic_execution = true; // As long as the thread is still sleeping, it is enough to set this to true to keep it alive

	return 0;   // Always 0 means OK.

}

void getChallengeParameters() {
	printf("--- Getting challenge parameters...\n");

	// General parameters are stored in their own global variables
	// Create a python dict to contain the challenge-specific parameters
	pDictArgs = PyDict_New();

	json_value* value = challenge->properties;
	for (int i = 0; i < value->u.object.length; i++) {
		if (strcmp(value->u.object.values[i].name, "module_python") == 0) {
			module_python = value->u.object.values[i].value->u.string.ptr;
		}

		if (strcmp(value->u.object.values[i].name, "validity_time") == 0) {
			validity_time = (int)(value->u.object.values[i].value->u.integer);
		}
		else if (strcmp(value->u.object.values[i].name, "refresh_time") == 0) {
			refresh_time = (int)(value->u.object.values[i].value->u.integer);
		}
		else {
			// Challenge-specific parameters (supposedly unknown)
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
	printf("--- Challenge parameters obtained\n");
}

int importModulePython() {

	printf("--- Importing module '%s'...\n", module_python);
	pName = PyUnicode_DecodeFSDefault(module_python);
	pModule = PyImport_Import(pName);
	Py_DECREF(pName);
	if (pModule == NULL) {
		PyErr_Print();
		fprintf(stderr, "--- ERROR: failed to load python module\n");
		//Py_DECREF(pModule);
		return 1;
	}
	printf("--- Import OK\n");


	pFuncInit = PyObject_GetAttrString(pModule, "init"); // pFunc is a new reference, the attribute(function) execute from module pModule
	if (!(pFuncInit && PyCallable_Check(pFuncInit))) { //PyCallable_Check check if its a function (its callable)
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "--- ERROR: cannot find function init()\n");
		//Py_DECREF(pFuncInit);
		return 1;
	}
	printf("--- Function init() is callable\n");

	pFuncExec = PyObject_GetAttrString(pModule, "executeChallenge"); // pFunc is a new reference, the attribute(function) execute from module pModule
	if (!(pFuncExec && PyCallable_Check(pFuncExec))) { //PyCallable_Check check if its a function (its callable)
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "--- ERROR: cannot find function executeChallenge()\n");
		//Py_DECREF(pFuncExec);
		return 1;
	}
	printf("--- Function executeChallenge() is callable\n");


	return 0;
}
