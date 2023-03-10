#ifndef CONTEXT_CHALLENGE_H
#define CONTEXT_CHALLENGE_H


/////  FILE INCLUDES  /////

#include <Windows.h>
#include <time.h>
#include <stdio.h>
#include "json.h"




/// /////  GLOBAL VARIABLES  /////

struct ChallengeEquivalenceGroup* group = NULL;
struct Challenge* challenge = NULL;
bool periodic_execution = true;
int validity_time = 0;
int refresh_time = INT_MAX;
HANDLE h_thread = INVALID_HANDLE_VALUE;




/////////  FUNCTIONS  /////

extern "C" _declspec(dllexport) int init(struct ChallengeEquivalenceGroup* group_param, struct Challenge* challenge_param);
extern "C" _declspec(dllexport) int executeChallenge();
extern "C" _declspec(dllexport) void setPeriodicExecution(bool active_param);
void launchPeriodicExecution();
void refreshSubkey(LPVOID th_param);




/////  STRUCTS AND ENUMS  /////

typedef unsigned char byte;

struct ChallengeEquivalenceGroup {
	char* id;
	struct KeyData* subkey;	// Not obtained from json
	struct Challenge** challenges;
};

struct Challenge {
	WCHAR* file_name;
	HINSTANCE lib_handle;
	json_value* properties;	// Properties as a json_value
};

struct KeyData {
	byte* data;
	int size;
	time_t expires;
	CRITICAL_SECTION critical_section;
};


void setPeriodicExecution(bool active_param) {
	printf("--- Setting periodic_execution: %s \n", active_param ? "true" : "false");
	periodic_execution = active_param;
}

void launchPeriodicExecution() {
	if (periodic_execution && h_thread==NULL) {
		h_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)refreshSubkey, NULL, 0, NULL);
	}
}

void refreshSubkey(LPVOID th_param) {
	Sleep(refresh_time*1000);
	
	while (periodic_execution) {
		printf("--- Periodic challenge execution for key refreshing...\n");
		executeChallenge();
		Sleep(refresh_time*1000);      // Sleep first due to execute function already launched by init()
	}
	h_thread = NULL;
	return;
}

#endif // !CONTEXT_CHALLENGE_H