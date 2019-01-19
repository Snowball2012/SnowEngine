#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>

#include <string>

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = Utils::AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw Utils::DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif

namespace Utils
{
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer );

	// must be multiple of 0x100
	constexpr UINT CalcConstantBufferByteSize( UINT byteSize )
	{
		return ( byteSize + 0xFF ) & ~0xFF;
	}

	Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary( const std::wstring& filename );

	template<typename T>
	class UploadBuffer
	{
	public:
		UploadBuffer( ID3D12Device* device, UINT elementCount, bool isConstantBuffer ):
			mIsConstantBuffer( isConstantBuffer )
		{
			mElementByteSize = sizeof( T );

			// Constant buffer elements need to be multiples of 256 bytes.
			// This is because the hardware can only view constant data 
			// at m*256 byte offsets and of n*256 byte lengths. 
			// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
			// UINT64 OffsetInBytes; // multiple of 256
			// UINT   SizeInBytes;   // multiple of 256
			// } D3D12_CONSTANT_BUFFER_VIEW_DESC;
			if ( isConstantBuffer )
				mElementByteSize = Utils::CalcConstantBufferByteSize( sizeof( T ) );

			ThrowIfFailed( device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer( mElementByteSize*elementCount ),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS( &mUploadBuffer ) ) );

			ThrowIfFailed( mUploadBuffer->Map( 0, nullptr, reinterpret_cast<void**>( &mMappedData ) ) );

			// We do not need to unmap until we are done with the resource.  However, we must not write to
			// the resource while it is in use by the GPU (so we must use synchronization techniques).
		}

		UploadBuffer( const UploadBuffer& rhs ) = delete;
		UploadBuffer& operator=( const UploadBuffer& rhs ) = delete;
		~UploadBuffer()
		{
			if ( mUploadBuffer != nullptr )
				mUploadBuffer->Unmap( 0, nullptr );

			mMappedData = nullptr;
		}

		ID3D12Resource* Resource()const
		{
			return mUploadBuffer.Get();
		}

		void CopyData( int elementIndex, const T& data )
		{
			memcpy( &mMappedData[elementIndex*mElementByteSize], &data, sizeof( T ) );
		}

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
		BYTE* mMappedData = nullptr;

		UINT mElementByteSize = 0;
		bool mIsConstantBuffer = false;
	};

	inline std::array<CD3DX12_STATIC_SAMPLER_DESC, 7> StaticSamplers()
	{
		const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
			0, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP ); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
			1, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP ); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
			2, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP ); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
			3, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP ); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
			4, // shaderRegister
			D3D12_FILTER_ANISOTROPIC, // filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
			0.0f,                             // mipLODBias
			8 );                               // maxAnisotropy

		const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
			5, // shaderRegister
			D3D12_FILTER_ANISOTROPIC, // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
			0.0f,                              // mipLODBias
			8 );                                // maxAnisotropy

		const CD3DX12_STATIC_SAMPLER_DESC shadow(
			6,
			D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,
			0.0f,
			16,
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
		);
		return { pointWrap, pointClamp,
				 linearWrap, linearClamp,
				 anisotropicWrap, anisotropicClamp, shadow };
	}

	// temp
	class DxException
	{
	public:
		DxException() = default;
		DxException( HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber );

		std::wstring ToString()const;

		HRESULT ErrorCode = S_OK;
		std::wstring FunctionName;
		std::wstring Filename;
		int LineNumber = -1;
	};

	inline std::wstring AnsiToWString( const std::string& str )
	{
		WCHAR buffer[512];
		MultiByteToWideChar( CP_ACP, 0, str.c_str(), -1, buffer, 512 );
		return std::wstring( buffer );
	}
}
