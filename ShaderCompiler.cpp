#include "stdafx.h"

//#include "dxc/dxcapi.use.h"
#include"ShaderCompiler.h"

namespace ShaderCompiler
{
	IDxcCompiler*			Compiler;
	IDxcLibrary*			Library;

	void Initialise()
	{
		ThrowIfFailed( DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&Library)) ) ;
		ThrowIfFailed( DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)) );
	}

	inline bool is_dxil_signed(void* buffer)
	{
		DxilMinimalHeader* header = reinterpret_cast<DxilMinimalHeader*>(buffer);
		bool has_digest = false;
		has_digest |= header->hash_digest[0] != 0x0;
		has_digest |= header->hash_digest[1] != 0x0;
		has_digest |= header->hash_digest[2] != 0x0;
		has_digest |= header->hash_digest[3] != 0x0;
		return has_digest;
	}

	void Compile(LPCWSTR filename, LPCWSTR entry, LPCWSTR profile, IDxcBlob** blob)
	{
		*blob = nullptr;

		UINT32 codePage(CP_UTF8);
		IDxcBlobEncoding* shaderSource;

		// Load and encode the shader file
		ThrowIfFailed( Library->CreateBlobFromFile(filename, &codePage, &shaderSource) );

		// Create the compiler include handler
		IDxcIncludeHandler* dxcIncludeHandler;
		ThrowIfFailed( Library->CreateIncludeHandler(&dxcIncludeHandler));

		// Compile the shader
		IDxcOperationResult* result;
		HRESULT hr = Compiler->Compile(shaderSource, filename, entry, profile, nullptr, 0, nullptr, 0, dxcIncludeHandler, &result);

		dxcIncludeHandler->Release();

		// Verify the result
		if (SUCCEEDED(hr))
			result->GetStatus(&hr);

		if (FAILED(hr))
		{
			if (result)
			{
				IDxcBlobEncoding* errorsBlob;
				hr = result->GetErrorBuffer(&errorsBlob);
				if (SUCCEEDED(hr) && errorsBlob)
				{
					wprintf(L"Compilation failed with errors:\n%hs\n", (const char*)errorsBlob->GetBufferPointer());
				}
				errorsBlob->Release();
			}

			//IDxcBlobEncoding* error;
			//ThrowIfFailed( result->GetErrorBuffer(&error) );

			//// Convert error blob to a string
			//vector<char> infoLog(error->GetBufferSize() + 1);
			//memcpy(infoLog.data(), error->GetBufferPointer(), error->GetBufferSize());
			//infoLog[error->GetBufferSize()] = 0;

			//string errorMsg = "Shader Compiler Error:\n";
			//errorMsg.append(infoLog.data());

			//MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
			return;
		}

		ThrowIfFailed( result->GetResult(blob) );

		bool isSigned = is_dxil_signed(blob);

	}

	void Destroy()
	{
		Compiler->Release();
		Library->Release();
	}
}