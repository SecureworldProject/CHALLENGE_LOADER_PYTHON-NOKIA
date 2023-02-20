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
CRITICAL_SECTION* py_critical_section = NULL;




/////  FUNCTION DEFINITIONS  /////
void getChallengeParameters();
int importModulePython();
extern "C" _declspec(dllexport) void setPyCriticalSection(CRITICAL_SECTION * py_crit_sect);




/////  FUNCTION IMPLEMENTATIONS  /////
int init(struct ChallengeEquivalenceGroup* group_param, struct Challenge* challenge_param) {
	//PyGILState_STATE gstate;
	EnterCriticalSection(py_critical_section);

	int result = 0;
	PyObject* pValue;

	// It is mandatory to fill these global variables
	group = group_param;
	challenge = challenge_param;
	if (group == NULL || challenge == NULL) {
		printf("--- ERROR: group or challenge are NULL\n");
		return ERROR_INVALID_PARAMETER;
	}
	printf("--- Proceding to initialize challenge '%ws'\n", challenge->file_name);

	// Init python shell
	if (Py_IsInitialized()) {
		printf("--- Python ALREADY initialized\n");
	}
	else {
		printf("--- Python NOT YET initialized\n");
		Py_Initialize();
	}
	//gstate = PyGILState_Ensure();


	// Process challenge parameters
	getChallengeParameters();

	if (refresh_time == INT_MAX || refresh_time == 0) {
		setPeriodicExecution(false);
	}

	// Import the specified python module
	result = importModulePython();
	if (result != 0) {
		if (Py_FinalizeEx() < 0) {
			result = -1;
			goto CH_INIT_LEAVE_CRIT_SECTION;
		}
		goto CH_INIT_LEAVE_CRIT_SECTION;
	}

	// Call python challenge init()
	pValue = PyObject_CallOneArg(pFuncInit, pDictArgs);

	// Check the result
	result = PyLong_Check(pValue);
	if (result == 0) {
		Py_XDECREF(pFuncInit);
		Py_DECREF(pModule);
		if (Py_FinalizeEx() < 0) {
			result = -1;
			goto CH_INIT_LEAVE_CRIT_SECTION;
		}
		result = -2;
		goto CH_INIT_LEAVE_CRIT_SECTION;
	}
	result = PyLong_AsLong(pValue);
	if (result != 0) {
		printf("--- ERROR: result is NOT zero in python challenge init()!\n");
		Py_XDECREF(pFuncInit);
		Py_DECREF(pModule);
		if (Py_FinalizeEx() < 0) {
			result = -1;
			goto CH_INIT_LEAVE_CRIT_SECTION;
		}
		result = -3;
		goto CH_INIT_LEAVE_CRIT_SECTION;
	}
	printf("--- Result IS zero in python challenge init(): OK\n");
	// It is optional to execute the challenge here.
	// As long as this is a wrapper for many python challenges, better not to call it here.
	// Note that calling it from python init will not renew the key. That happens in the C part of the executeChallenge() function
	// result = executeChallenge();

	// It is optional to launch a thread to refresh the key here, but it is recommended
	if (result == 0) {
		launchPeriodicExecution();  // This function is located at context_challenge.h
	}

	CH_INIT_LEAVE_CRIT_SECTION:
	//PyGILState_Release(gstate);
	LeaveCriticalSection(py_critical_section);
	if (result != 0) {
		printf("--- Python may have been FINALIZED\n");
	}

	return result;
}

int executeChallenge() {
	//PyGILState_STATE gstate;
	printf("---****** exec pre PyGILState_Ensure()\n");
	//gstate = PyGILState_Ensure();
	EnterCriticalSection(py_critical_section);
	printf("---****** exec post PyGILState_Ensure()\n");

	int result = 0;
	PyObject* pValue = NULL;
	PyObject* pValuekey = NULL;
	PyObject* pValuekeysize = NULL;
	int size_of_key = 0;
	byte* key_data = NULL;

	if (group == NULL || challenge == NULL) {
		printf("--- ERROR: group or challenge are NULL\n");
		result = ERROR_INVALID_PARAMETER;
		goto CH_EXEC_LEAVE_CRIT_SECTION;
	}
	printf("--- Execute (%ws with module %s)\n", challenge->file_name, module_python);
	printf("--- Python %s initialized\n", (Py_IsInitialized()) ? "IS" : "is NOT");

	pValue = PyObject_CallNoArgs(pFuncExec);
	printf("---****** After PyObject_CallNoArgs()\n");
	result = PyTuple_Check(pValue);
	printf("---****** After PyObject_CallNoArgs()\n");
	if (result == 0) {
		printf("--- ERROR: result is not a tuple! \n");
		setPeriodicExecution(false); // This is enough to ensure that the thread dies and does not execute any other time.
		result = -2;
		goto CH_EXEC_LEAVE_CRIT_SECTION;
	}
	printf("---****** After if (result == 0)\n");
	pValuekey = PyTuple_GetItem(pValue, 0);
	printf("---****** After PyTuple_GetItem(pValue, 0)\n");
	pValuekeysize = PyTuple_GetItem(pValue, 1);
	printf("---****** After PyTuple_GetItem(pValue, 1)\n");

	//TO DO: robustecer esto para que la tupla sepamos que mide 2 y que sus tipos estan bien

	size_of_key = (int) PyLong_AsLong(pValuekeysize);
	printf("---****** After PyLong_AsLong()\n");
	if (size_of_key == 0) {
		// This indicates that the challenge could not execute correctly
		printf("--- ERROR: size_of_key is 0. It was NOT possible to execute the challenge '%s'\n", module_python);
		printf("--- Stopping thread %s. Securemirror will automatically try to launch next equivalent challenge\n", module_python);
		setPeriodicExecution(false); // This is enough to ensure that the thread dies and does not execute any other time.
		result = -3;
		goto CH_EXEC_LEAVE_CRIT_SECTION;
	}
	printf("---****** After if (size_of_key == 0)\n");
	key_data = (byte*)PyBytes_AsString(pValuekey);
	printf("--- After execute: key_data[0] = %d\n", key_data[0]);


	// BEGIN CRITICAL
	// --------------
	EnterCriticalSection(&(group->subkey->critical_section));
	printf("--- Entered in critical section\n");
	/* TO DO: esto no sabemos si debemos descomentarlo o no. pendiente experimentar
	if ((group->subkey)->data != NULL) {
		printf("--- free memory \n");
		free((group->subkey)->data);
	}*/

	group->subkey->data = key_data;
	group->subkey->expires = time(NULL) + validity_time;
	group->subkey->size = size_of_key;
	LeaveCriticalSection(&(group->subkey->critical_section));
	// LEAVE CRITICAL
	// --------------
	printf("--- Exited from critical section\n");

	setPeriodicExecution(true); // As long as the thread is still sleeping, it is enough to set this to true to keep it alive

	CH_EXEC_LEAVE_CRIT_SECTION:
	//PyGILState_Release(gstate);
	LeaveCriticalSection(py_critical_section);
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
		return ERROR_FILE_NOT_FOUND;
	}
	printf("--- Import OK\n");

	pFuncInit = PyObject_GetAttrString(pModule, "init"); // pFunc is a new reference, the attribute(function) execute from module pModule
	if (!(pFuncInit && PyCallable_Check(pFuncInit))) { //PyCallable_Check check if its a function (its callable)
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "--- ERROR: cannot find function init()\n");
		//Py_DECREF(pFuncInit);
		return ERROR_FUNCTION_NOT_CALLED;
	}
	printf("--- Function init() is callable\n");

	pFuncExec = PyObject_GetAttrString(pModule, "executeChallenge"); // pFunc is a new reference, the attribute(function) execute from module pModule
	if (!(pFuncExec && PyCallable_Check(pFuncExec))) { //PyCallable_Check check if its a function (its callable)
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "--- ERROR: cannot find function executeChallenge()\n");
		//Py_DECREF(pFuncExec);
		return ERROR_FUNCTION_NOT_CALLED;
	}
	printf("--- Function executeChallenge() is callable\n");

	return 0;
}

void setPyCriticalSection(CRITICAL_SECTION* py_crit_sect) {
	py_critical_section = py_crit_sect;
}
