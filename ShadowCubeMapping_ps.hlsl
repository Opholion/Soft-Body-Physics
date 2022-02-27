//--------------------------------------------------------------------------------------
// Per-Pixel Lighting Pixel Shader
//--------------------------------------------------------------------------------------
// Pixel shader receives position and normal from the vertex shader and uses them to calculate
// lighting per pixel. Also samples a samples a diffuse + specular texture map and combines with light colour.

#include "Common.hlsli" // Shaders can also use include files - note the extension


//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// Here we allow the shader access to a texture that has been loaded from the C++ side and stored in GPU memory.
// Note that textures are often called maps (because texture mapping describes wrapping a texture round a mesh).
// Get used to people using the word "texture" and "map" interchangably.
Texture2D DiffuseSpecularMap1 : register(t0); // Textures here can contain a diffuse map (main colour) in their rgb channels and a specular map (shininess) in the a channel
Texture2D DiffuseSpecularMap2 : register(t15);
SamplerState TexSampler       : register(s0); // A sampler is a filter for a texture like bilinear, trilinear or anisotropic - this is the sampler used for the texture above

Texture2D ShadowMapLight1 : register(t1);
Texture2D ShadowMapLight2 : register(t2); // Texture holding the view of the scene from a light
SamplerState PointClamp1   : register(s1); // No filtering for shadow maps (you might think you could use trilinear or similar, but it will filter light depths not the shadows cast...)
SamplerState PointClamp2   : register(s2);

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Pixel shader entry point - each shader has a "main" function
// This shader just samples a diffuse texture map
float4 main(LightingPixelShaderInput input) : SV_Target
{


	// Slight adjustment to calculated depth of pixels so they don't shadow themselves
	const float DepthAdjust = .00375f;

// Normal might have been scaled by model scaling or interpolation so renormalise
input.worldNormal = normalize(input.worldNormal);

///////////////////////
// Calculate lighting

// Direction from pixel to camera
float3 cameraDirection = normalize(gCameraPosition - input.worldPosition);


//----------
// LIGHT 1

float3 diffuseLight1 = 0; // Initialy assume no contribution from this light
float3 specularLight1 = 0;

float3 diffuseLight2 = 0; // Initialy assume no contribution from this light
float3 specularLight2 = 0;

float3 diffuseLight3 = 0; // Initialy assume no contribution from this light
float3 specularLight3 = 0;

// Direction from pixel to light
float3 light1Direction = normalize(gLightPosition[0] - input.worldPosition);
float3 light2Direction = normalize(gLightPosition[1] - input.worldPosition);
float3 light3Direction = normalize(gLightPosition[2] - input.worldPosition);


float3 light1Distance;
float3 light2Distance;
float3 light3Distance;





// Check if pixel is within light cone
if (dot(light1Direction, input.worldPosition - -gLightPosition[0]) > cos(gLightFacing[0].a))
{

	float4 light1ViewPosition = mul(gLightViewMatrix[0],       float4(input.worldPosition, 1.0f));
	float4 light1Projection = mul(gLightProjectionMatrix[0], light1ViewPosition);

	// Convert 2D pixel position as viewed from light into texture coordinates for shadow map - an advanced topic related to the projection step
	// Detail: 2D position x & y get perspective divide, then converted from range -1->1 to UV range 0->1. Also flip V axis
	float2 shadowMapUV1 = 0.5f * light1Projection.xy / light1Projection.w + float2(0.5f, 0.5f);
	shadowMapUV1.y = 1.0f - shadowMapUV1.y;	// Check if pixel is within light cone

	// Get depth of this pixel if it were visible from the light (another advanced projection step)
	float depthFromLight1 = light1Projection.z / light1Projection.w - DepthAdjust; //*** Adjustment so polygons don't shadow themselves

	// Compare pixel depth from light with depth held in shadow map of the light. If shadow map depth is less than something is nearer
	// to the light than this pixel - so the pixel gets no effect from this light
	if (depthFromLight1 < ShadowMapLight1.Sample(PointClamp1, shadowMapUV1).r)
	{
		float3 light1Dist = length(gLightPosition[0] - input.worldPosition);
		diffuseLight1 = gLightColour[0] * max(dot(input.worldNormal, light1Direction), 0) / light1Dist; // Equations from lighting lecture
		float3 halfway1 = normalize(light1Direction + cameraDirection);
		specularLight1 = diffuseLight1 * pow(max(dot(input.worldNormal, halfway1), 0), gSpecularPower); // Multiplying by diffuseLight instead of light colour - my own personal preference
	}
}


//CUSTOM LIGHT 2 BELOW:
if (dot(light2Direction, input.worldPosition - -gLightPosition[1]) > cos(gLightFacing[1].a))//(dot(light2Direction, input.worldPosition - -gLight2Position) > gLight2CosHalfAngle)
{
float4 light2ViewPosition = mul(gLightViewMatrix[1], float4(input.worldPosition, 1.0f));
float4 light2Projection = mul(gLightProjectionMatrix[1], light2ViewPosition);

float2 shadowMapUV2 = 0.5f * light2Projection.xy / light2Projection.w + float2(0.5f, 0.5f);
shadowMapUV2.y = 1.0f - shadowMapUV2.y;

float depthFromLight2 = (light2Projection.z / light2Projection.w - DepthAdjust) * 1.001f; //*** Adjustment so polygons don't shadow themselves

if (depthFromLight2 < ShadowMapLight2.Sample(PointClamp2, shadowMapUV2).r)
{
	float3 light2Dist = length(gLightPosition[1] - input.worldPosition);
	diffuseLight2 = gLightColour[1] / 100; // * max(dot(input.worldNormal, light2Direction), 0) / light2Dist; // Equations from lighting lecture
	float3 halfway2 = normalize(light2Direction + cameraDirection);
	specularLight2 = diffuseLight2 * pow(max(dot(input.worldNormal, halfway2), 0), gSpecularPower); // Multiplying by diffuseLight instead of light colour - my own personal preference

}
}


//Personal Testing


	if (saturate(dot(input.worldNormal, gLightDirection)))
	{
		diffuseLight3 = gLightColour[2] * max(dot(input.worldNormal, light3Direction), 0.15); // Equations from lighting lecture
		float3 halfway3 = normalize(light3Direction + cameraDirection);
		specularLight3 = diffuseLight3 * pow(max(dot(input.worldNormal, halfway3), 0), gSpecularPower); // Multiplying by diffuseLight instead of light colour - my own personal preference
	}


	// Sum the effect of the lights - add the ambient at this stage rather than for each light (or we will get too much ambient)
	float3 diffuseLight = (gAmbientColour + diffuseLight1 + diffuseLight2 + diffuseLight3);
	float3 specularLight = specularLight1 + specularLight2 + specularLight3;

	specularLight = min(specularLight, .5f);
	////////////////////
	// Combine lighting and textures

	// Sample diffuse material and specular material colour for this pixel from a texture using a given sampler that you set up in the C++ code
	float4 textureColour = DiffuseSpecularMap1.Sample(TexSampler, input.uvw);
	float3 diffuseMaterialColour = textureColour.rgb; //float3(0.8f, .05f, .05f) *  // Diffuse material colour in texture RGB (base colour of model)
	float specularMaterialColour = textureColour.a;   // Specular material colour in texture A (shininess of the surface)


	// Combine lighting with texture colours
	float3 finalColour = diffuseLight * diffuseMaterialColour + specularLight * specularMaterialColour;


	return float4(finalColour,1);// float4(finalColour, 1.0f); // Always use 1.0f for output alpha - no alpha blending in this lab
}