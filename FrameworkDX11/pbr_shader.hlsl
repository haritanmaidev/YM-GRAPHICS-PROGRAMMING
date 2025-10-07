//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
cbuffer ConstantBuffer : register( b0 )
{
	matrix World;
	matrix View;
	matrix Projection;
	float4 vOutputColor;
}

Texture2D albedoMap : register(t0);
Texture2D roughnessMap : register(t1);
SamplerState samLinear : register(s0);

static const float PI = 3.14159265f;

#define MAX_LIGHTS 1
// Light types.
#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct Light
{
	float4      Position;               // 16 bytes
										//----------------------------------- (16 byte boundary)
	float4      Direction;              // 16 bytes
										//----------------------------------- (16 byte boundary)
	float4      Color;                  // 16 bytes
										//----------------------------------- (16 byte boundary)
	float       SpotAngle;              // 4 bytes
	float       ConstantAttenuation;    // 4 bytes
	float       LinearAttenuation;      // 4 bytes
	float       QuadraticAttenuation;   // 4 bytes
										//----------------------------------- (16 byte boundary)
	int         LightType;              // 4 bytes
	bool        Enabled;                // 4 bytes
	int2        Padding;                // 8 bytes
										//----------------------------------- (16 byte boundary)
};  // Total:                           // 80 bytes (5 * 16)

cbuffer LightProperties : register(b1)
{
	float4 EyePosition;                 // 16 bytes
										//----------------------------------- (16 byte boundary)
	float4 GlobalAmbient;               // 16 bytes
										//----------------------------------- (16 byte boundary)
	Light Lights[MAX_LIGHTS];           // 80 * 8 = 640 bytes
}; 


//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Norm : NORMAL;
    float4 Tangent : TANGENT;
    float2 Tex : TEXCOORD0;
    uint4 Joints : BLENDINDICES0;
    float4 Weights : BLENDWEIGHT0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
	float4 worldPos : POSITION;
	float3 Norm : NORMAL;
	float2 Tex : TEXCOORD0;
};


float4 DoDiffuse(Light light, float3 L, float3 N)
{
	float NdotL = max(0, dot(N, L));
	return light.Color * NdotL;
}

float4 DoSpecular(Light lightObject, float3 pixelToEyeVectorNormalised, float3 pixelToLightVectorNormalised, float3 Normal)
{
    float lightIntensity = saturate(dot(Normal, pixelToLightVectorNormalised));
	float4 specular = float4(0, 0, 0, 0);
	if (lightIntensity > 0.0f)
	{
		// note the reflection equation requires the *light to pixel* vector - hence we reverse it
        float3 reflection = reflect(-pixelToLightVectorNormalised, Normal);
        specular = pow(saturate(dot(reflection, pixelToEyeVectorNormalised)), 4); // 32 = specular power Material.SpecularPower
    }
	
    //return specular * 200;
	return specular;
}

float DoAttenuation(Light light, float d)
{
	return 1.0f / (light.ConstantAttenuation + light.LinearAttenuation * d + light.QuadraticAttenuation * d * d);
}

struct LightingResult
{
	float4 Diffuse;
	float4 Specular;
};

LightingResult DoPointLight(Light light, float3 pixelToLightVectorNormalised, float3 pixelToEyeVectorNormalised, float distanceFromPixelToLight, float3 N)
{
	LightingResult result;

    float attenuation = DoAttenuation(light, distanceFromPixelToLight);
    attenuation = 1;

    result.Diffuse = DoDiffuse(light, pixelToLightVectorNormalised, N) * attenuation;
    result.Specular = DoSpecular(light, pixelToEyeVectorNormalised, pixelToLightVectorNormalised, N) * attenuation; 

	return result;
}

LightingResult ComputeLighting(float4 pixelToLightVectorNormalised, float4 pixelToEyeVectorNormalised, float distanceFromPixelToLight, float3 N)
{
	LightingResult totalResult = { { 0, 0, 0, 0 },{ 0, 0, 0, 0 } };

	[unroll]
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		LightingResult result = { { 0, 0, 0, 0 },{ 0, 0, 0, 0 } };

		if (!Lights[i].Enabled) 
			continue;
		
        result = DoPointLight(Lights[i], pixelToLightVectorNormalised.xyz, pixelToEyeVectorNormalised.xyz, distanceFromPixelToLight, N);
		
		totalResult.Diffuse += result.Diffuse;
		totalResult.Specular += result.Specular;
	}

	totalResult.Diffuse = saturate(totalResult.Diffuse);
	totalResult.Specular = saturate(totalResult.Specular);

	return totalResult;
}

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS( VS_INPUT input )
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul( input.Pos, World );
	output.worldPos = output.Pos;
    output.Pos = mul( output.Pos, View );
    output.Pos = mul( output.Pos, Projection );

	// multiply the normal by the world transform (to go from model space to world space)
	output.Norm = mul(float4(input.Norm, 0), World).xyz;

	output.Tex = input.Tex;
    
    return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

//Normal Distribution Function
float CalculateNDF(float roughness, float3 N, float3 H)
{
    float roughnessSquare = roughness * roughness;
    return roughnessSquare / (PI * pow(pow(dot(N, H), 2) * (roughnessSquare - 1) + 1, 2));
}

float G_sub(float3 N, float3 V, float k)
{
    return dot(N, V) / (dot(N, V) * (1 - k) + k);
}

float4 PS_PBR(PS_INPUT IN) : SV_TARGET
{
    float4 finalColour = float4(1, 0, 0, 1);
	
    float3 albedo = float3(0.3, 0.3, 0.3);
    float metallic = 1.0f;
    float roughness = 0.5f;
	
	//STEP 1: Calculate vectors
    float3 N = IN.Norm;
    float3 V = normalize(EyePosition - IN.worldPos); // View Direction
    float3 L = normalize(Lights[0].Position - IN.worldPos); // Light Direction
    float3 H = normalize(V + L); // Halfway Vector
    float cosTheta = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
	
	//STEP 2: Fresnel
	//Calculating Fresnel F0 where F0 = Constant in Dielectrics. For metals, its albedo.
	//Calculating  Fresnel-Schlick's Approximation Function where F = Fsch 
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);
    float3 F = F0 + (1 - F0) * pow(1 - cosTheta, 5); 
    //return float4(F, 1.0);
	
	//STEP 3: Specular BRDF
    float D = CalculateNDF(roughness, N, H); //Normal Distribution Function
    //return float4(NDF, NDF, NDF, 1.0);
	
    float k = pow(roughness + 1, 2) / 8; //Geometry Function v
    float G = G_sub(L, N, roughness) * G_sub(V, N, roughness);
    //return float4(G, G, G, 1.0);
	
    float3 SpecularBRDF = D * G * F / (4 * dot(N, L) * dot(N, V)); //Calculate Specular BRDF
    return float4(SpecularBRDF, 1.0);
	
	//STEP 4: Diffuse
    float3 kD = float3(1, 1, 1) - F * (1 - metallic); //Diffuse Colour based on Fresnel term & Metallic Value
    float3 diffuse = kD * albedo / PI;
	
    return float4(diffuse, 1.0);
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

float4 PS_Normal(PS_INPUT IN) : SV_TARGET
{
    float4 pixelToLightVectorNormalised = normalize(Lights[0].Position - IN.worldPos);
    float4 pixelToEyeVectorNormalised = normalize(EyePosition - IN.worldPos);
    float distanceFromPixelToLight = length(IN.worldPos - Lights[0].Position);
    LightingResult lit = ComputeLighting(pixelToLightVectorNormalised, pixelToEyeVectorNormalised, distanceFromPixelToLight, normalize(IN.Norm));

	// NOTE we aren't using any material properties just yet. 
    float4 ambient = GlobalAmbient;  
	float4 diffuse = lit.Diffuse;
	float4 specular = lit.Specular;
	
    //float2 mirroredCoords = float2(1 - IN.Tex.x, IN.Tex.y);
    float4 texColor = albedoMap.Sample(samLinear, IN.Tex);
    float4 texColor2 = roughnessMap.Sample(samLinear, IN.Tex);
    texColor = lerp(texColor, texColor2, 0.25);
	
    float4 diffuseColor = (ambient + diffuse + specular) * texColor;
    //return diffuseColor;	
	
	//TOON EFFECT 2 - View Based
    float diffuseIntensity2 = dot(pixelToEyeVectorNormalised.xyz, normalize(IN.Norm));
    if (diffuseIntensity2 > 0.8f) diffuseIntensity2 = 1.0f; // Brightest band
    else if (diffuseIntensity2 > 0.4f) diffuseIntensity2 = 0.6f; // Mid-tone band
    else diffuseIntensity2 = 0.2f; // Darkest band
	
    float4 finalColor = (ambient + (Lights[0].Color * diffuseIntensity2)) * float4(texColor.rgb, 1.0);
	
	//TOON EFFECT - Light Based
    //float diffuseIntensity = saturate(dot(Lights[0].Direction, diffuse));
    //if (diffuseIntensity > 0.8f) diffuseIntensity = 1.0f; // Brightest band
    //else if (diffuseIntensity > 0.4f) diffuseIntensity = 0.6f; // Mid-tone band
    //else diffuseIntensity = 0.2f; // Darkest band
    //float4 finalColor = (ambient + (Lights[0].Color * diffuseIntensity)) * float4(texColor.rgb, 1.0);
    return finalColor;
}


//--------------------------------------------------------------------------------------
// PSSolid - render a solid color
//--------------------------------------------------------------------------------------
float4 PSSolid(PS_INPUT input) : SV_Target
{
	return vOutputColor;
}
