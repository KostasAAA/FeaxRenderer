
#include "stdafx.h"
#include "FeaxRenderer.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    FeaxRenderer sample(1280, 720, L"FeaxRenderer");
    return Win32Application::Run(&sample, hInstance, nCmdShow);
}
