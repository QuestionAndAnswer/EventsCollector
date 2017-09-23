// MathFuncsDll.h
#include <Windows.h>

#ifdef EVENTS_COLLECTOR_EXPORTS
#define EVENTS_COLLECTOR_EXPORTS __declspec(dllexport) 
#else
#define EVENTS_COLLECTOR_EXPORTS __declspec(dllimport) 
#endif

//EVENTS_COLLECTOR_EXPORTS LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
EVENTS_COLLECTOR_EXPORTS int testCall(int a, int b);