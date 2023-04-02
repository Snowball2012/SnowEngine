#pragma once

#include <wrl.h>
#include <WindowsX.h>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;