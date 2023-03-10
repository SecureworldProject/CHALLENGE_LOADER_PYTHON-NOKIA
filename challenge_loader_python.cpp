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
PyObject* pDictArgs = NULL;
PyObject* pModule = NULL;
PyObject* pFuncInit = NULL;
PyObject* pFuncExec = NULL;
CRITICAL_SECTION* py_critical_section = NULL;




/////  FUNCTION DEFINITIONS  /////
void getChallengeParameters();
int importModulePython();
extern "C" _declspec(dllexport) void setPyCriticalSection(CRITICAL_SECTION * py_crit_sect);




/////  FUNCTION IMPLEMENTATIONS  /////
int init(struct ChallengeEquivalenceGroup* group_param, struct Challenge* challenge_param) {
	int result = 0;
	PyObject* pValue = NULL;

	if (group_param == NULL || challenge_param == NULL) {
		fprintf(stderr, "--- ERROR: group or challenge are NULL in challenge_loader_python init()\n");
		return ERROR_INVALID_PARAMETER;
	}

	printf("--- Proceding to initialize challenge '%ws'\n", challenge_param->file_name);
	EnterCriticalSection(py_critical_section);

	// It is mandatory to fill these global variables
	group = group_param;
	challenge = challenge_param;

	// Initialize python shell if it has not been already
	Py_Initialize();

	// Process challenge parameters
	getChallengeParameters();

	// Do not activate periodic execution in these specific cases
	if (refresh_time == INT_MAX || refresh_time == 0) {
		setPeriodicExecution(false);
	}

	// Import the specified python module
	result = importModulePython();
	if (result != 0) {
		fprintf(stderr, "--- ERROR: could not import module or access one of its functions\n");
		goto CH_INIT_LEAVE_CRIT_SECTION;
	}

	// Call python challenge init()
	pValue = PyObject_CallOneArg(pFuncInit, pDictArgs);

	// Check the result
	result = PyLong_Check(pValue);
	if (result == 0) {
		fprintf(stderr, "--- ERROR: python challenge init() did not return a valid (long type) value\n");
		result = -1;
		goto CH_INIT_LEAVE_CRIT_SECTION;
	}
	result = PyLong_AsLong(pValue);
	if (result != 0) {
		fprintf(stderr, "--- ERROR: python challenge init() did not return zero\n");
		result = -2;
		goto CH_INIT_LEAVE_CRIT_SECTION;
	}
	printf("--- Python challenge init() returned zero: OK\n");
	// It is optional to execute the challenge here.
	// As long as this is a wrapper for many python challenges, better not to call it here.
	// Note that calling it from python init will not renew the key. That happens in the C part of the executeChallenge() function
	// result = executeChallenge();

	// It is optional to launch a thread to refresh the key here, but it is recommended
	if (result == 0 && h_thread == NULL) {
		launchPeriodicExecution();  // This function is located at context_challenge.h
	}

	CH_INIT_LEAVE_CRIT_SECTION:
	if (result != 0) {
		printf("--- Stopping thread associated to %s.py. Securemirror will automatically try to launch next equivalent challenge in the group\n", module_python);
		setPeriodicExecution(false);
		Py_XDECREF(pValue);
		Py_XDECREF(pDictArgs);
		Py_XDECREF(pModule);
		Py_XDECREF(pFuncInit);
		Py_XDECREF(pFuncExec);
	}
	LeaveCriticalSection(py_critical_section);

	return result;
}

int executeChallenge() {
	int result = 0;
	PyObject* pValue = NULL;
	PyObject* pValueKey = NULL;
	PyObject* pValueKeySize = NULL;
	int size_of_key = 0;
	byte* key_data_tmp = NULL;
	byte* key_data = NULL;

	if (group == NULL || challenge == NULL) {
		fprintf(stderr, "--- ERROR: group or challenge are NULL in challenge_loader_python executeChallenge()\n");
		result = ERROR_INVALID_PARAMETER;
		goto CH_EXEC_LEAVE_CRIT_SECTION;
	}
	printf("--- Proceding to execute challenge '%ws' (with module '%s')\n", challenge->file_name, module_python);
	EnterCriticalSection(py_critical_section);

	pValue = PyObject_CallNoArgs(pFuncExec);
	result = PyTuple_Check(pValue);
	if (result == 0) {
		fprintf(stderr, "--- ERROR: python challenge executeChallenge() did not return a valid value (result is not a tuple)\n");
		setPeriodicExecution(false);    // This is enough to ensure that the thread dies and does not execute any other time.
		result = -2;
		goto CH_EXEC_LEAVE_CRIT_SECTION;
	}
	pValueKey = PyTuple_GetItem(pValue, 0);
	pValueKeySize = PyTuple_GetItem(pValue, 1);

	//TO DO: robustecer esto para que la tupla sepamos que mide 2 y que sus tipos estan bien

	size_of_key = (int) PyLong_AsLong(pValueKeySize);
	if (size_of_key == 0) {
		fprintf(stderr, "--- ERROR: python challenge executeChallenge() did not return a valid value (size_of_key is 0)\n");
		setPeriodicExecution(false);    // This is enough to ensure that the thread dies and does not execute any other time.
		result = -3;
		goto CH_EXEC_LEAVE_CRIT_SECTION;
	}

	printf("--- size_of_key is %d\n", size_of_key);
	key_data_tmp = (byte*)PyBytes_AsString(pValueKey);
	printf("--- key_data_tmp is %p: %02X...\n", key_data_tmp, key_data_tmp[0]);
	key_data = (byte*)malloc(size_of_key * sizeof(byte));
	printf("--- key_data (%p) allocated\n", key_data);
	if (key_data == NULL) {
		fprintf(stderr, "--- ERROR: not enough memory\n");
		result = ERROR_NOT_ENOUGH_MEMORY;
		goto CH_EXEC_LEAVE_CRIT_SECTION;
	}
	memcpy_s(key_data, size_of_key, key_data_tmp, size_of_key);
	printf("--- After execute: size_of_key = %d, key_data = ", size_of_key);
	for (size_t i = 0; i < size_of_key; i++) {
		printf("%02X ", (unsigned char)key_data[i]);
	}
	printf("\n");


	printf("--- Entering in key critical section\n");
	EnterCriticalSection(&(group->subkey->critical_section));
	if ((group->subkey)->data != NULL) {
		printf("--- Freeing old key data from (group->subkey)->data \n");
		free((group->subkey)->data);
	}
	group->subkey->data = key_data;
	group->subkey->expires = time(NULL) + validity_time;
	group->subkey->size = size_of_key;
	LeaveCriticalSection(&(group->subkey->critical_section));
	printf("--- Exited from key critical section\n");


	setPeriodicExecution(true); // As long as the thread is still sleeping, it is enough to set this to true to keep it alive

	result = 0;

	CH_EXEC_LEAVE_CRIT_SECTION:
	LeaveCriticalSection(py_critical_section);
	if (result != 0) {
		printf("--- Stopping thread associated to %s.py. Securemirror will automatically try to launch next equivalent challenge in the group\n", module_python);
		setPeriodicExecution(false);
		Py_XDECREF(pValue);
		Py_XDECREF(pValueKey);
		Py_XDECREF(pValueKeySize);
		Py_XDECREF(pDictArgs);
		Py_XDECREF(pModule);
		Py_XDECREF(pFuncInit);
		Py_XDECREF(pFuncExec);
	}


	return result;

}

void getChallengeParameters() {
	printf("--- Getting challenge parameters...\n");

	// General parameters are stored in their own global variables
	// Create a python dict to contain the challenge-specific parameters
	pDictArgs = PyDict_New();

	json_value* value = challenge->properties;
	for (size_t i = 0; i < value->u.object.length; i++) {
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
	PyObject* pName = NULL;

	pName = PyUnicode_DecodeFSDefault(module_python);
	pModule = PyImport_Import(pName);
	Py_XDECREF(pName);
	if (pModule == NULL) {
		PyErr_Print();
		fprintf(stderr, "--- ERROR: failed to load python module\n");
		return ERROR_FILE_NOT_FOUND;
	}
	printf("--- Module import OK\n");

	pFuncInit = PyObject_GetAttrString(pModule, "init"); // pFunc is a new reference, the attribute(function) execute from module pModule
	if (!(pFuncInit && PyCallable_Check(pFuncInit))) { //PyCallable_Check check if its a function (its callable)
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "--- ERROR: cannot find function init()\n");
		return ERROR_FUNCTION_NOT_CALLED;
	}
	printf("--- Function init() is callable\n");

	pFuncExec = PyObject_GetAttrString(pModule, "executeChallenge"); // pFunc is a new reference, the attribute(function) execute from module pModule
	if (!(pFuncExec && PyCallable_Check(pFuncExec))) { //PyCallable_Check check if its a function (its callable)
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "--- ERROR: cannot find function executeChallenge()\n");
		return ERROR_FUNCTION_NOT_CALLED;
	}
	printf("--- Function executeChallenge() is callable\n");

	return 0;
}

void setPyCriticalSection(CRITICAL_SECTION* py_crit_sect) {
	py_critical_section = py_crit_sect;
}
