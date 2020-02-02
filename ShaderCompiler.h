#pragma once

namespace ShaderCompiler
{
	extern IDxcCompiler*		Compiler;
	extern IDxcLibrary*			Library;

	struct DxilMinimalHeader
	{
		UINT32 four_cc;
		UINT32 hash_digest[4];
	};

	void Initialise();
	void Compile(LPCWSTR filename, LPCWSTR entry, LPCWSTR profile, IDxcBlob** blob);
	void Destroy();
}