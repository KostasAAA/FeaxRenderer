#include "stdafx.h"

#include "dxc/dxcapi.use.h"
#include"ShaderCompiler.h"

namespace ShaderCompiler
{
	dxc::DxcDllSupport		DxcDllHelper;
	IDxcCompiler*			Compiler;
	IDxcLibrary*			Library;

	void Initialise()
	{
		ThrowIfFailed( DxcDllHelper.Initialize() );
		ThrowIfFailed( DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &Compiler) );
		ThrowIfFailed( DxcDllHelper.CreateInstance(CLSID_DxcLibrary, &Library) );
	}

	void Compile(LPCWSTR filename, LPCWSTR entry, IDxcBlob** blob)
	{
		UINT32 codePage(0);
		IDxcBlobEncoding* shaderSource;

		// Load and encode the shader file
		ThrowIfFailed( Library->CreateBlobFromFile(filename, &codePage, &shaderSource) );

		// Create the compiler include handler
		IDxcIncludeHandler* dxcIncludeHandler;
		ThrowIfFailed( Library->CreateIncludeHandler(&dxcIncludeHandler));

		// Compile the shader
		IDxcOperationResult* result;
		ThrowIfFailed( Compiler->Compile(shaderSource, filename, entry, L"lib_6_3", nullptr, 0, nullptr, 0, dxcIncludeHandler, &result));

		// Verify the result
		HRESULT hr;
		result->GetStatus(&hr);
		if (FAILED(hr))
		{
			IDxcBlobEncoding* error;
			ThrowIfFailed( result->GetErrorBuffer(&error) );

			// Convert error blob to a string
			vector<char> infoLog(error->GetBufferSize() + 1);
			memcpy(infoLog.data(), error->GetBufferPointer(), error->GetBufferSize());
			infoLog[error->GetBufferSize()] = 0;

			string errorMsg = "Shader Compiler Error:\n";
			errorMsg.append(infoLog.data());

			MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
			return;
		}

		ThrowIfFailed( result->GetResult(blob) );

		dxcIncludeHandler->Release();
	}
}