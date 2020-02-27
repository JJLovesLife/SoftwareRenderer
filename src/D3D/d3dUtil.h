#pragma once

#include "d3dx12.h"

Microsoft::WRL::ComPtr<ID3D10Blob> CompileShader(
	const std::wstring& filename,
	const D3D_SHADER_MACRO* defines,
	const std::string& entrypoint,
	const std::string& target);