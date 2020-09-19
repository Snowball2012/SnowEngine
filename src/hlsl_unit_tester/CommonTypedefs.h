#pragma once

struct IDxcCompiler3;
struct IDxcIncludeHandler;
struct ID3D12Device;
struct ID3D12RootSignature;
struct ID3D12PipelineState;

namespace Microsoft::WRL
{
	template<typename T>
	class ComPtr;
}

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace ATL
{
	template <class T>
	class CComPtr;
}