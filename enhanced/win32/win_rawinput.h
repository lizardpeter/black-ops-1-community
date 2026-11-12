#pragma once

#include <windows.h>

bool BO1_StartRawMouse();
void BO1_StopRawMouse();
void BO1_SetRawMouseWindow(HWND window);
void BO1_TakeRawMouse(int &dx, int &dy);
