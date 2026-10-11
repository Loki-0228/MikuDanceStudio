///////////// メインプログラム側から引き渡される変数 /////////////////
float4x4 matLightViewProj;	// 光方向ビュー射影変換済み行列
float4x4 matWorldViewProj;	// ワールドビュー射影変換済み行列
float4x4 matWorld;			// ワールド座標のみ行列
float4x4 matRotate;			// 回転のみ行列
float4x4 matWRotate;		//
float4	 EgColor;			// エッジ色
float4	 ToonColor;			// トゥーン色
float4   LightDir;			// 光方向
float4   SpcColor;			// スペキュラ色
float4   Place;				// カメラの位置
float4   DifColor;			// ディフューズ色
float4   TexCAdd;			// テクスチャパレット加算値
float4   TexCMul;			// テクスチャパレット乗算値
float4   SphCAdd;			// スフィアテクスチャパレット値加算値
float4   SphCMul;			// スフィアテクスチャパレット値乗算値
float4   MatDifColor;
float4   MatAmbColor;
float4   MatEmsColor;
float4   MatSpcColor;
bool	 parthf;			// パースペクティブフラグ
bool	 spadd;				// スフィアマップ加算合成フラグ
bool	 transp;			// 半透明フラグ


///////////// 定数 ////////////////
#define SKII1	1500
#define	SKII2	8000
#define	Toon	3

///////////// 単色塗り潰しテクニック ////////////////

// 頂点シェーダ
float4 ColorRender_VS(float4 Pos : POSITION) : POSITION
{
	return mul( Pos, matWorldViewProj );
}

// ピクセルシェーダ
float4 ColorRender_PS() : COLOR
{
	// EgColor色で塗り潰し
	return float4(EgColor.r,EgColor.g,EgColor.b,EgColor.a);
}

technique ColorRenderTec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;

		VertexShader = compile vs_3_0 ColorRender_VS();
		PixelShader  = compile ps_3_0 ColorRender_PS();
	}
}


///////////// Zバッファプロットテクニック ////////////////

struct VS_ZValuePlot_OUTPUT
{
	float4 Pos : POSITION;				// 射影変換座標
	float4 ShadowMapTex : TEXCOORD0;	// Zバッファテクスチャ
};

// 頂点シェーダ
VS_ZValuePlot_OUTPUT ZValuePlot_VS( float4 Pos : POSITION )
{
	VS_ZValuePlot_OUTPUT Out = (VS_ZValuePlot_OUTPUT)0;

	// ライトの目線によるワールドビュー射影変換をする
	Out.Pos = mul( Pos, matLightViewProj );

	// テクスチャ座標を頂点に合わせる
	Out.ShadowMapTex = Out.Pos;

	return Out;
}


// ピクセルシェーダ
float4 ZValuePlot_PS( float4 ShadowMapTex : TEXCOORD0 ) : COLOR
{
	return float4(ShadowMapTex.z/ShadowMapTex.w,0,0,1);
}

technique ZValuePlotTec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;
		AlphaBlendEnable = FALSE;
	
		VertexShader = compile vs_3_0 ZValuePlot_VS();
		PixelShader  = compile ps_3_0 ZValuePlot_PS();
	}
}

///////////// 分散光なしテクスチャなしバッファシャドウテクニック ////////////////

sampler DefSampler = sampler_state	// サンプラーステート
{
	AddressU  = Clamp;
	AddressV  = Clamp;
	AddressW  = Clamp;
	MINFILTER = POINT;
	MAGFILTER = POINT;
	MIPFILTER = NONE;
};

// Native receiver filtering adapted from MMN's default material shader.
// Filter comparisons, never interpolated depths, to retain thin silhouettes.
float4 MdsShadowMapSize = float4(2048,2048,0,0);

float MdsCompareShadow(float2 texel, float2 uv, float depth,
                      float2 gradient, float bias)
{
    float2 size = max(MdsShadowMapSize.xy, 1);
    // Match the receiver plane to the texel actually fetched at map borders.
    texel = clamp(texel, 0, size - 1);
    // D3D9 rasterizes the projection at integer texels. Texture fetches use
    // texel centres, hence the half-texel offset only on the lookup.
    float receiver = depth + dot(gradient, texel / size - uv);
    float stored = tex2Dlod(DefSampler, float4((texel + 0.5) / size, 0, 0)).r;
    return receiver - bias <= stored ? 1 : 0;
}

float MdsBilinearShadow(float2 sampleUv, float2 uv, float depth,
                       float2 gradient, float bias)
{
    float2 grid = sampleUv * max(MdsShadowMapSize.xy, 1);
    float2 base = floor(grid), weight = frac(grid);
    float a = MdsCompareShadow(base, uv, depth, gradient, bias);
    float b = MdsCompareShadow(base + float2(1,0), uv, depth, gradient, bias);
    float c = MdsCompareShadow(base + float2(0,1), uv, depth, gradient, bias);
    float d = MdsCompareShadow(base + 1, uv, depth, gradient, bias);
    return lerp(lerp(a,b,weight.x), lerp(c,d,weight.x), weight.y);
}

// A cubic B-spline reconstructs magnified shadow texels with continuous
// first and second derivatives. The compact 4x4 comparison kernel smooths
// stair steps without widening the shadow into a large penumbra.
float MdsShadowRow(float2 base, float4 weights, float2 uv,
                   float depth, float2 gradient, float bias)
{
    return weights.x*MdsCompareShadow(base+float2(-1,0),uv,depth,gradient,bias)
         + weights.y*MdsCompareShadow(base,uv,depth,gradient,bias)
         + weights.z*MdsCompareShadow(base+float2(1,0),uv,depth,gradient,bias)
         + weights.w*MdsCompareShadow(base+float2(2,0),uv,depth,gradient,bias);
}
float4 MdsCubicWeights(float f)
{
    float f2=f*f, f3=f2*f;
    return float4(1-3*f+3*f2-f3, 4-6*f2+3*f3,
                  1+3*f+3*f2-3*f3, f3) / 6;
}
float MdsReconstructShadow(float2 uv, float depth, float2 gradient, float bias)
{
    float2 grid = uv * max(MdsShadowMapSize.xy, 1);
    float2 base = floor(grid), f = frac(grid);
    float4 wx = MdsCubicWeights(f.x), wy = MdsCubicWeights(f.y);
    return wy.x*MdsShadowRow(base+float2(0,-1),wx,uv,depth,gradient,bias)
         + wy.y*MdsShadowRow(base,wx,uv,depth,gradient,bias)
         + wy.z*MdsShadowRow(base+float2(0,1),wx,uv,depth,gradient,bias)
         + wy.w*MdsShadowRow(base+float2(0,2),wx,uv,depth,gradient,bias);
}

float MdsShadowVisibility(float3 projected, float2 uv, float3 normal)
{
    // Derivatives must be evaluated before the per-pixel coverage test.
    float2 dx = ddx(uv), dy = ddy(uv);
    float dzdx = ddx(projected.z), dzdy = ddy(projected.z);
    // Leaving the map removes occlusion only. Keep material toon lighting
    // and its specular mask independent of camera-dependent map coverage.
    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1 ||
        projected.z < 0 || projected.z > 1)
        return 1;
    float det = dx.x * dy.y - dx.y * dy.x;
    float2 gradient = 0;
    if (abs(det) > 1e-12)
        gradient = float2(dzdx * dy.y - dzdy * dx.y,
                          dzdy * dx.x - dzdx * dy.x) / det;
    float ndl = saturate(dot(normalize(normal), -LightDir.xyz));
    // Keep the native mode-dependent depth scale; grazing surfaces need more
    // bias, while receiver-plane correction accounts for each tap's offset.
    float threshold = parthf ? max(SKII2 * uv.y, 1) : SKII1;
    float bias = (0.3 / threshold) * max(1, 2 * (1 - ndl));
    float2 size = max(MdsShadowMapSize.xy, 1);
    float footprint = max(length(dx * size), length(dy * size));
    float nearVisibility = 0;
    // LOD 0 comparisons are safe inside this derivative-dependent branch.
    // Blend over 1..2 shadow texels per pixel so camera motion does not switch
    // filters abruptly. Both modes keep the depth scale calculated above.
    [branch] if (footprint < 2)
        nearVisibility = MdsReconstructShadow(uv, projected.z, gradient, bias);
    [branch] if (footprint <= 1)
        return nearVisibility;
    // Retain pixel-footprint integration when the map is minified.
    float farVisibility = 0.25 * (MdsBilinearShadow(uv + 0.25*(dx+dy), uv, projected.z, gradient, bias)
                 +MdsBilinearShadow(uv + 0.25*(dx-dy), uv, projected.z, gradient, bias)
                 +MdsBilinearShadow(uv + 0.25*(-dx+dy), uv, projected.z, gradient, bias)
                 +MdsBilinearShadow(uv - 0.25*(dx+dy), uv, projected.z, gradient, bias));
    return lerp(nearVisibility, farVisibility, smoothstep(1, 2, footprint));
}

struct BufferShadow_OUTPUT
{
	float4 Pos      : POSITION;		// 射影変換座標
	float4 ZCalcTex : TEXCOORD0;	// Z値
	float3 N		: TEXCOORD1;	// 法線
	float3 Eye		: TEXCOORD2;	// カメラとの相対位置
};

// 頂点シェーダ
BufferShadow_OUTPUT BufferShadow_VS(float4 Pos : POSITION, float4 Normal : NORMAL)
{
	BufferShadow_OUTPUT Out = (BufferShadow_OUTPUT)0;

	// 普通にカメラの目線によるワールドビュー射影変換をする
	Out.Pos = mul( Pos, matWorldViewProj );
   
	// ライトの目線によるワールドビュー射影変換をする
	Out.ZCalcTex = mul( Pos, matLightViewProj );
	
	// 法線
	Out.N = Normal.xyz;

	// カメラ相対位置
	Out.Eye = Place - Pos.xyz;
	
	return Out;
}

// ピクセルシェーダ
float4 BufferShadow_PS(float4 ZCalcTex : TEXCOORD0,float3 N : TEXCOORD1,float3 Eye : TEXCOORD2) : COLOR
{
	// スペキュラ色計算
	float4 Specula = pow(max(0,dot(normalize(N),normalize(-LightDir.xyz + normalize(Eye)))),SpcColor.w);
	Specula *= SpcColor;
	Specula.w = 0.0f;
	Specula = saturate(Specula);

	// テクスチャ座標に変換
	ZCalcTex /= ZCalcTex.w;
	float2 TransTexCoord;
	TransTexCoord.x = (1.0f + ZCalcTex.x)*0.5f;
	TransTexCoord.y = (1.0f - ZCalcTex.y)*0.5f;
    float shadowVisibility = MdsShadowVisibility(ZCalcTex.xyz, TransTexCoord, N);
   
	{
		float comp = shadowVisibility;
		comp = min(smoothstep(0,1,saturate(dot(normalize(N),-LightDir)*Toon)),comp);	// 分散光
		if(comp==1.0f){
			float4 ans = EgColor + Specula;
			if(transp) ans.a = 0.5f;
			return ans;
		}else{
			float4 ans = EgColor*comp + EgColor*ToonColor*(1-comp) + Specula*comp;
			if(transp) ans.a = 0.5f;
			else	   ans.a = EgColor.a;
			return ans;			
		}
	}
}

technique BufferShadowTec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;
		VertexShader = compile vs_3_0 BufferShadow_VS();
		PixelShader  = compile ps_3_0 BufferShadow_PS();
	}
}


///////////// 分散光なしテクスチャありバッファシャドウテクニック ////////////////

sampler s1 : register(s1);      //オブジェクトのテクスチャー

struct BShadowTexture_OUTPUT
{
	float4 Pos      : POSITION;		// 射影変換座標
	float4 ZCalcTex : TEXCOORD0;	// Z値
	float2 Tex      : TEXCOORD1;	// テクスチャ
	float3 N		: TEXCOORD2;	// 法線
	float3 Eye		: TEXCOORD3;	// カメラとの相対位置
};

// 頂点シェーダ
BShadowTexture_OUTPUT BShadowTexture_VS(float4 Pos : POSITION, float4 Normal : NORMAL, float2 Tex : TEXCOORD0 )
{
	BShadowTexture_OUTPUT Out = (BShadowTexture_OUTPUT)0;
	
	// テクスチャ座標
	Out.Tex = Tex;

	// 普通にカメラの目線によるワールドビュー射影変換をする
	Out.Pos = mul( Pos, matWorldViewProj );
   
	// ライトの目線によるワールドビュー射影変換をする
	Out.ZCalcTex = mul( Pos, matLightViewProj );

	// 法線
	Out.N = Normal.xyz;

	// カメラ相対位置
	Out.Eye = Place - Pos.xyz;
	
	return Out;
}

// ピクセルシェーダ
float4 BShadowTexture_PS(float4 ZCalcTex : TEXCOORD0,float2 Tex : TEXCOORD1,float3 N : TEXCOORD2,float3 Eye : TEXCOORD3) : COLOR
{
	// スペキュラ色計算
	float4 Specula = pow(max(0,dot(normalize(N),normalize(-LightDir.xyz + normalize(Eye)))),SpcColor.w);
	Specula *= SpcColor;
	Specula.w = 0.0f;
	Specula = saturate(Specula);

   // テクスチャ座標に変換
   ZCalcTex /= ZCalcTex.w;
   float2 TransTexCoord;
   TransTexCoord.x = (1.0f + ZCalcTex.x)*0.5f;
   TransTexCoord.y = (1.0f - ZCalcTex.y)*0.5f;
    float shadowVisibility = MdsShadowVisibility(ZCalcTex.xyz, TransTexCoord, N);
   float alpha = TexCMul.w + TexCAdd.w;

	{
		float comp = shadowVisibility;
		comp = min(smoothstep(0,1,saturate(dot(normalize(N),-LightDir)*Toon)),comp);	// 分散光
		if(comp==1.0f){
			float4 ans = EgColor*((tex2D(s1,Tex) * TexCMul + TexCAdd) * alpha + (1-alpha)) + Specula*comp;
			if(transp) ans.w = 0.5f;
			else ans.w = EgColor.w * tex2D(s1,Tex).w;
			return ans;
		}else{
			float4 tmpColor=EgColor*((tex2D(s1,Tex) * TexCMul + TexCAdd) * alpha + (1-alpha));
			float4 ans = tmpColor*(comp+ToonColor*(1-comp)) + Specula*comp;
			if(transp) ans.w = 0.5f;
			else	   ans.w = EgColor.w * tex2D(s1,Tex).w;
			return ans;
		}
	}
}

technique BShadowTextureTec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;
		VertexShader = compile vs_3_0 BShadowTexture_VS();
		PixelShader  = compile ps_3_0 BShadowTexture_PS();
	}
}


///////////// 分散光なしテクスチャなしスフィアありバッファシャドウテクニック ////////////////

// 頂点シェーダ
BShadowTexture_OUTPUT BShadowSphia_VS(float4 Pos : POSITION, float4 Normal : NORMAL)
{
	BShadowTexture_OUTPUT Out = (BShadowTexture_OUTPUT)0;
	
	// スフィアマップテクスチャ座標
	Out.Tex = mul(Normal, matRotate);

	// 普通にカメラの目線によるワールドビュー射影変換をする
	Out.Pos = mul( Pos, matWorldViewProj );
   
	// ライトの目線によるワールドビュー射影変換をする
	Out.ZCalcTex = mul( Pos, matLightViewProj );

	// 法線
	Out.N = Normal.xyz;

	// カメラ相対位置
	Out.Eye = Place - Pos.xyz;
	
	return Out;
}

// ピクセルシェーダ
float4 BShadowSphia_PS(float4 ZCalcTex : TEXCOORD0,float2 Tex : TEXCOORD1,float3 N : TEXCOORD2,float3 Eye : TEXCOORD3) : COLOR
{
	// スペキュラ色計算
	float4 Specula = pow(max(0,dot(normalize(N),normalize(-LightDir.xyz + normalize(Eye)))),SpcColor.w);
	Specula *= SpcColor;
	Specula.w = 0.0f;
	Specula = saturate(Specula);

	// テクスチャ座標に変換
	ZCalcTex /= ZCalcTex.w;
	float2 TransTexCoord;
	TransTexCoord.x = (1.0f + ZCalcTex.x)*0.5f;
	TransTexCoord.y = (1.0f - ZCalcTex.y)*0.5f;
    float shadowVisibility = MdsShadowVisibility(ZCalcTex.xyz, TransTexCoord, N);
	
	// スフィアマップ座標計算
	Tex.x = Tex.x * 0.5f + 0.5f;
	Tex.y = Tex.y * -0.5f + 0.5f;
	float alpha = SphCMul.w + SphCAdd.w;

	float4 ans;
	if(spadd) ans = EgColor + (tex2D(s1,Tex) * SphCMul + SphCAdd) * alpha;
	else	  ans = EgColor * ((tex2D(s1,Tex) * SphCMul + SphCAdd)*alpha + (1-alpha));
	ans.w = EgColor.w * tex2D(s1,Tex).w;

	{
		float comp = shadowVisibility;
		comp = min(smoothstep(0,1,saturate(dot(normalize(N),-LightDir)*Toon)),comp);	// 分散光
		if(comp==1.0f){
			ans = ans + Specula*comp;
			if(transp) ans.w = 0.5f;
			else	   ans.w = EgColor.w * tex2D(s1,Tex).w;
			return ans;
		}else{
			ans = ans*comp + ans*ToonColor*(1-comp) + Specula*comp;
			if(transp) ans.w = 0.5f;
			else	   ans.w = EgColor.w * tex2D(s1,Tex).w;
			return ans;
		}
	}
}

technique BShadowSphiaTec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;
		VertexShader = compile vs_3_0 BShadowSphia_VS();
		PixelShader  = compile ps_3_0 BShadowSphia_PS();
	}
}


///////////// 分散光なしテクスチャありスフィアマップありバッファシャドウテクニック ////////////////

sampler s2 : register(s2);      //オブジェクトのテクスチャー

struct BShadowSphiaTexture_OUTPUT
{
	float4 Pos      : POSITION;		// 射影変換座標
	float4 ZCalcTex : TEXCOORD0;	// Z値
	float2 Tex      : TEXCOORD1;	// テクスチャ座標
	float2 SpTex	: TEXCOORD2;	// スフィアマップテクスチャ座標
	float3 N		: TEXCOORD3;	// 法線
	float3 Eye		: TEXCOORD4;	// カメラとの相対位置
};

// 頂点シェーダ
BShadowSphiaTexture_OUTPUT BShadowSphiaTexture_VS(float4 Pos : POSITION, float4 Normal : NORMAL, float2 Tex : TEXCOORD0 )
{
	BShadowSphiaTexture_OUTPUT Out = (BShadowSphiaTexture_OUTPUT)0;
	
	// テクスチャ座標
	Out.Tex = Tex;

	// スフィアマップテクスチャ座標
	Out.SpTex = mul(Normal, matRotate);

	// 普通にカメラの目線によるワールドビュー射影変換をする
	Out.Pos = mul( Pos, matWorldViewProj );
   
	// ライトの目線によるワールドビュー射影変換をする
	Out.ZCalcTex = mul( Pos, matLightViewProj );

	// 法線
	Out.N = Normal.xyz;

	// カメラ相対位置
	Out.Eye = Place - Pos.xyz;
	
	return Out;
}

// ピクセルシェーダ
float4 BShadowSphiaTexture_PS(float4 ZCalcTex : TEXCOORD0,float2 Tex : TEXCOORD1,float2 SpTex : TEXCOORD2,float3 N : TEXCOORD3,float3 Eye : TEXCOORD4) : COLOR
{
	// スペキュラ色計算
	float4 Specula = pow(max(0,dot(normalize(N),normalize(-LightDir.xyz + normalize(Eye)))),SpcColor.w);
	Specula *= SpcColor;
	Specula.w = 0.0f;
	Specula = saturate(Specula);

	// テクスチャ座標に変換
	ZCalcTex /= ZCalcTex.w;
	float2 TransTexCoord;
	TransTexCoord.x = (1.0f + ZCalcTex.x)*0.5f;
	TransTexCoord.y = (1.0f - ZCalcTex.y)*0.5f;
    float shadowVisibility = MdsShadowVisibility(ZCalcTex.xyz, TransTexCoord, N);
	float texalp = TexCMul.w + TexCAdd.w;
   
  	// スフィアマップ座標計算
	SpTex.x = SpTex.x * 0.5f + 0.5f;
	SpTex.y = SpTex.y * -0.5f + 0.5f;
	float sphalp = SphCMul.w + SphCAdd.w;

	float4 ans;
	float4 texColor = (tex2D(s1,Tex) * TexCMul + TexCAdd)* texalp + (1-texalp);
	if(spadd) ans = EgColor * texColor + (tex2D(s2,SpTex) * SphCMul + SphCAdd) * sphalp;
	else	  ans = EgColor * texColor * ((tex2D(s2,SpTex) * SphCMul + SphCAdd) * sphalp + (1-sphalp));
	ans.w = EgColor.w * tex2D(s1,Tex).w * tex2D(s2,SpTex).w;
	
	{
		float comp = shadowVisibility;
		comp = min(smoothstep(0,1,saturate(dot(normalize(N),-LightDir)*Toon)),comp);	// 分散光
		if(comp==1.0f){
			ans = ans + Specula*comp;
			if(transp) ans.w = 0.5f;
			else	   ans.w = EgColor.w * tex2D(s1,Tex).w * tex2D(s2,SpTex).w;
			return ans;
		}else{
			ans = ans*(comp+ToonColor*(1-comp)) + Specula*comp;
			if(transp) ans.w = 0.5f;
			else	   ans.w = EgColor.w * tex2D(s1,Tex).w * tex2D(s2,SpTex).w;
			return ans;
		}
	}
}

technique BShadowSphiaTextureTec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;
		VertexShader = compile vs_3_0 BShadowSphiaTexture_VS();
		PixelShader  = compile ps_3_0 BShadowSphiaTexture_PS();
	}
}

///////////// 分散光なしテクスチャなしテクスチャコード２ありバッファシャドウテクニック ////////////////

// 頂点シェーダ
BShadowTexture_OUTPUT BShadowTexCd2_VS(float4 Pos : POSITION, float4 Normal : NORMAL, float2 Tex : TEXCOORD1)
{
	BShadowTexture_OUTPUT Out = (BShadowTexture_OUTPUT)0;
	
	// テクスチャ座標
	Out.Tex = Tex;

	// 普通にカメラの目線によるワールドビュー射影変換をする
	Out.Pos = mul( Pos, matWorldViewProj );
   
	// ライトの目線によるワールドビュー射影変換をする
	Out.ZCalcTex = mul( Pos, matLightViewProj );

	// 法線
	Out.N = Normal.xyz;

	// カメラ相対位置
	Out.Eye = Place - Pos.xyz;
	
	return Out;
}

// ピクセルシェーダ
float4 BShadowTexCd2_PS(float4 ZCalcTex : TEXCOORD0,float2 Tex : TEXCOORD1,float3 N : TEXCOORD2,float3 Eye : TEXCOORD3) : COLOR
{
	// スペキュラ色計算
	float4 Specula = pow(max(0,dot(normalize(N),normalize(-LightDir.xyz + normalize(Eye)))),SpcColor.w);
	Specula *= SpcColor;
	Specula.w = 0.0f;
	Specula = saturate(Specula);

   // テクスチャ座標に変換
   ZCalcTex /= ZCalcTex.w;
   float2 TransTexCoord;
   TransTexCoord.x = (1.0f + ZCalcTex.x)*0.5f;
   TransTexCoord.y = (1.0f - ZCalcTex.y)*0.5f;
    float shadowVisibility = MdsShadowVisibility(ZCalcTex.xyz, TransTexCoord, N);
   float alpha = SphCMul.w + SphCAdd.w;

	{
		float comp = shadowVisibility;
		comp = min(smoothstep(0,1,saturate(dot(normalize(N),-LightDir)*Toon)),comp);	// 分散光
		if(comp==1.0f){
			float4 ans = EgColor*((tex2D(s1,Tex) * SphCMul + SphCAdd)*alpha + (1-alpha)) + Specula*comp;
			if(transp) ans.a = 0.5f;
			else	   ans.w = EgColor.w * tex2D(s1,Tex).w;
			return ans;
		}else{
			float4 tmpColor=EgColor*((tex2D(s1,Tex) * SphCMul + SphCAdd)*alpha + (1-alpha));
			float4 ans = tmpColor*(comp+ToonColor*(1-comp)) + Specula*comp;
			if(transp) ans.a = 0.5f;
			else	   ans.w = EgColor.w * tex2D(s1,Tex).w;
			return ans;
		}
	}
}

technique BShadowTexCd2Tec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;
		VertexShader = compile vs_3_0 BShadowTexCd2_VS();
		PixelShader  = compile ps_3_0 BShadowTexCd2_PS();
	}
}

///////////// 分散光なしテクスチャありテクスチャコード２ありバッファシャドウテクニック ////////////////

// 頂点シェーダ
BShadowSphiaTexture_OUTPUT BShadowTextureTexCd2Tec_VS(float4 Pos : POSITION, float4 Normal : NORMAL, float2 Tex : TEXCOORD0, float2 Tex2 : TEXCOORD1 )
{
	BShadowSphiaTexture_OUTPUT Out = (BShadowSphiaTexture_OUTPUT)0;
	
	// テクスチャ座標
	Out.Tex = Tex;

	// スフィアマップテクスチャ座標
	Out.SpTex = Tex2;

	// 普通にカメラの目線によるワールドビュー射影変換をする
	Out.Pos = mul( Pos, matWorldViewProj );
   
	// ライトの目線によるワールドビュー射影変換をする
	Out.ZCalcTex = mul( Pos, matLightViewProj );

	// 法線
	Out.N = Normal.xyz;

	// カメラ相対位置
	Out.Eye = Place - Pos.xyz;
	
	return Out;
}

// ピクセルシェーダ
float4 BShadowTextureTexCd2Tec_PS(float4 ZCalcTex : TEXCOORD0,float2 Tex : TEXCOORD1,float2 SpTex : TEXCOORD2,float3 N : TEXCOORD3,float3 Eye : TEXCOORD4) : COLOR
{
	// スペキュラ色計算
	float4 Specula = pow(max(0,dot(normalize(N),normalize(-LightDir.xyz + normalize(Eye)))),SpcColor.w);
	Specula *= SpcColor;
	Specula.w = 0.0f;
	Specula = saturate(Specula);

	// テクスチャ座標に変換
	ZCalcTex /= ZCalcTex.w;
	float2 TransTexCoord;
	TransTexCoord.x = (1.0f + ZCalcTex.x)*0.5f;
	TransTexCoord.y = (1.0f - ZCalcTex.y)*0.5f;
    float shadowVisibility = MdsShadowVisibility(ZCalcTex.xyz, TransTexCoord, N);
	float texalp = TexCMul.w + TexCAdd.w;
	float sphalp = SphCMul.w + SphCAdd.w;
   
	float4 ans;
	float4 texColor = (tex2D(s1,Tex) * TexCMul + TexCAdd)*texalp + ( 1-texalp);
	if(spadd) ans = EgColor * texColor + (tex2D(s2,SpTex) * SphCMul + SphCAdd) * sphalp;
	else	  ans = EgColor * texColor * ((tex2D(s2,SpTex) * SphCMul + SphCAdd)*sphalp + (1-sphalp));
	ans.w = EgColor.w * tex2D(s1,Tex).w * tex2D(s2,SpTex).w;
	
	{
		float comp = shadowVisibility;
		comp = min(smoothstep(0,1,saturate(dot(normalize(N),-LightDir)*Toon)),comp);	// 分散光
		if(comp==1.0f){
			ans = ans + Specula*comp;
			if(transp) ans.a = 0.5f;
			else	   EgColor.w * tex2D(s1,Tex).w * tex2D(s2,SpTex).w;
			return ans;
		}else{
			ans = ans*(comp+ToonColor*(1-comp)) + Specula*comp;
			if(transp) ans.a = 0.5f;
			else	   EgColor.w * tex2D(s1,Tex).w * tex2D(s2,SpTex).w;
			return ans;
		}
	}
}

technique BShadowTextureTexCd2Tec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;
		VertexShader = compile vs_3_0 BShadowTextureTexCd2Tec_VS();
		PixelShader  = compile ps_3_0 BShadowTextureTexCd2Tec_PS();
	}
}


///////////// 分散光ありテクスチャなしバッファシャドウテクニック ////////////////

struct DiffuseBufferShadow_OUTPUT
{
	float4 Pos      : POSITION;		// 射影変換座標
	float4 ZCalcTex : TEXCOORD0;	// Z値
	float3 N		: TEXCOORD1;	// 法線
	float3 Eye		: TEXCOORD2;	// カメラとの相対位置
	float4 Color	: COLOR0;		// Diffuse色
};

// 頂点シェーダ
DiffuseBufferShadow_OUTPUT DiffuseBufferShadow_VS(float4 Pos : POSITION, float4 Normal : NORMAL)
{
	DiffuseBufferShadow_OUTPUT Out = (DiffuseBufferShadow_OUTPUT)0;

	// 普通にカメラの目線によるワールドビュー射影変換をする
	Out.Pos = mul( Pos, matWorldViewProj );
   
	// ライトの目線によるワールドビュー射影変換をする
	Out.ZCalcTex = mul( Pos, matLightViewProj );

	// 法線
	Out.N = mul(Normal,matWorld).xyz;

	// カメラ相対位置
	Out.Eye = Place - mul(Pos,matWorld).xyz;
	
	// Diffuse色
	float3 L = -LightDir.xyz;
	Out.Color = saturate(EgColor + max(0,DifColor * dot(normalize(Out.N),L )));
	Out.Color.a = EgColor.a;

	return Out;
}

// ピクセルシェーダ
float4 DiffuseBufferShadow_PS(float4 ZCalcTex : TEXCOORD0,float3 N : TEXCOORD1,float3 Eye : TEXCOORD2,float4 Color : COLOR0) : COLOR
{
	// スペキュラ色計算
	float4 Specula = pow(max(0,dot(normalize(N),normalize(-LightDir.xyz + normalize(Eye)))),SpcColor.w);
	Specula *= SpcColor;
	Specula.w = 0.0f;
	Specula = saturate(Specula);
	
   // テクスチャ座標に変換
   ZCalcTex /= ZCalcTex.w;
   float2 TransTexCoord;
   TransTexCoord.x = (1.0f + ZCalcTex.x)*0.5f;
   TransTexCoord.y = (1.0f - ZCalcTex.y)*0.5f;
    float shadowVisibility = MdsShadowVisibility(ZCalcTex.xyz, TransTexCoord, N);

	{
		float comp = shadowVisibility;
		if(comp==1.0f){
			return Color + Specula;
		}else{
			float4 ans = (Color + Specula)*comp + EgColor*(1-comp);
			ans.a = Color.a;
			return ans;
		}
	}
}

technique DiffuseBufferShadowTec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;
	
		VertexShader = compile vs_3_0 DiffuseBufferShadow_VS();
		PixelShader  = compile ps_3_0 DiffuseBufferShadow_PS();
	}
}


///////////// 分散光ありテクスチャありバッファシャドウテクニック ////////////////

struct DiffuseBSTexture_OUTPUT
{
	float4 Pos      : POSITION;		// 射影変換座標
	float4 ZCalcTex : TEXCOORD0;	// Z値
	float2 Tex      : TEXCOORD1;	// テクスチャ
	float3 N		: TEXCOORD2;	// 法線
	float3 Eye		: TEXCOORD3;	// カメラとの相対位置
	float4 Color	: COLOR0;		// Diffuse色
};

// 頂点シェーダ
DiffuseBSTexture_OUTPUT DiffuseBSTexture_VS(float4 Pos : POSITION, float4 Normal : NORMAL, float2 Tex : TEXCOORD0 )
{
	DiffuseBSTexture_OUTPUT Out = (DiffuseBSTexture_OUTPUT)0;

	// 普通にカメラの目線によるワールドビュー射影変換をする
	Out.Pos = mul( Pos, matWorldViewProj );
   
	// ライトの目線によるワールドビュー射影変換をする
	Out.ZCalcTex = mul( Pos, matLightViewProj );

	// テクスチャ座標
	Out.Tex = Tex;

	// 法線
	Out.N = mul(Normal,matWorld).xyz;

	// カメラ相対位置
	Out.Eye = Place - mul(Pos,matWorld).xyz;
	
	// Diffuse色
	float3 L = -LightDir.xyz;
	Out.Color = saturate(EgColor + max(0,DifColor * dot(normalize(Out.N),L )));
	Out.Color.a = EgColor.a;

	return Out;
}

// ピクセルシェーダ
float4 DiffuseBSTexture_PS(float4 ZCalcTex : TEXCOORD0,float2 Tex : TEXCOORD1,float3 N : TEXCOORD2,float3 Eye : TEXCOORD3,float4 Color : COLOR0) : COLOR
{
	// スペキュラ色計算
	float4 Specula = pow(max(0,dot(normalize(N),normalize(-LightDir.xyz + normalize(Eye)))),SpcColor.w);
	Specula *= SpcColor;
	Specula.w = 0.0f;
	Specula = saturate(Specula);

   // テクスチャ座標に変換
   ZCalcTex /= ZCalcTex.w;
   float2 TransTexCoord;
   TransTexCoord.x = (1.0f + ZCalcTex.x)*0.5f;
   TransTexCoord.y = (1.0f - ZCalcTex.y)*0.5f;
    float shadowVisibility = MdsShadowVisibility(ZCalcTex.xyz, TransTexCoord, N);

	{
		float comp = shadowVisibility;
		if(comp==1.0f){
			return Color * tex2D(s1,Tex) + Specula;
		}else{
			return (Color * tex2D(s1,Tex) + Specula)*comp + EgColor*tex2D(s1,Tex)*(1-comp);
		}
	}
}

technique DiffuseBSTextureTec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;
	
		VertexShader = compile vs_3_0 DiffuseBSTexture_VS();
		PixelShader  = compile ps_3_0 DiffuseBSTexture_PS();
	}
}


///////////// 分散光ありテクスチャなしスフィアマップありバッファシャドウテクニック ////////////////

// 頂点シェーダ
DiffuseBSTexture_OUTPUT DiffuseBSSphia_VS(float4 Pos : POSITION, float4 Normal : NORMAL)
{
	DiffuseBSTexture_OUTPUT Out = (DiffuseBSTexture_OUTPUT)0;

	// スフィアマップテクスチャ座標
	Out.Tex = mul(Normal,matWRotate);

	// 普通にカメラの目線によるワールドビュー射影変換をする
	Out.Pos = mul( Pos, matWorldViewProj );
   
	// ライトの目線によるワールドビュー射影変換をする
	Out.ZCalcTex = mul( Pos, matLightViewProj );

	// 法線
	Out.N = mul(Normal,matWorld).xyz;

	// カメラ相対位置
	Out.Eye = Place - mul(Pos,matWorld).xyz;
	
	// Diffuse色
	float3 L = -LightDir.xyz;
	Out.Color = saturate(EgColor + max(0,DifColor * dot(normalize(Out.N),L )));
	Out.Color.a = EgColor.a;

	return Out;
}

// ピクセルシェーダ
float4 DiffuseBSSphia_PS(float4 ZCalcTex : TEXCOORD0,float2 Tex : TEXCOORD1,float3 N : TEXCOORD2,float3 Eye : TEXCOORD3,float4 Color : COLOR0) : COLOR
{
	// スペキュラ色計算
	float4 Specula = pow(max(0,dot(normalize(N),normalize(-LightDir.xyz + normalize(Eye)))),SpcColor.w);
	Specula *= SpcColor;
	Specula.w = 0.0f;
	Specula = saturate(Specula);

	// テクスチャ座標に変換
	ZCalcTex /= ZCalcTex.w;
	float2 TransTexCoord;
	TransTexCoord.x = (1.0f + ZCalcTex.x)*0.5f;
	TransTexCoord.y = (1.0f - ZCalcTex.y)*0.5f;
    float shadowVisibility = MdsShadowVisibility(ZCalcTex.xyz, TransTexCoord, N);

	// スフィアマップ座標計算
	float2 SpTex;
	SpTex.x = Tex.x * 0.5f + 0.5f;
	SpTex.y = Tex.y * -0.5f + 0.5f;

	float4 ans;
	float4 texUV=tex2D(s1,SpTex);
	if(spadd) ans = Color + texUV;
	else	  ans = Color * texUV;

	{
		float comp = shadowVisibility;
		if(comp==1.0f){
			return ans + Specula;
		}else{
			if(spadd){
				ans = (ans + Specula)*comp + (EgColor+texUV)*(1-comp);
				ans.a = EgColor.a;
				return ans;
			}else{
				ans = (ans + Specula)*comp + (EgColor*texUV)*(1-comp);
				ans.a = EgColor.a;
				return ans;
			}
		}
	}
}

technique DiffuseBSSphiaTec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;
	
		VertexShader = compile vs_3_0 DiffuseBSSphia_VS();
		PixelShader  = compile ps_3_0 DiffuseBSSphia_PS();
	}
}

///////////// 分散光ありテクスチャありスフィアマップありバッファシャドウテクニック ////////////////

struct DiffuseBSSphiaTex_OUTPUT
{
	float4 Pos      : POSITION;		// 射影変換座標
	float4 ZCalcTex : TEXCOORD0;	// Z値
	float2 Tex      : TEXCOORD1;	// テクスチャ座標
	float2 SpTex    : TEXCOORD2;	// スフィアマップ座標
	float3 N		: TEXCOORD3;	// 法線
	float3 Eye		: TEXCOORD4;	// カメラとの相対位置
	float4 Color	: COLOR0;		// Diffuse色
};

// 頂点シェーダ
DiffuseBSSphiaTex_OUTPUT DiffuseBSSphiaTex_VS(float4 Pos : POSITION, float4 Normal : NORMAL, float2 Tex : TEXCOORD0 )
{
	DiffuseBSSphiaTex_OUTPUT Out = (DiffuseBSSphiaTex_OUTPUT)0;

	// 普通にカメラの目線によるワールドビュー射影変換をする
	Out.Pos = mul( Pos, matWorldViewProj );
   
	// ライトの目線によるワールドビュー射影変換をする
	Out.ZCalcTex = mul( Pos, matLightViewProj );

	// テクスチャ座標
	Out.Tex = Tex;

	// スフィアマップテクスチャ座標
	Out.SpTex = mul(Normal,matWRotate);

	// 法線
	Out.N = mul(Normal,matWorld).xyz;

	// カメラ相対位置
	Out.Eye = Place - mul(Pos,matWorld).xyz;
	
	// Diffuse色
	float3 L = -LightDir.xyz;
	Out.Color = saturate(EgColor + max(0,DifColor * dot(normalize(Out.N),L )));
	Out.Color.a = EgColor.a;

	return Out;
}

// ピクセルシェーダ
float4 DiffuseBSSphiaTex_PS(float4 ZCalcTex : TEXCOORD0,float2 Tex : TEXCOORD1,float2 SpTex : TEXCOORD2,float3 N : TEXCOORD3,float3 Eye : TEXCOORD4,float4 Color : COLOR0) : COLOR
{
	// スペキュラ色計算
	float4 Specula = pow(max(0,dot(normalize(N),normalize(-LightDir.xyz + normalize(Eye)))),SpcColor.w);
	Specula *= SpcColor;
	Specula.w = 0.0f;
	Specula = saturate(Specula);

   // テクスチャ座標に変換
   ZCalcTex /= ZCalcTex.w;
   float2 TransTexCoord;
   TransTexCoord.x = (1.0f + ZCalcTex.x)*0.5f;
   TransTexCoord.y = (1.0f - ZCalcTex.y)*0.5f;
    float shadowVisibility = MdsShadowVisibility(ZCalcTex.xyz, TransTexCoord, N);

	// スフィアマップ座標計算
	SpTex.x = SpTex.x * 0.5f + 0.5f;
	SpTex.y = SpTex.y * -0.5f + 0.5f;

	float4 ans;
	float4 tex1UV=tex2D(s1,Tex);
	float4 tex2UV=tex2D(s2,SpTex);
	if(spadd) ans = Color * tex1UV + tex2UV;
	else	  ans = Color * tex1UV * tex2UV;
	float answ = Color.a * tex1UV.a;

	{
		float comp = shadowVisibility;
		if(comp==1.0f){
			return ans + Specula;
		}else{
			if(spadd){
				ans = (ans + Specula)*comp + (EgColor*tex1UV+tex2UV)*(1-comp);
				ans.a = answ;
				return ans;
			}else{
				ans = (ans + Specula)*comp + (EgColor*tex1UV*tex2UV)*(1-comp);
				ans.a = answ;
				return ans;
			}
		}
	}
}

technique DiffuseBSSphiaTexTec
{
	pass P0
	{
		ColorOp[0]   = Disable;
		AlphaOp[0]   = Disable;
	
		VertexShader = compile vs_3_0 DiffuseBSSphiaTex_VS();
		PixelShader  = compile ps_3_0 DiffuseBSSphiaTex_PS();
	}
}
