cbuffer cbConstants : register(b0) {
	uint gMagLevel;
	uint2 gScreenSize;
	bool gIsHorizontal;
};

Texture2D gOriginTex : register(t0);



static const float2 gScreenCoord[6] = {
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

struct VertexOut {
	float4 PosH : SV_POSITION;
	float2 PosS : TEXCOORD;
};

VertexOut TextureVS(uint vID : SV_VertexID) {
	VertexOut vOut;
	vOut.PosS = gScreenCoord[vID];
	vOut.PosH = float4(2.0f * vOut.PosS.x - 1.0f, 1.0f - 2.0f * vOut.PosS.y, 1.0f, 1.0f);
	return vOut;
}

float4 TexturePS(VertexOut pIn) : SV_Target {
	return gOriginTex[trunc(pIn.PosH.xy) / gMagLevel];
}

float4 LineVS(uint vID : SV_VertexID, uint insID : SV_InstanceID) : SV_POSITION{
	if (gIsHorizontal)
		return float4(vID * 2.0f - 1.0f, 1.0f - ((insID * 8 * gMagLevel + 0.5f) / gScreenSize.y) * 2.0f, 1.0f, 1.0f);
	else
		return float4(2.0f * ((insID * 8 * gMagLevel + 0.5f) / gScreenSize.x) - 1.0f, 1.0f - vID * 2.0f, 1.0f, 1.0f);
}

float4 LinePS(float4 posH : SV_POSITION) : SV_Target{
	return 1.0f;
}