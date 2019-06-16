#pragma once

namespace ShaderCompiler
{
	extern dxc::DxcDllSupport	DxcDllHelper;
	extern IDxcCompiler*		Compiler;
	extern IDxcLibrary*			Library;

	void Compile(LPCWSTR filename, LPCWSTR entry, IDxcBlob** blob);

}