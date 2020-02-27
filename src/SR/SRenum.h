#pragma once

typedef
enum SRResourceDimension {
	SRResourceDimensionBuffer = 0,
	SRResourceDimensionTexture1D = 1,
	SRResourceDimensionTexture2D = 2,
	SRResourceDimensionTexture3D = 3,
	SRResourceDimensionTextureCube = 4
} SRResourceDimension;

typedef
enum SRClearFlags {
	SRClearFlagDepth = 1,
	SRClearFlagStencil = 2,
	SRClearFlagDepthStencil = 3
} SRClearFlags;

typedef
enum SRBlend {
	SRBlendZero = 1,
	SRBlendOne = 2,
	SRBlendSrcColor = 3,
	SRBlendInvSrcColor = 4,
	SRBlendSrcAlpha = 5,
	SRBlendInvSrcAlpha = 6,
	SRBlendDestAlpha = 7,
	SRBlendInvDestAlpha = 8,
	SRBlendDestColor = 9,
	SRBlendInvDestColor = 10,
	SRBlendSrcAlphaSat = 11,
	SRBlendBlendFactor = 14,
	SRBlendInvBlendFactor = 15,
} SRBlend;

typedef
enum SRBlendOp {
	SRBlendOpAdd = 1,
	SRBlendOpSubstract = 2,
	SRBlendOpRevSubstract = 3,
	SRBlendOpMin = 4,
	SRBlendOpMax = 5,
} SRBlendOp;

typedef
enum SRPrimitiveTopology
{
	SRPrimitiveTopologyTriangleList = 0,
	SRPrimitiveCount
} SRPrimitiveTopology;